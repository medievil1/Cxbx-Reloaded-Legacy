#pragma once
#include <windows.h>

struct HostMutex {
    SRWLOCK srw;
};

struct HostCond {
    CONDITION_VARIABLE cv;
};

__forceinline void host_mutex_init(HostMutex* m) {
    InitializeSRWLock(&m->srw);
}

__forceinline void host_mutex_destroy(HostMutex*) {
}

__forceinline void host_mutex_lock(HostMutex* m) {
    AcquireSRWLockExclusive(&m->srw);
}

__forceinline void host_mutex_lock_shared(HostMutex* m) {
    AcquireSRWLockShared(&m->srw);
}

__forceinline int host_mutex_trylock(HostMutex* m) {
    return !TryAcquireSRWLockExclusive(&m->srw);
}

__forceinline void host_mutex_unlock(HostMutex* m) {
    ReleaseSRWLockExclusive(&m->srw);
}

__forceinline void host_mutex_unlock_shared(HostMutex* m) {
    ReleaseSRWLockShared(&m->srw);
}

__forceinline void host_cond_init(HostCond* c) {
    InitializeConditionVariable(&c->cv);
}

__forceinline void host_cond_destroy(HostCond*) {
}

__forceinline void host_cond_signal(HostCond* c) {
    WakeConditionVariable(&c->cv);
}

__forceinline void host_cond_broadcast(HostCond* c) {
    WakeAllConditionVariable(&c->cv);
}

// Flags for cond_wait: pass 0 (default, exclusive lock) or
// CONDITION_VARIABLE_LOCKMODE_SHARED (when the mutex is held in shared mode).
__forceinline void host_cond_wait(HostCond* c, HostMutex* m, ULONG flags = 0) {
    SleepConditionVariableSRW(&c->cv, &m->srw, INFINITE, flags);
}

__forceinline bool host_cond_timedwait(HostCond* c, HostMutex* m, unsigned int ms, ULONG flags = 0) {
    return SleepConditionVariableSRW(&c->cv, &m->srw, ms, flags) == FALSE;
}
