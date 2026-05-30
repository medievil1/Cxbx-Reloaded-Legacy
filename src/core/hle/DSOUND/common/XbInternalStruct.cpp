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
// *  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// *
// *  (c) 2020 RadWolfie
// *
// *  All rights reserved
// *
// ******************************************************************

#include "XbInternalStruct.hpp"

xbox::CUnknownTemplate::CUnknownTemplate() { ref_count = 1; }

xbox::CMcpxVoiceClient::_settings xbox::CMcpxVoiceClient::default_settings =
{
    {},                             // Unknown2_pre (0x08 - 0x30)
    0,                              // dwBufferAllocSize (0x30)
    0,                              // pPlayCursor (0x34)
    {},                             // Unknown2_post (0x38 - 0x300)
};
