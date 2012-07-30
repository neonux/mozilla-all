/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "mozilla/CondVar.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "nsAutoLock.h"

using namespace mozilla;

static PRThread*
spawn(void (*run)(void*), void* arg)
{
    return PR_CreateThread(PR_SYSTEM_THREAD,
                           run,
                           arg,
                           PR_PRIORITY_NORMAL,
                           PR_GLOBAL_THREAD,
                           PR_JOINABLE_THREAD,
                           0);
}


#define PASS()                                  \
    do {                                        \
        passed(__FUNCTION__);                   \
        return NS_OK;                           \
    } while (0)

#define FAIL(why)                               \
    do {                                        \
        fail("%s | %s - %s", __FILE__, __FUNCTION__, why); \
        return NS_ERROR_FAILURE;                \
    } while (0)

//-----------------------------------------------------------------------------
// Sanity check: tests that can be done on a single thread
//
static nsresult
Sanity()
{
    Mutex lock("sanity::lock");
    lock.Lock();
    lock.AssertCurrentThreadOwns();
    lock.Unlock();
    
    {
        MutexAutoLock autolock(lock);
        lock.AssertCurrentThreadOwns();
    }

    lock.Lock();
    lock.AssertCurrentThreadOwns();
    {
        MutexAutoUnlock autounlock(lock);
    }
    lock.AssertCurrentThreadOwns();
    lock.Unlock();

    Monitor mon("sanity::monitor");
    mon.Enter();
    mon.AssertCurrentThreadIn();
    mon.Enter();
    mon.AssertCurrentThreadIn();
    mon.Exit();
    mon.AssertCurrentThreadIn();
    mon.Exit();

    {
        MonitorAutoEnter automon(mon);
        mon.AssertCurrentThreadIn();
    }

    PASS();
}

//-----------------------------------------------------------------------------
// Mutex contention tests
//
static Mutex* gLock1;

static void
MutexContention_thread(void* /*arg*/)
{
    for (int i = 0; i < 100000; ++i) {
        gLock1->Lock();
        gLock1->AssertCurrentThreadOwns();
        gLock1->Unlock();
    }
}

static nsresult
MutexContention()
{
    gLock1 = new Mutex("lock1");
    // PURPOSELY not checking for OOM.  YAY!

    PRThread* t1 = spawn(MutexContention_thread, nullptr);
    PRThread* t2 = spawn(MutexContention_thread, nullptr);
    PRThread* t3 = spawn(MutexContention_thread, nullptr);

    PR_JoinThread(t1);
    PR_JoinThread(t2);
    PR_JoinThread(t3);

    delete gLock1;

    PASS();
}

//-----------------------------------------------------------------------------
// Monitor tests
//
static Monitor* gMon1;

static void
MonitorContention_thread(void* /*arg*/)
{
    for (int i = 0; i < 100000; ++i) {
        gMon1->Enter();
        gMon1->AssertCurrentThreadIn();
        gMon1->Exit();
    }
}

static nsresult
MonitorContention()
{
    gMon1 = new Monitor("mon1");

    PRThread* t1 = spawn(MonitorContention_thread, nullptr);
    PRThread* t2 = spawn(MonitorContention_thread, nullptr);
    PRThread* t3 = spawn(MonitorContention_thread, nullptr);

    PR_JoinThread(t1);
    PR_JoinThread(t2);
    PR_JoinThread(t3);

    delete gMon1;

    PASS();
}


static Monitor* gMon2;

static void
MonitorContention2_thread(void* /*arg*/)
{
    for (int i = 0; i < 100000; ++i) {
        gMon2->Enter();
        gMon2->AssertCurrentThreadIn();
        {
            gMon2->Enter();
            gMon2->AssertCurrentThreadIn();
            gMon2->Exit();
        }
        gMon2->AssertCurrentThreadIn();
        gMon2->Exit();
    }
}

static nsresult
MonitorContention2()
{
    gMon2 = new Monitor("mon1");

    PRThread* t1 = spawn(MonitorContention2_thread, nullptr);
    PRThread* t2 = spawn(MonitorContention2_thread, nullptr);
    PRThread* t3 = spawn(MonitorContention2_thread, nullptr);

    PR_JoinThread(t1);
    PR_JoinThread(t2);
    PR_JoinThread(t3);

    delete gMon2;

    PASS();
}


static Monitor* gMon3;
static PRInt32 gMonFirst;

static void
MonitorSyncSanity_thread(void* /*arg*/)
{
    gMon3->Enter();
    gMon3->AssertCurrentThreadIn();
    if (gMonFirst) {
        gMonFirst = 0;
        gMon3->Wait();
        gMon3->Enter();
    } else {
        gMon3->Notify();
        gMon3->Enter();
    }
    gMon3->AssertCurrentThreadIn();
    gMon3->Exit();
    gMon3->AssertCurrentThreadIn();
    gMon3->Exit();
}

