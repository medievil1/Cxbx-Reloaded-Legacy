// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
// *  This file is heavily based on code from XQEMU
// *  https://github.com/xqemu/xqemu/blob/master/hw/xbox/nv2a/nv2a_user.c
// *  Copyright (c) 2012 espes
// *  Copyright (c) 2015 Jannik Vogel
// *  Copyright (c) 2018 Matt Borgerson
// *
// *  Contributions for Cxbx-Reloaded
// *  Copyright (c) 2017-2018 Luke Usher <luke.usher@outlook.com>
// *  Copyright (c) 2018 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************

/* USER - PFIFO MMIO and DMA submission area */
DEVICE_READ32(USER)
{
	unsigned int channel_id = addr >> 16;
	assert(channel_id < NV2A_NUM_CHANNELS);

	if ((addr & 0xFFFF) == NV_USER_DMA_GET) {
		uint32_t get_v = d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_GET)];
		uint32_t put_v = d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_PUT)];
		if (get_v != put_v) {
	if (d->enable_overlay) {
		LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
		uint32_t channel_modes = d->pfifo.regs[RI(NV_PFIFO_MODE)];
		if (channel_modes & (1 << channel_id)) {
			unsigned int cur_channel_id =
				GET_MASK(d->pfifo.regs[RI(NV_PFIFO_CACHE1_PUSH1)],
					NV_PFIFO_CACHE1_PUSH1_CHID);
			if (channel_id == cur_channel_id) {
				switch (addr & 0xFFFF) {
				case NV_USER_DMA_PUT:
					d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_PUT)] = value;
					break;
				case NV_USER_DMA_GET:
					d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_GET)] = value;
					break;
				case NV_USER_REF:
					d->pfifo.regs[RI(NV_PFIFO_CACHE1_REF)] = value;
					break;
				default: break;
				}
			}
		}
		SetEvent(d->pfifo.puller_event);
		{ LARGE_INTEGER t1; QueryPerformanceCounter(&t1); 
		  double us = (double)(t1.QuadPart - t0.QuadPart); 
		  extern void DumpUserTime(const char*, LARGE_INTEGER*);
		  DumpUserTime("WR-overlay", &t0); }
		DEVICE_WRITE32_END(USER);
	}
			}
		}
		uint32_t result = get_v;
		DEVICE_READ32_END(USER);
	}

	if ((addr & 0xFFFF) == NV_USER_REF) {
		uint32_t get_v = d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_GET)];
		uint32_t put_v = d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_PUT)];
		if (get_v != put_v && !d->enable_overlay) {
			pfifo_flush_to_pgraph(d);
		}
		uint32_t result = d->pfifo.regs[RI(NV_PFIFO_CACHE1_REF)];
		DEVICE_READ32_END(USER);
	}

	qemu_mutex_lock(&d->pfifo.pfifo_lock);

	uint32_t channel_modes = d->pfifo.regs[RI(NV_PFIFO_MODE)];
	uint32_t result = 0;
	if (channel_modes & (1 << channel_id)) {
		unsigned int cur_channel_id =
			GET_MASK(d->pfifo.regs[RI(NV_PFIFO_CACHE1_PUSH1)],
				NV_PFIFO_CACHE1_PUSH1_CHID);
		if (channel_id == cur_channel_id) {
			switch (addr & 0xFFFF) {
			case NV_USER_DMA_PUT:
				result = d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_PUT)];
				break;
			case NV_USER_DMA_GET:
				result = d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_GET)];
				break;
			case NV_USER_REF:
				result = d->pfifo.regs[RI(NV_PFIFO_CACHE1_REF)];
				break;
			default:
				assert(false);
				break;
			}
		} else {
			assert(false);
		}
	} else {
		assert(false);
	}

	qemu_mutex_unlock(&d->pfifo.pfifo_lock);
	DEVICE_READ32_END(USER);
}

DEVICE_WRITE32(USER)
{
	unsigned int channel_id = addr >> 16;
	assert(channel_id < NV2A_NUM_CHANNELS);

	// The pfifo_lock was held here to serialize PFIFO register access between
	// the game thread and the puller thread.  Holding pfifo_lock while the
	// puller also holds it creates a lock-ordering deadlock when both threads
	// then need pgraph_lock.  32-bit aligned register reads/writes are atomic
	// on x86, so the lock is unnecessary for correctness.  The puller sees
	// either the old or new value of DMA_GET/PUT — never a torn write.
	// We still call pfifo_run_pusher (non-overlay) and SetEvent to keep D3D
	// pushbuffer processing and overlay compositing progressing.

	uint32_t channel_modes = d->pfifo.regs[RI(NV_PFIFO_MODE)];
	if (channel_modes & (1 << channel_id)) {
		unsigned int cur_channel_id =
			GET_MASK(d->pfifo.regs[RI(NV_PFIFO_CACHE1_PUSH1)],
				NV_PFIFO_CACHE1_PUSH1_CHID);

		if (channel_id == cur_channel_id) {
			switch (addr & 0xFFFF) {
			case NV_USER_DMA_PUT: {
				d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_PUT)] = value;
				// During overlay (video), skip inline pushbuffer processing.
				// pfifo_run_pusher holds pgraph_lock while processing DMA
				// commands; the puller's pfifo_run_puller uses TryEnter on
				// pgraph_lock and yields if contended.  Processing inline
				// would hold pgraph_lock for 38ms, starving the puller and
				// preventing flip_stall overlay compositing entirely.
				if (!d->enable_overlay) {
					uint32_t push0    = d->pfifo.regs[RI(NV_PFIFO_CACHE1_PUSH0)];
					uint32_t dma_push = d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_PUSH)];
					bool pusher_can_run = GET_MASK(push0, NV_PFIFO_CACHE1_PUSH0_ACCESS)
					                   && GET_MASK(dma_push, NV_PFIFO_CACHE1_DMA_PUSH_ACCESS)
					                   && !GET_MASK(dma_push, NV_PFIFO_CACHE1_DMA_PUSH_STATUS);
					if (pusher_can_run) {
						CxbxSetPullerContext(true);
						pfifo_run_pusher(d);
						CxbxSetPullerContext(false);
					}
				}
				break;
			}
			case NV_USER_DMA_GET:
				d->pfifo.regs[RI(NV_PFIFO_CACHE1_DMA_GET)] = value;
				break;
			case NV_USER_REF:
				d->pfifo.regs[RI(NV_PFIFO_CACHE1_REF)] = value;
				break;
			default:
				assert(false);
				break;
			}

			SetEvent(d->pfifo.puller_event);
		} else {
			/* ramfc */
			assert(false);
		}
	} else {
		/* PIO Mode */
		assert(false);
	}

	DEVICE_WRITE32_END(USER);
}
