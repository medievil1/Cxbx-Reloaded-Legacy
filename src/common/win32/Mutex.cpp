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
// *  (c) 2002-2003 Aaron Robinson <caustik@caustik.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#include "Mutex.h"

// Number of 1ms Sleep() iterations after which we check whether the process
// that owns the mutex cross-process is still alive.  At 1ms per iteration this
// is approximately 100ms.
static constexpr unsigned int DEAD_PROCESS_CHECK_INTERVAL = 100;

// ******************************************************************
// * Constructor
// ******************************************************************
Mutex::Mutex()
{
    InterlockedExchange(&m_MutexLock, 0);
    InterlockedExchange(&m_OwnerProcess, 0);
    InterlockedExchange(&m_OwnerThread, 0);
    InterlockedExchange(&m_LockCount, 0);
}

// ******************************************************************
// * Lock
// ******************************************************************
void Mutex::Lock()
{
    LONG _CurrentProcessId = (LONG) GetCurrentProcessId();
    LONG _CurrentThreadId = (LONG) GetCurrentThreadId();
    // Counts consecutive iterations where a foreign process holds the lock,
    // used to trigger a periodic dead-process check (every ~100ms).
    unsigned int stallCount = 0;
    while(true)
    {
        // Grab the lock, letting us look at the variables
#if defined(_MSC_VER) && (_MSC_VER < 1300)  // We are not using VC++.NET
        while(InterlockedCompareExchange((LPVOID*)&m_MutexLock, (LPVOID)1, (LPVOID)0))
#else
        while(InterlockedCompareExchange((LPLONG)&m_MutexLock, (LONG)1, (LONG)0))
#endif
            Sleep(1);

        // Are we the the new owner?
        if (!m_OwnerProcess)
        {
            stallCount = 0;
            // Take ownership
            InterlockedExchange(&m_OwnerProcess, _CurrentProcessId);
            InterlockedExchange(&m_OwnerThread, _CurrentThreadId);
            InterlockedExchange(&m_LockCount, 1);

            // Unlock the mutex itself
            InterlockedExchange(&m_MutexLock, 0);

            return;
        }

        // If a different process owns this mutex right now, unlock
        // the mutex lock and wait.  The reading need not be
        // interlocked.
        if ((m_OwnerProcess != _CurrentProcessId) ||
            (m_OwnerThread  != _CurrentThreadId))
        {
            // Periodically check whether the owning process is still alive.
            // A process that crashed or was force-terminated (e.g. via
            // TerminateProcess) may never call Unlock(), leaving the mutex
            // permanently abandoned.  Detect this and forcibly reclaim it.
            if (m_OwnerProcess != _CurrentProcessId && ++stallCount >= DEAD_PROCESS_CHECK_INTERVAL)
            {
                stallCount = 0;
                LONG ownerPID = m_OwnerProcess;
                HANDLE hOwner = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)ownerPID);
                bool ownerDead;
                if (hOwner == NULL) {
                    // OpenProcess returned NULL: the process does not exist or we
                    // do not have access rights to it — treat it as gone.
                    ownerDead = true;
                } else {
                    // A non-NULL handle was returned.  Poll with a zero timeout:
                    // WAIT_OBJECT_0 means the process has already terminated.
                    ownerDead = (WaitForSingleObject(hOwner, 0) == WAIT_OBJECT_0);
                    CloseHandle(hOwner);
                }
                if (ownerDead)
                {
                    // Owner process is gone; forcibly reset the mutex so any
                    // waiting process can acquire it on the next iteration.
                    InterlockedExchange(&m_OwnerProcess, 0);
                    InterlockedExchange(&m_OwnerThread, 0);
                    InterlockedExchange(&m_LockCount, 0);
                }
            }

            // Unlock the mutex itself
            InterlockedExchange(&m_MutexLock, 0);

            // Wait and try again
			// TODO : Improve performance replacing Sleep(1) with YieldProcessor and perhaps an optional SpinLock
            Sleep(1);
            continue;
        }

        // The mutex was already locked, but by us.  Just increment
        // the lock count and unlock the mutex itself.
        InterlockedIncrement(&m_LockCount);
        InterlockedExchange(&m_MutexLock, 0);

        return;
    }
}

// ******************************************************************
// * Unlock
// ******************************************************************
void Mutex::Unlock()
{
    // Grab the lock, letting us look at the variables
#if defined(_MSC_VER) && (_MSC_VER < 1300)  // We are not using VC++.NET
    while(InterlockedCompareExchange((LPVOID*)&m_MutexLock, (LPVOID)1, (LPVOID)0))
#else
    while (InterlockedCompareExchange((LPLONG)&m_MutexLock, (LONG)1, (LONG)0))
#endif
        Sleep(1);

    // Decrement the lock count
    if (!InterlockedDecrement(&m_LockCount))
    {
        // Mark the mutex as now unused
        InterlockedExchange(&m_OwnerProcess, 0);
        InterlockedExchange(&m_OwnerThread, 0);
    }

    // Unlock the mutex itself
    InterlockedExchange(&m_MutexLock, 0);
}