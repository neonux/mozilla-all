#include "TestRPCErrorCleanup.h"

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

#include "IPDLUnitTests.h"      // fail etc.
#include "IPDLUnitTestSubprocess.h"

using mozilla::CondVar;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

// NB: this test does its own shutdown, rather than going through
// QuitParent(), because it's testing degenerate edge cases

void DeleteSubprocess(Mutex* mutex, CondVar* cvar)
{
    MutexAutoLock lock(*mutex);

    delete gSubprocess;
    gSubprocess = NULL;

    cvar->Notify();
}

void DeleteTheWorld()
{
    delete static_cast<TestRPCErrorCleanupParent*>(gParentActor);
    gParentActor = NULL;

    // needs to be synchronous to avoid affecting event ordering on
    // the main thread
    Mutex mutex("TestRPCErrorCleanup.DeleteTheWorld.mutex");
    CondVar cvar(mutex, "TestRPCErrorCleanup.DeleteTheWorld.cvar");

    MutexAutoLock lock(mutex);

    XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(DeleteSubprocess, &mutex, &cvar));

    cvar.Wait();
}

void Done()
{
  static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
  nsCOMPtr<nsIAppShell> appShell (do_GetService(kAppShellCID));
  appShell->Exit();

  passed(__FILE__);
}

TestRPCErrorCleanupParent::TestRPCErrorCleanupParent()
{
    MOZ_COUNT_CTOR(TestRPCErrorCleanupParent);
}

TestRPCErrorCleanupParent::~TestRPCErrorCleanupParent()
{
    MOZ_COUNT_DTOR(TestRPCErrorCleanupParent);
}

void
TestRPCErrorCleanupParent::Main()
{
    // This test models the following sequence of events
    //
    // (1) Parent: RPC out-call
    // (2) Child: crash
    // --[Parent-only hereafter]--
    // (3) RPC out-call return false
    // (4) Close()
    // --[event loop]--
    // (5) delete parentActor
    // (6) delete childProcess
    // --[event loop]--
    // (7) Channel::OnError notification
    // --[event loop]--
    // (8) Done, quit
    //
    // See bug 535298 and friends; this seqeunce of events captures
    // three differnent potential errors
    //  - Close()-after-error (semantic error previously)
    //  - use-after-free of parentActor
    //  - use-after-free of channel
    //
    // Because of legacy constraints related to nsNPAPI* code, we need
    // to ensure that this sequence of events can occur without
    // errors/crashes.

    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableFunction(DeleteTheWorld));

    // it's a failure if this *succeeds*
    if (CallError())
        fail("expected an error!");

    // it's OK to Close() a channel after an error, because nsNPAPI*
    // wants to do this
    Close();

    // we know that this event *must* be after the MaybeError
    // notification enqueued by AsyncChannel, because that event is
    // enqueued within the same mutex that ends up signaling the
    // wakeup-on-error of |CallError()| above
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(Done));
}


//-----------------------------------------------------------------------------
// child

TestRPCErrorCleanupChild::TestRPCErrorCleanupChild()
{
    MOZ_COUNT_CTOR(TestRPCErrorCleanupChild);
}

TestRPCErrorCleanupChild::~TestRPCErrorCleanupChild()
{
    MOZ_COUNT_DTOR(TestRPCErrorCleanupChild);
}

bool
TestRPCErrorCleanupChild::AnswerError()
{
    _exit(0);
    NS_RUNTIMEABORT("unreached");
    return false;
}


} // namespace _ipdltest
} // namespace mozilla
