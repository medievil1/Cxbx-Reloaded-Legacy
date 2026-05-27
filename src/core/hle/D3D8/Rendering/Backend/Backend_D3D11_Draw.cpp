// ******************************************************************
// *
// *  This file is part of the Cxbx project.
// *
// *  Cxbx and Cxbe are free software; you can redistribute them
// *  and/or modify them under the terms of the GNU General Public
// *  License as published by the Free Software Foundation; either
// *  version 2 of the license, or (at your option) any later version.
// *
// *  This program is distributed in the hope that it will be useful,
// *  but WITHOUT ANY WARRANTY; without even the implied warranty of
// *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// *  GNU General Public License for more details.
// *
// *  You should have recieved a copy of the GNU General Public License
// *  along with this program; see the file COPYING.
// *  If not, write to the Free Software Foundation, Inc.,
// *  59 Temple Place - Suite 330, Bostom, MA 02111-1307, USA.
// *
// *  All rights reserved
// *
// ******************************************************************

#include "Backend_D3D11_Internal.h"

// ******************************************************************
// * Rendering helpers (D3D11 implementations)
// ******************************************************************

void CxbxD3DClear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	LOG_INIT;
	FLOAT clearColor[4];
	clearColor[0] = ((Color >> 16) & 0xFF) / 255.0f;
	clearColor[1] = ((Color >>  8) & 0xFF) / 255.0f;
	clearColor[2] = ((Color >>  0) & 0xFF) / 255.0f;
	clearColor[3] = ((Color >> 24) & 0xFF) / 255.0f;

	if ((Flags & D3DCLEAR_TARGET) && g_pD3DCurrentRTV != nullptr) {
		if (Count > 0 && pRects != nullptr) {
			ComPtr<ID3D11DeviceContext1> context1;
			if (SUCCEEDED(g_pD3DDeviceContext->QueryInterface(IID_PPV_ARGS(context1.GetAddressOf())))) {
				context1->ClearView(g_pD3DCurrentRTV, clearColor, (const D3D11_RECT*)pRects, Count);
			}
		} else {
			g_pD3DDeviceContext->ClearRenderTargetView(g_pD3DCurrentRTV, clearColor);
		}
	}

	if (g_pD3DDepthStencilView != nullptr) {
		UINT clearFlags = 0;
		if (Flags & D3DCLEAR_ZBUFFER) clearFlags |= D3D11_CLEAR_DEPTH;
		if (Flags & D3DCLEAR_STENCIL) clearFlags |= D3D11_CLEAR_STENCIL;
		if (clearFlags != 0) {
			g_pD3DDeviceContext->ClearDepthStencilView(g_pD3DDepthStencilView, clearFlags, Z, (UINT8)Stencil);
		}
	}
}

HRESULT CxbxBltSurface(ID3D11Texture2D* pSrc, const RECT* pSrcRect, ID3D11Texture2D* pDst, const RECT* pDstRect, D3DTEXTUREFILTERTYPE Filter)
{
	return CxbxD3D11Blt(pSrc, pSrcRect, pDst, pDstRect, Filter);
}

