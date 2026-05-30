#include "PersistDisplay.h"

#include <string>
#include <cstring>

namespace PersistDisplay {

namespace {

constexpr LONG kStateEmpty = 0;
constexpr LONG kStateWriting = 1;
constexpr LONG kStateReady = 2;
constexpr size_t kMaxBytes = static_cast<size_t>(kMaxWidth) * static_cast<size_t>(kMaxHeight) * 4u;

struct SharedFrame {
	volatile LONG state;
	LONG width;
	LONG height;
	LONG rowBytes;
	uint8_t pixels[kMaxBytes];
};

HANDLE g_hMapObject = nullptr;
SharedFrame* g_sharedFrame = nullptr;

inline bool IsDimensionValid(LONG value, uint32_t maxValue)
{
	return value > 0 && static_cast<uint32_t>(value) <= maxValue;
}

} // namespace

bool Init(long long sessionID)
{
	if (g_hMapObject != nullptr && g_sharedFrame != nullptr) {
		return true;
	}

	if (sessionID == 0) {
		return false;
	}

	const std::string mapName = "Local\\PersistDisplay-s" + std::to_string(sessionID);
	g_hMapObject = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		nullptr,
		PAGE_READWRITE,
		0,
		static_cast<DWORD>(sizeof(SharedFrame)),
		mapName.c_str());
	if (g_hMapObject == nullptr) {
		return false;
	}

	const bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
	g_sharedFrame = static_cast<SharedFrame*>(MapViewOfFile(g_hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedFrame)));
	if (g_sharedFrame == nullptr) {
		CloseHandle(g_hMapObject);
		g_hMapObject = nullptr;
		return false;
	}

	if (!alreadyExists) {
		std::memset(g_sharedFrame, 0, sizeof(SharedFrame));
	}

	return true;
}

void Cleanup()
{
	if (g_sharedFrame != nullptr) {
		UnmapViewOfFile(g_sharedFrame);
		g_sharedFrame = nullptr;
	}
	if (g_hMapObject != nullptr) {
		CloseHandle(g_hMapObject);
		g_hMapObject = nullptr;
	}
}

void Clear()
{
	if (g_sharedFrame == nullptr) {
		return;
	}

	g_sharedFrame->width = 0;
	g_sharedFrame->height = 0;
	g_sharedFrame->rowBytes = 0;
	MemoryBarrier();
	g_sharedFrame->state = kStateEmpty;
}

bool HasFrame()
{
	if (g_sharedFrame == nullptr || g_sharedFrame->state != kStateReady) {
		return false;
	}

	return IsDimensionValid(g_sharedFrame->width, kMaxWidth)
		&& IsDimensionValid(g_sharedFrame->height, kMaxHeight)
		&& g_sharedFrame->rowBytes >= g_sharedFrame->width * 4;
}

bool StoreBgraFrame(const void* pixels, uint32_t width, uint32_t height, uint32_t strideBytes)
{
	if (g_sharedFrame == nullptr || pixels == nullptr) {
		return false;
	}

	const uint32_t rowBytes = width * 4;
	if (width == 0 || height == 0 || width > kMaxWidth || height > kMaxHeight || strideBytes < rowBytes) {
		Clear();
		return false;
	}

	g_sharedFrame->state = kStateWriting;
	g_sharedFrame->width = static_cast<LONG>(width);
	g_sharedFrame->height = static_cast<LONG>(height);
	g_sharedFrame->rowBytes = static_cast<LONG>(rowBytes);

	const auto* src = static_cast<const uint8_t*>(pixels);
	auto* dst = g_sharedFrame->pixels;
	for (uint32_t row = 0; row < height; ++row) {
		std::memcpy(dst, src, rowBytes);
		src += strideBytes;
		dst += rowBytes;
	}

	MemoryBarrier();
	g_sharedFrame->state = kStateReady;
	return true;
}

bool Paint(HDC hdc, const RECT& destRect)
{
	if (hdc == nullptr || !HasFrame()) {
		return false;
	}

	BITMAPINFO bitmapInfo = {};
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = g_sharedFrame->width;
	bitmapInfo.bmiHeader.biHeight = -g_sharedFrame->height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;

	SetStretchBltMode(hdc, COLORONCOLOR);
	const int result = StretchDIBits(
		hdc,
		destRect.left,
		destRect.top,
		destRect.right - destRect.left,
		destRect.bottom - destRect.top,
		0,
		0,
		g_sharedFrame->width,
		g_sharedFrame->height,
		g_sharedFrame->pixels,
		&bitmapInfo,
		DIB_RGB_COLORS,
		SRCCOPY);

	return result != GDI_ERROR;
}

} // namespace PersistDisplay
