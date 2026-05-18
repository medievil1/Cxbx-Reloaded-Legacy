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
// *  (c) 2018 RadWolfie
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef IPC_HYBRID_HPP
#define IPC_HYBRID_HPP

// NOTE: Keep in mind, IPC handling will be temporary until migrate kernel process into a thread.
//       Plus using this method will allow support for cross-platform if proper virtualize emulation
//       isn't there yet.

// ******************************************************************
// WM_COPYDATA payload sent from the emu process to the GUI parent
// just before a reboot so the GUI can freeze on the last rendered
// frame instead of flashing the Cxbx splash screen.
// ******************************************************************
#define CXBXR_COPYDATA_LASTFRAME 0x43584246UL  // 'CXBF'

#pragma pack(push, 1)
struct CxbxLastFrameHeader {
    DWORD Width;    ///< frame width in pixels
    DWORD Height;   ///< frame height in pixels
    DWORD Pitch;    ///< bytes per row
    DWORD Format;   ///< Xbox X_D3DFORMAT value
    // Immediately followed by Pitch * Height bytes of pixel data
};
#pragma pack(pop)

// ******************************************************************
// For kernel process use only
// ******************************************************************

typedef enum class _IPC_UPDATE_GUI {
	  LLE_FLAGS = 0
	, XBOX_LED_COLOUR
	, LOG_ENABLED
	, KRNL_IS_READY
	, OVERLAY
	, WINDOW_HANDLE
	, WINDOW_DESTROYED
} IPC_UPDATE_GUI;

void ipc_send_gui_update(IPC_UPDATE_GUI command, const unsigned int value);


// ******************************************************************
// For GUI process use only
// ******************************************************************

typedef enum class _IPC_UPDATE_KERNEL {
	CONFIG_LOGGING_SYNC = 0,
	CONFIG_INPUT_SYNC,
	CONFIG_CHANGE_TIME
} IPC_UPDATE_KERNEL;

void ipc_send_kernel_update(IPC_UPDATE_KERNEL command, const int value, const unsigned int hwnd);

#endif