HRESULT CxbxBltSurfaceYUY2(ID3D11Texture2D* pSrcYUY2, UINT srcPixelWidth, const RECT* pSrcRect, ID3D11Texture2D* pDst, const RECT* pDstRect)
{
	if (!g_pD3D11BlitYUY2PS || !g_pD3D11BlitVS) {
		return E_FAIL;
	}

	D3D11_TEXTURE2D_DESC dstDesc;
	pDst->GetDesc(&dstDesc);

	UINT dstX = pDstRect ? pDstRect->left : 0;
	UINT dstY = pDstRect ? pDstRect->top : 0;
	UINT dstW = pDstRect ? (pDstRect->right - pDstRect->left) : dstDesc.Width;
	UINT dstH = pDstRect ? (pDstRect->bottom - pDstRect->top) : dstDesc.Height;

	// Create SRV for the YUY2 source texture (R8G8_B8G8_UNORM)
	ID3D11ShaderResourceView* pSRV = nullptr;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8_B8G8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	HRESULT hr = g_pD3DDevice->CreateShaderResourceView(pSrcYUY2, &srvDesc, &pSRV);
	if (FAILED(hr)) return hr;

	// Create temporary RTV for destination
	ID3D11RenderTargetView* pRTV = nullptr;
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = dstDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	hr = g_pD3DDevice->CreateRenderTargetView(pDst, &rtvDesc, &pRTV);
	if (FAILED(hr)) { pSRV->Release(); return hr; }

	// Save current pipeline state
	ID3D11RenderTargetView* pOldRTV = nullptr;
	ID3D11DepthStencilView* pOldDSV = nullptr;
	g_pD3DDeviceContext->OMGetRenderTargets(1, &pOldRTV, &pOldDSV);
	D3D11_VIEWPORT oldVP; UINT numVP = 1;
	g_pD3DDeviceContext->RSGetViewports(&numVP, &oldVP);
	ComPtr<ID3D11BlendState> pOldBlendState;
	FLOAT oldBlendFactor[4]; UINT oldSampleMask;
	g_pD3DDeviceContext->OMGetBlendState(&pOldBlendState, oldBlendFactor, &oldSampleMask);
	ComPtr<ID3D11RasterizerState> pOldRasterizerState;
	g_pD3DDeviceContext->RSGetState(&pOldRasterizerState);

	// Set YUY2 blit pipeline
	D3D11_VIEWPORT vp = { (FLOAT)dstX, (FLOAT)dstY, (FLOAT)dstW, (FLOAT)dstH, 0.0f, 1.0f };
	g_pD3DDeviceContext->RSSetViewports(1, &vp);
	g_pD3DDeviceContext->RSSetState(nullptr);
	g_pD3DDeviceContext->OMSetRenderTargets(1, &pRTV, nullptr);
	g_pD3DDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	g_pD3DDeviceContext->VSSetShader(g_pD3D11BlitVS, nullptr, 0);
	g_pD3DDeviceContext->PSSetShader(g_pD3D11BlitYUY2PS, nullptr, 0);
	g_pD3DDeviceContext->GSSetShader(nullptr, nullptr, 0);
	g_pD3DDeviceContext->PSSetShaderResources(0, 1, &pSRV);
	// Use point sampler for accurate byte-level sampling of YUY2 macroblocks
	g_pD3DDeviceContext->PSSetSamplers(0, 1, &g_pD3D11BlitSamplerPoint);
	g_pD3DDeviceContext->IASetInputLayout(nullptr);
	g_pD3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	g_pD3DDeviceContext->Draw(3, 0);

	// Restore state
	g_pD3DDeviceContext->OMSetRenderTargets(1, &pOldRTV, pOldDSV);
	g_pD3DDeviceContext->RSSetViewports(1, &oldVP);
	g_pD3DDeviceContext->RSSetState(pOldRasterizerState.Get());
	g_pD3DDeviceContext->OMSetBlendState(pOldBlendState.Get(), oldBlendFactor, oldSampleMask);
	if (pOldRTV) pOldRTV->Release();
	if (pOldDSV) pOldDSV->Release();

	ID3D11ShaderResourceView* pNullSRV = nullptr;
	g_pD3DDeviceContext->PSSetShaderResources(0, 1, &pNullSRV);

	extern void CxbxInvalidateTextureStateCache();
	CxbxInvalidateTextureStateCache();
	extern void CxbxInvalidateActivePixelShader();
	CxbxInvalidateActivePixelShader();
	extern void CxbxInvalidateVertexShaderCache();
	CxbxInvalidateVertexShaderCache();

	pRTV->Release();
	pSRV->Release();

	return S_OK;
}

// ******************************************************************
// * Dual-backend wrappers — D3D11 implementations
// ******************************************************************

static ID3D11VertexShader* s_LastBoundVS = nullptr;

HRESULT CxbxSetVertexShader(ID3D11VertexShader* pHostVertexShader)
{
	if (pHostVertexShader != s_LastBoundVS) {
		g_pD3DDeviceContext->VSSetShader(pHostVertexShader, nullptr, 0);
		s_LastBoundVS = pHostVertexShader;
	}
	return S_OK;
}

void CxbxInvalidateVertexShaderCache()
{
	s_LastBoundVS = nullptr;
	// Also invalidate topology — blit sets TRIANGLELIST directly
	extern void CxbxInvalidateTopologyCache();
	CxbxInvalidateTopologyCache();
	// Also invalidate GS — blit sets GS=nullptr directly
	extern void CxbxInvalidateGSCache();
	CxbxInvalidateGSCache();
}