static nsresult
MonitorSyncSanity()
{
    gMon3 = new Monitor("monitor::syncsanity");
   
    for (PRInt32 i = 0; i < 10000; ++i) {
        gMonFirst = 1;
        PRThread* ping = spawn(MonitorSyncSanity_thread, nullptr);
        PRThread* pong = spawn(MonitorSyncSanity_thread, nullptr);
        PR_JoinThread(ping);
        PR_JoinThread(pong);
    }

    delete gMon3;

    PASS();
}

//-----------------------------------------------------------------------------
// Condvar tests
//
static Mutex* gCvlock1;
static CondVar* gCv1;
static PRInt32 gCvFirst;

static void
CondVarSanity_thread(void* /*arg*/)
{
    gCvlock1->Lock();
    gCvlock1->AssertCurrentThreadOwns();
    if (gCvFirst) {
        gCvFirst = 0;
        gCv1->Wait();
    } else {
        gCv1->Notify();
    }
    gCvlock1->AssertCurrentThreadOwns();
    gCvlock1->Unlock();
}

static nsresult
CondVarSanity()
{
    gCvlock1 = new Mutex("cvlock1");
    gCv1 = new CondVar(*gCvlock1, "cvlock1");

    for (PRInt32 i = 0; i < 10000; ++i) {
        gCvFirst = 1;
        PRThread* ping = spawn(CondVarSanity_thread, nullptr);
        PRThread* pong = spawn(CondVarSanity_thread, nullptr);
        PR_JoinThread(ping);
        PR_JoinThread(pong);
    }

    delete gCv1;
    delete gCvlock1;

    PASS();
}

//-----------------------------------------------------------------------------
// AutoLock tests
//
static nsresult
AutoLock()
{
    Mutex l1("autolock");
    MutexAutoLock autol1(l1);

    l1.AssertCurrentThreadOwns();

    {
        Mutex l2("autolock2");
        MutexAutoLock autol2(l2);

        l1.AssertCurrentThreadOwns();
        l2.AssertCurrentThreadOwns();
    }

    l1.AssertCurrentThreadOwns();

    PASS();
}

//-----------------------------------------------------------------------------
// AutoUnlock tests
//
static nsresult
AutoUnlock()
{
    Mutex l1("autounlock");
    Mutex l2("autounlock2");

    l1.Lock();
    l1.AssertCurrentThreadOwns();

    {
        MutexAutoUnlock autol1(l1);
        {
            l2.Lock();
            l2.AssertCurrentThreadOwns();

            MutexAutoUnlock autol2(l2);
        }
        l2.AssertCurrentThreadOwns();
        l2.Unlock();
    }
    l1.AssertCurrentThreadOwns();

    l1.Unlock();

    PASS();
}

//-----------------------------------------------------------------------------
// AutoMonitor tests
//
static nsresult
AutoMonitor()
{
    Monitor m1("automonitor");
    Monitor m2("automonitor2");

    m1.Enter();
    m1.AssertCurrentThreadIn();
    {
        MonitorAutoEnter autom1(m1);
        m1.AssertCurrentThreadIn();

        m2.Enter();
        m2.AssertCurrentThreadIn();
        {
            MonitorAutoEnter autom2(m2);
            m1.AssertCurrentThreadIn();
            m2.AssertCurrentThreadIn();
        }
        m2.AssertCurrentThreadIn();
        m2.Exit();

        m1.AssertCurrentThreadIn();
    }
    m1.AssertCurrentThreadIn();
    m1.Exit();

    PASS();
}

//-----------------------------------------------------------------------------

int
main(int argc, char** argv)
{
    ScopedXPCOM xpcom("Synchronization (" __FILE__ ")");
    if (xpcom.failed())
        return 1;

    int rv = 0;

    if (NS_FAILED(Sanity()))
        rv = 1;
    if (NS_FAILED(MutexContention()))
        rv = 1;
    if (NS_FAILED(MonitorContention()))
        rv = 1;
    if (NS_FAILED(MonitorContention2()))
        rv = 1;
    if (NS_FAILED(MonitorSyncSanity()))
        rv = 1;
    if (NS_FAILED(CondVarSanity()))
        rv = 1;
    if (NS_FAILED(AutoLock()))
        rv = 1;
    if (NS_FAILED(AutoUnlock()))
        rv = 1;
    if (NS_FAILED(AutoMonitor()))
        rv = 1;

    return rv;
}
