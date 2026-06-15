# QEMU Dependency Removal Summary

This document summarizes the complete removal of QEMU-derived code from the Cxbx-Reloaded
NV2A device emulation layer. The original NV2A codebase was ported from XQEMU (a QEMU-based
Xbox emulator) and carried extensive QEMU infrastructure that was unnecessary for a
Windows-native emulator.

## Phases

### Phase 1: Synchronization Primitives

**Replaced:** `QemuMutex` (CRITICAL_SECTION wrapper) + `QemuCond` (hand-rolled semaphore+event)
→ `HostMutex` (SRWLOCK) + `HostCond` (CONDITION_VARIABLE)

**Files deleted:**
- `src/devices/video/qemu-thread-win32.cpp` (427 lines)
- `src/devices/video/thread-win32.h` (33 lines)
- `src/devices/video/qemu-thread.h` (85 lines)

**New file created:**
- `src/devices/video/host_sync.h` (~50 lines of inline wrappers)

**Benefit:** `SRWLOCK` + `CONDITION_VARIABLE` operate entirely in user mode in the
uncontended case, eliminating kernel transitions on every signal/wait pair.

### Phase 2: Dead Code & GLib Dependencies

**Files deleted:**
- `src/devices/video/g-lru-cache.c` (368 lines — required GLib runtime, never compiled)
- `src/devices/video/g-lru-cache.h` (97 lines)

**Cleaned:**
- Removed `#undef USE_SHADER_CACHE` and dead GLib include path from `nv2a_int.h`
- Removed GLib type compatibility typedefs (`gchar`, `gint`, `gboolean`, `GError`, etc.)
  from `nv2a.cpp` (19 lines)

### Phase 3: QEMU Queue Macros

**Files deleted:**
- `src/devices/video/queue.h` (414 lines — QEMU-adapted NetBSD sys/queue.h)

**Replaced with `std::list`:**
- `USBEndpoint::Queue`: `QTAILQ_HEAD(, USBPacket)` → `std::list<USBPacket*>`
- `USBPacket::Queue` entry: `QTAILQ_ENTRY(USBPacket)` → removed
- `XboxDeviceState::Strings`: `QLIST_HEAD(, USBDescString)` → `std::list<USBDescString>`
- `USBDescString::next`: `QLIST_ENTRY(USBDescString)` → removed
- All `QTAILQ_*` / `QLIST_*` operations replaced with `std::list` methods
- `QLIST_INIT` calls removed (`std::list` default-constructs empty)
- `QLIST_INSERT_HEAD` with `new` → `push_front` by value (eliminates per-string `new`/`delete`)

### Phase 4: PFIFO DMA Engine Restructuring

**Removed background pusher thread:**
- `pfifo_pusher_thread()` function (42 lines) and its `std::thread` launch/join
- `pusher_cond`, `flush_complete_cond`, `flush_requested` fields from `NV2AState`

**Behaviour change:**
- `NV_USER_DMA_PUT` always processes pushbuffer commands inline on the calling thread
  (previous overlay path used `host_cond_signal` to wake background pusher)
- `pfifo_run_pusher()` is always called synchronously — no OS thread context switch overhead
- Puller thread retained for CACHE1 draining, auto-present fallback, overlay compositing

### Phase 5: PGRAPH Lock Granularity

**Added shared-read lock support:**
- `host_mutex_lock_shared` / `host_mutex_unlock_shared` for `AcquireSRWLockShared`

**Changed read paths to shared locking:**
- `DEVICE_READ32(PGRAPH)`: exclusive → shared (MMIO reads no longer block each other)
- `CxbxUpdateNativeD3DResources` (HLE render thread): exclusive → shared (HLE reads don't
  block MMIO reads)

**Lock-free dirty counters:**
- `uint32_t dirty[NV2A_DIRTY_COUNT]` → `std::atomic<uint32_t> dirty[NV2A_DIRTY_COUNT]`
- Safe for concurrent read under shared lock

### Phase 6: MMIO Dispatch & IRQ Simplification

**O(1) MMIO dispatch:**
- Replaced `EmuNV2A_Block()` O(n) linear search with `s_mmioPageTable[4096]` direct-index
  lookup table (indexed by `addr >> 12`)
- Removed `BlockRead`/`BlockWrite` indirection — inlined into `MMIORead`/`MMIOWrite`
- Removed `MemoryRegionOps` struct — function pointers stored directly in `NV2ABlockInfo`

**`update_irq` cleanup:**
- Returns a `uint32_t` summary bitmask of pending interrupt sources
- Uses bitmask expression instead of `bool any_pending` accumulator

### Phase 7: Architectural Cleanup

**Removed QEMU compatibility typedefs:**
- `hwaddr` → replaced with `xbox::addr_xt` throughout
- `value_t` → replaced with `uint32_t`
- `ldl_le_p`, `stl_le_p`, `stq_le_p` → replaced with direct `*` dereference and casts
- `read_func`/`write_func`/`MemoryRegionOps` → replaced with `nv2a_read_func`/`nv2a_write_func`

**Updated file headers:**
- ~35 files had XQEMU source URLs and references updated to note significant rework

## Total Lines Removed

| Category | Files | LOC |
|----------|-------|-----|
| Thread/sync primitives | 3 deleted | 545 |
| GLib dead code | 2 deleted | 465 |
| QEMU queue macros | 1 deleted | 414 |
| Compatibility wrappers/tables | inline removed | ~40 |
| Cleaned code (typedefs, stubs) | ~5 modified | ~50 |
| **Total** | **6 deleted, many modified** | **~1500** |

## Remaining QEMU References

The following components still contain code derived from QEMU:

1. **USB subsystem** (`src/devices/usb/`) — OHCI controller and USB device model were
   ported from QEMU. These use `std::list` now (Phase 3) but the device model structure
   remains QEMU-derived. Further cleanup would require rearchitecting the USB stack.
2. **NVNet device** (`src/devices/network/`) — Ethernet controller from XQEMU. Minimal
   changes made; primarily cosmetic.
3. **Xbox kernel HAL** — Uses QEMU's `PKINTERRUPT` pattern for interrupt dispatch,
   which is natural since the Xbox kernel API mirrors this.
