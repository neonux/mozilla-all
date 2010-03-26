// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <ObjBase.h>
#include <winternl.h>
#include <psapi.h>

#include <cassert>
#include <cstdio>

#include "common/windows/string_utils-inl.h"

#include "client/windows/common/ipc_protocol.h"
#include "client/windows/handler/exception_handler.h"
#include "common/windows/guid_string.h"

namespace {

// Helper for GetProcId()
bool GetProcIdViaGetProcessId(HANDLE process, DWORD* id) {
  // Dynamically get a pointer to GetProcessId().
  typedef DWORD (WINAPI *GetProcessIdFunction)(HANDLE);
  static GetProcessIdFunction GetProcessIdPtr = NULL;
  static bool initialize_get_process_id = true;
  if (initialize_get_process_id) {
    initialize_get_process_id = false;
    HMODULE kernel32_handle = GetModuleHandle(L"kernel32.dll");
    if (!kernel32_handle) {
      return false;
    }
    GetProcessIdPtr = reinterpret_cast<GetProcessIdFunction>(GetProcAddress(
        kernel32_handle, "GetProcessId"));
  }
  if (!GetProcessIdPtr)
    return false;
  // Ask for the process ID.
  *id = (*GetProcessIdPtr)(process);
  return true;
}

// Helper for GetProcId()
bool GetProcIdViaNtQueryInformationProcess(HANDLE process, DWORD* id) {
  // Dynamically get a pointer to NtQueryInformationProcess().
  typedef NTSTATUS (WINAPI *NtQueryInformationProcessFunction)(
      HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
  static NtQueryInformationProcessFunction NtQueryInformationProcessPtr = NULL;
  static bool initialize_query_information_process = true;
  if (initialize_query_information_process) {
    initialize_query_information_process = false;
    // According to nsylvain, ntdll.dll is guaranteed to be loaded, even though
    // the Windows docs seem to imply that you should LoadLibrary() it.
    HMODULE ntdll_handle = GetModuleHandle(L"ntdll.dll");
    if (!ntdll_handle) {
      return false;
    }
    NtQueryInformationProcessPtr =
        reinterpret_cast<NtQueryInformationProcessFunction>(GetProcAddress(
            ntdll_handle, "NtQueryInformationProcess"));
  }
  if (!NtQueryInformationProcessPtr)
    return false;
  // Ask for the process ID.
  PROCESS_BASIC_INFORMATION info;
  ULONG bytes_returned;
  NTSTATUS status = (*NtQueryInformationProcessPtr)(process,
                                                    ProcessBasicInformation,
                                                    &info, sizeof info,
                                                    &bytes_returned);
  if (!SUCCEEDED(status) || (bytes_returned != (sizeof info)))
    return false;

  *id = static_cast<DWORD>(info.UniqueProcessId);
  return true;
}

DWORD GetProcId(HANDLE process) {
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    // Try to use GetProcessId(), if it exists.  Fall back on
    // NtQueryInformationProcess() otherwise (< Win XP SP1).
    DWORD id;
    bool success =
        GetProcIdViaGetProcessId(process_with_query_rights, &id) ||
        GetProcIdViaNtQueryInformationProcess(process_with_query_rights, &id);
    CloseHandle(process_with_query_rights);
    if (success)
      return id;
  }

  // We're screwed.
  return 0;
}

} // namespace

namespace google_breakpad {

static const int kWaitForHandlerThreadMs = 60000;
static const int kExceptionHandlerThreadInitialStackSize = 64 * 1024;

vector<ExceptionHandler*>* ExceptionHandler::handler_stack_ = NULL;
LONG ExceptionHandler::handler_stack_index_ = 0;
CRITICAL_SECTION ExceptionHandler::handler_stack_critical_section_;
volatile LONG ExceptionHandler::instance_count_ = 0;

ExceptionHandler::ExceptionHandler(const wstring& dump_path,
                                   FilterCallback filter,
                                   MinidumpCallback callback,
                                   void* callback_context,
                                   int handler_types,
                                   MINIDUMP_TYPE dump_type,
                                   const wchar_t* pipe_name,
                                   const CustomClientInfo* custom_info) {
  Initialize(dump_path,
             filter,
             callback,
             callback_context,
             handler_types,
             dump_type,
             pipe_name,
             custom_info);
}

ExceptionHandler::ExceptionHandler(const wstring &dump_path,
                                   FilterCallback filter,
                                   MinidumpCallback callback,
                                   void* callback_context,
                                   int handler_types) {
  Initialize(dump_path,
             filter,
             callback,
             callback_context,
             handler_types,
             MiniDumpNormal,
             NULL,
             NULL);
}

void ExceptionHandler::Initialize(const wstring& dump_path,
                                  FilterCallback filter,
                                  MinidumpCallback callback,
                                  void* callback_context,
                                  int handler_types,
                                  MINIDUMP_TYPE dump_type,
                                  const wchar_t* pipe_name,
                                  const CustomClientInfo* custom_info) {
  LONG instance_count = InterlockedIncrement(&instance_count_);
  filter_ = filter;
  callback_ = callback;
  callback_context_ = callback_context;
  dump_path_c_ = NULL;
  next_minidump_id_c_ = NULL;
  next_minidump_path_c_ = NULL;
  dbghelp_module_ = NULL;
  minidump_write_dump_ = NULL;
  dump_type_ = dump_type;
  rpcrt4_module_ = NULL;
  uuid_create_ = NULL;
  handler_types_ = handler_types;
  previous_filter_ = NULL;
#if _MSC_VER >= 1400  // MSVC 2005/8
  previous_iph_ = NULL;
#endif  // _MSC_VER >= 1400
  previous_pch_ = NULL;
  handler_thread_ = NULL;
  is_shutdown_ = false;
  handler_start_semaphore_ = NULL;
  handler_finish_semaphore_ = NULL;
  requesting_thread_id_ = 0;
  exception_info_ = NULL;
  assertion_ = NULL;
  handler_return_value_ = false;
  handle_debug_exceptions_ = false;

  // Attempt to use out-of-process if user has specified pipe name.
  if (pipe_name != NULL) {
    scoped_ptr<CrashGenerationClient> client(
        new CrashGenerationClient(pipe_name,
                                  dump_type_,
                                  custom_info));

    // If successful in registering with the monitoring process,
    // there is no need to setup in-process crash generation.
    if (client->Register()) {
      crash_generation_client_.reset(client.release());
    }
  }

  if (!IsOutOfProcess()) {
    // Either client did not ask for out-of-process crash generation
    // or registration with the server process failed. In either case,
    // setup to do in-process crash generation.

    // Set synchronization primitives and the handler thread.  Each
    // ExceptionHandler object gets its own handler thread because that's the
    // only way to reliably guarantee sufficient stack space in an exception,
    // and it allows an easy way to get a snapshot of the requesting thread's
    // context outside of an exception.
    InitializeCriticalSection(&handler_critical_section_);
    handler_start_semaphore_ = CreateSemaphore(NULL, 0, 1, NULL);
    assert(handler_start_semaphore_ != NULL);

    handler_finish_semaphore_ = CreateSemaphore(NULL, 0, 1, NULL);
    assert(handler_finish_semaphore_ != NULL);

    // Don't attempt to create the thread if we could not create the semaphores.
    if (handler_finish_semaphore_ != NULL && handler_start_semaphore_ != NULL) {
      DWORD thread_id;
      handler_thread_ = CreateThread(NULL,         // lpThreadAttributes
                                     kExceptionHandlerThreadInitialStackSize,
                                     ExceptionHandlerThreadMain,
                                     this,         // lpParameter
                                     0,            // dwCreationFlags
                                     &thread_id);
      assert(handler_thread_ != NULL);
    }

    dbghelp_module_ = LoadLibrary(L"dbghelp.dll");
    if (dbghelp_module_) {
      minidump_write_dump_ = reinterpret_cast<MiniDumpWriteDump_type>(
          GetProcAddress(dbghelp_module_, "MiniDumpWriteDump"));
    }

    // Load this library dynamically to not affect existing projects.  Most
    // projects don't link against this directly, it's usually dynamically
    // loaded by dependent code.
    rpcrt4_module_ = LoadLibrary(L"rpcrt4.dll");
    if (rpcrt4_module_) {
      uuid_create_ = reinterpret_cast<UuidCreate_type>(
          GetProcAddress(rpcrt4_module_, "UuidCreate"));
    }

    // set_dump_path calls UpdateNextID.  This sets up all of the path and id
    // strings, and their equivalent c_str pointers.
    set_dump_path(dump_path);
  }

  // There is a race condition here. If the first instance has not yet
  // initialized the critical section, the second (and later) instances may
  // try to use uninitialized critical section object. The feature of multiple
  // instances in one module is not used much, so leave it as is for now.
  // One way to solve this in the current design (that is, keeping the static
  // handler stack) is to use spin locks with volatile bools to synchronize
  // the handler stack. This works only if the compiler guarantees to generate
  // cache coherent code for volatile.
  // TODO(munjal): Fix this in a better way by changing the design if possible.

  // Lazy initialization of the handler_stack_critical_section_
  if (instance_count == 1) {
    InitializeCriticalSection(&handler_stack_critical_section_);
  }

  if (handler_types != HANDLER_NONE) {
    EnterCriticalSection(&handler_stack_critical_section_);

    // The first time an ExceptionHandler that installs a handler is
    // created, set up the handler stack.
    if (!handler_stack_) {
      handler_stack_ = new vector<ExceptionHandler*>();
    }
    handler_stack_->push_back(this);

    if (handler_types & HANDLER_EXCEPTION)
      previous_filter_ = SetUnhandledExceptionFilter(HandleException);

#if _MSC_VER >= 1400  // MSVC 2005/8
    if (handler_types & HANDLER_INVALID_PARAMETER)
      previous_iph_ = _set_invalid_parameter_handler(HandleInvalidParameter);
#endif  // _MSC_VER >= 1400

    if (handler_types & HANDLER_PURECALL)
      previous_pch_ = _set_purecall_handler(HandlePureVirtualCall);

    LeaveCriticalSection(&handler_stack_critical_section_);
  }
}

ExceptionHandler::~ExceptionHandler() {
  if (dbghelp_module_) {
    FreeLibrary(dbghelp_module_);
  }

  if (rpcrt4_module_) {
    FreeLibrary(rpcrt4_module_);
  }

  if (handler_types_ != HANDLER_NONE) {
    EnterCriticalSection(&handler_stack_critical_section_);

    if (handler_types_ & HANDLER_EXCEPTION)
      SetUnhandledExceptionFilter(previous_filter_);

#if _MSC_VER >= 1400  // MSVC 2005/8
    if (handler_types_ & HANDLER_INVALID_PARAMETER)
      _set_invalid_parameter_handler(previous_iph_);
#endif  // _MSC_VER >= 1400

    if (handler_types_ & HANDLER_PURECALL)
      _set_purecall_handler(previous_pch_);

    if (handler_stack_->back() == this) {
      handler_stack_->pop_back();
    } else {
      // TODO(mmentovai): use advapi32!ReportEvent to log the warning to the
      // system's application event log.
      fprintf(stderr, "warning: removing Breakpad handler out of order\n");
      vector<ExceptionHandler*>::iterator iterator = handler_stack_->begin();
      while (iterator != handler_stack_->end()) {
        if (*iterator == this) {
          iterator = handler_stack_->erase(iterator);
        } else {
          ++iterator;
        }
      }
    }

    if (handler_stack_->empty()) {
      // When destroying the last ExceptionHandler that installed a handler,
      // clean up the handler stack.
      delete handler_stack_;
      handler_stack_ = NULL;
    }

    LeaveCriticalSection(&handler_stack_critical_section_);
  }

  // Some of the objects were only initialized if out of process
  // registration was not done.
  if (!IsOutOfProcess()) {
#ifdef BREAKPAD_NO_TERMINATE_THREAD
    // Clean up the handler thread and synchronization primitives. The handler
    // thread is either waiting on the semaphore to handle a crash or it is
    // handling a crash. Coming out of the wait is fast but wait more in the
    // eventuality a crash is handled.  This compilation option results in a
    // deadlock if the exception handler is destroyed while executing code
    // inside DllMain.
    is_shutdown_ = true;
    ReleaseSemaphore(handler_start_semaphore_, 1, NULL);
    WaitForSingleObject(handler_thread_, kWaitForHandlerThreadMs);
#else
    TerminateThread(handler_thread_, 1);
#endif  // BREAKPAD_NO_TERMINATE_THREAD

    CloseHandle(handler_thread_);
    handler_thread_ = NULL;
    DeleteCriticalSection(&handler_critical_section_);
    CloseHandle(handler_start_semaphore_);
    CloseHandle(handler_finish_semaphore_);
  }

  // There is a race condition in the code below: if this instance is
  // deleting the static critical section and a new instance of the class
  // is created, then there is a possibility that the critical section be
  // initialized while the same critical section is being deleted. Given the
  // usage pattern for the code, this race condition is unlikely to hit, but it
  // is a race condition nonetheless.
  if (InterlockedDecrement(&instance_count_) == 0) {
    DeleteCriticalSection(&handler_stack_critical_section_);
  }
}

// static
DWORD ExceptionHandler::ExceptionHandlerThreadMain(void* lpParameter) {
  ExceptionHandler* self = reinterpret_cast<ExceptionHandler *>(lpParameter);
  assert(self);
  assert(self->handler_start_semaphore_ != NULL);
  assert(self->handler_finish_semaphore_ != NULL);

  while (true) {
    if (WaitForSingleObject(self->handler_start_semaphore_, INFINITE) ==
        WAIT_OBJECT_0) {
      // Perform the requested action.
      if (self->is_shutdown_) {
        // The instance of the exception handler is being destroyed.
        break;
      } else {
        self->handler_return_value_ =
            self->WriteMinidumpWithException(self->requesting_thread_id_,
                                             self->exception_info_,
                                             self->assertion_);
      }

      // Allow the requesting thread to proceed.
      ReleaseSemaphore(self->handler_finish_semaphore_, 1, NULL);
    }
  }

  // This statement is not reached when the thread is unconditionally
  // terminated by the ExceptionHandler destructor.
  return 0;
}

// HandleException and HandleInvalidParameter must create an
// AutoExceptionHandler object to maintain static state and to determine which
// ExceptionHandler instance to use.  The constructor locates the correct
// instance, and makes it available through get_handler().  The destructor
// restores the state in effect prior to allocating the AutoExceptionHandler.
class AutoExceptionHandler {
 public:
  AutoExceptionHandler() {
    // Increment handler_stack_index_ so that if another Breakpad handler is
    // registered using this same HandleException function, and it needs to be
    // called while this handler is running (either becaause this handler
    // declines to handle the exception, or an exception occurs during
    // handling), HandleException will find the appropriate ExceptionHandler
    // object in handler_stack_ to deliver the exception to.
    //
    // Because handler_stack_ is addressed in reverse (as |size - index|),
    // preincrementing handler_stack_index_ avoids needing to subtract 1 from
    // the argument to |at|.
    //
    // The index is maintained instead of popping elements off of the handler
    // stack and pushing them at the end of this method.  This avoids ruining
    // the order of elements in the stack in the event that some other thread
    // decides to manipulate the handler stack (such as creating a new
    // ExceptionHandler object) while an exception is being handled.
    EnterCriticalSection(&ExceptionHandler::handler_stack_critical_section_);
    handler_ = ExceptionHandler::handler_stack_->at(
        ExceptionHandler::handler_stack_->size() -
        ++ExceptionHandler::handler_stack_index_);
    LeaveCriticalSection(&ExceptionHandler::handler_stack_critical_section_);

    // In case another exception occurs while this handler is doing its thing,
    // it should be delivered to the previous filter.
    SetUnhandledExceptionFilter(handler_->previous_filter_);
#if _MSC_VER >= 1400  // MSVC 2005/8
    _set_invalid_parameter_handler(handler_->previous_iph_);
#endif  // _MSC_VER >= 1400
    _set_purecall_handler(handler_->previous_pch_);
  }

  ~AutoExceptionHandler() {
    // Put things back the way they were before entering this handler.
    SetUnhandledExceptionFilter(ExceptionHandler::HandleException);
#if _MSC_VER >= 1400  // MSVC 2005/8
    _set_invalid_parameter_handler(ExceptionHandler::HandleInvalidParameter);
#endif  // _MSC_VER >= 1400
    _set_purecall_handler(ExceptionHandler::HandlePureVirtualCall);

    EnterCriticalSection(&ExceptionHandler::handler_stack_critical_section_);
    --ExceptionHandler::handler_stack_index_;
    LeaveCriticalSection(&ExceptionHandler::handler_stack_critical_section_);
  }

  ExceptionHandler* get_handler() const { return handler_; }

 private:
  ExceptionHandler* handler_;
};

// static
LONG ExceptionHandler::HandleException(EXCEPTION_POINTERS* exinfo) {
  AutoExceptionHandler auto_exception_handler;
  ExceptionHandler* current_handler = auto_exception_handler.get_handler();

  // Ignore EXCEPTION_BREAKPOINT and EXCEPTION_SINGLE_STEP exceptions.  This
  // logic will short-circuit before calling WriteMinidumpOnHandlerThread,
  // allowing something else to handle the breakpoint without incurring the
  // overhead transitioning to and from the handler thread.  This behavior
  // can be overridden by calling ExceptionHandler::set_handle_debug_exceptions.
  DWORD code = exinfo->ExceptionRecord->ExceptionCode;
  LONG action;
  bool is_debug_exception = (code == EXCEPTION_BREAKPOINT) ||
                            (code == EXCEPTION_SINGLE_STEP);

  bool success = false;

  if (!is_debug_exception ||
      current_handler->get_handle_debug_exceptions()) {
    // If out-of-proc crash handler client is available, we have to use that
    // to generate dump and we cannot fall back on in-proc dump generation
    // because we never prepared for an in-proc dump generation

    // In case of out-of-process dump generation, directly call
    // WriteMinidumpWithException since there is no separate thread running.
    if (current_handler->IsOutOfProcess()) {
      success = current_handler->WriteMinidumpWithException(
          GetCurrentThreadId(),
          exinfo,
          NULL);
    } else {
      success = current_handler->WriteMinidumpOnHandlerThread(exinfo, NULL);
    }
  }

  // The handler fully handled the exception.  Returning
  // EXCEPTION_EXECUTE_HANDLER indicates this to the system, and usually
  // results in the application being terminated.
  //
  // Note: If the application was launched from within the Cygwin
  // environment, returning EXCEPTION_EXECUTE_HANDLER seems to cause the
  // application to be restarted.
  if (success) {
    action = EXCEPTION_EXECUTE_HANDLER;
  } else {
    // There was an exception, it was a breakpoint or something else ignored
    // above, or it was passed to the handler, which decided not to handle it.
    // This could be because the filter callback didn't want it, because
    // minidump writing failed for some reason, or because the post-minidump
    // callback function indicated failure.  Give the previous handler a
    // chance to do something with the exception.  If there is no previous
    // handler, return EXCEPTION_CONTINUE_SEARCH, which will allow a debugger
    // or native "crashed" dialog to handle the exception.
    if (current_handler->previous_filter_) {
      action = current_handler->previous_filter_(exinfo);
    } else {
      action = EXCEPTION_CONTINUE_SEARCH;
    }
  }

  return action;
}

#if _MSC_VER >= 1400  // MSVC 2005/8
// static
void ExceptionHandler::HandleInvalidParameter(const wchar_t* expression,
                                              const wchar_t* function,
                                              const wchar_t* file,
                                              unsigned int line,
                                              uintptr_t reserved) {
  // This is an invalid parameter, not an exception.  It's safe to play with
  // sprintf here.
  AutoExceptionHandler auto_exception_handler;
  ExceptionHandler* current_handler = auto_exception_handler.get_handler();

  MDRawAssertionInfo assertion;
  memset(&assertion, 0, sizeof(assertion));
  _snwprintf_s(reinterpret_cast<wchar_t*>(assertion.expression),
               sizeof(assertion.expression) / sizeof(assertion.expression[0]),
               _TRUNCATE, L"%s", expression);
  _snwprintf_s(reinterpret_cast<wchar_t*>(assertion.function),
               sizeof(assertion.function) / sizeof(assertion.function[0]),
               _TRUNCATE, L"%s", function);
  _snwprintf_s(reinterpret_cast<wchar_t*>(assertion.file),
               sizeof(assertion.file) / sizeof(assertion.file[0]),
               _TRUNCATE, L"%s", file);
  assertion.line = line;
  assertion.type = MD_ASSERTION_INFO_TYPE_INVALID_PARAMETER;

  bool success = false;
  // In case of out-of-process dump generation, directly call
  // WriteMinidumpWithException since there is no separate thread running.
  if (current_handler->IsOutOfProcess()) {
    success = current_handler->WriteMinidumpWithException(
        GetCurrentThreadId(),
        NULL,
        &assertion);
  } else {
    success = current_handler->WriteMinidumpOnHandlerThread(NULL, &assertion);
  }

  if (!success) {
    if (current_handler->previous_iph_) {
      // The handler didn't fully handle the exception.  Give it to the
      // previous invalid parameter handler.
      current_handler->previous_iph_(expression,
                                     function,
                                     file,
                                     line,
                                     reserved);
    } else {
      // If there's no previous handler, pass the exception back in to the
      // invalid parameter handler's core.  That's the routine that called this
      // function, but now, since this function is no longer registered (and in
      // fact, no function at all is registered), this will result in the
      // default code path being taken: _CRT_DEBUGGER_HOOK and _invoke_watson.
      // Use _invalid_parameter where it exists (in _DEBUG builds) as it passes
      // more information through.  In non-debug builds, it is not available,
      // so fall back to using _invalid_parameter_noinfo.  See invarg.c in the
      // CRT source.
#ifdef _DEBUG
      _invalid_parameter(expression, function, file, line, reserved);
#else  // _DEBUG
      _invalid_parameter_noinfo();
#endif  // _DEBUG
    }
  }

  // The handler either took care of the invalid parameter problem itself,
  // or passed it on to another handler.  "Swallow" it by exiting, paralleling
  // the behavior of "swallowing" exceptions.
  exit(0);
}
#endif  // _MSC_VER >= 1400

// static
void ExceptionHandler::HandlePureVirtualCall() {
  AutoExceptionHandler auto_exception_handler;
  ExceptionHandler* current_handler = auto_exception_handler.get_handler();

  MDRawAssertionInfo assertion;
  memset(&assertion, 0, sizeof(assertion));
  assertion.type = MD_ASSERTION_INFO_TYPE_PURE_VIRTUAL_CALL;

  bool success = false;
  // In case of out-of-process dump generation, directly call
  // WriteMinidumpWithException since there is no separate thread running.

  if (current_handler->IsOutOfProcess()) {
    success = current_handler->WriteMinidumpWithException(
        GetCurrentThreadId(),
        NULL,
        &assertion);
  } else {
    success = current_handler->WriteMinidumpOnHandlerThread(NULL, &assertion);
  }

  if (!success) {
    if (current_handler->previous_pch_) {
      // The handler didn't fully handle the exception.  Give it to the
      // previous purecall handler.
      current_handler->previous_pch_();
    } else {
      // If there's no previous handler, return and let _purecall handle it.
      // This will just put up an assertion dialog.
      return;
    }
  }

  // The handler either took care of the invalid parameter problem itself,
  // or passed it on to another handler.  "Swallow" it by exiting, paralleling
  // the behavior of "swallowing" exceptions.
  exit(0);
}

bool ExceptionHandler::WriteMinidumpOnHandlerThread(
    EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion) {
  EnterCriticalSection(&handler_critical_section_);

  // There isn't much we can do if the handler thread
  // was not successfully created.
  if (handler_thread_ == NULL) {
    LeaveCriticalSection(&handler_critical_section_);
    return false;
  }

  // The handler thread should only be created when the semaphores are valid.
  assert(handler_start_semaphore_ != NULL);
  assert(handler_finish_semaphore_ != NULL);

  // Set up data to be passed in to the handler thread.
  requesting_thread_id_ = GetCurrentThreadId();
  exception_info_ = exinfo;
  assertion_ = assertion;

  // This causes the handler thread to call WriteMinidumpWithException.
  ReleaseSemaphore(handler_start_semaphore_, 1, NULL);

  // Wait until WriteMinidumpWithException is done and collect its return value.
  WaitForSingleObject(handler_finish_semaphore_, INFINITE);
  bool status = handler_return_value_;

  // Clean up.
  requesting_thread_id_ = 0;
  exception_info_ = NULL;
  assertion_ = NULL;

  LeaveCriticalSection(&handler_critical_section_);

  return status;
}

bool ExceptionHandler::WriteMinidump() {
  return WriteMinidumpForException(NULL);
}

bool ExceptionHandler::WriteMinidumpForException(EXCEPTION_POINTERS* exinfo) {
  // In case of out-of-process dump generation, directly call
  // WriteMinidumpWithException since there is no separate thread running.
  if (IsOutOfProcess()) {
    return WriteMinidumpWithException(GetCurrentThreadId(),
                                      exinfo,
                                      NULL);
  }

  bool success = WriteMinidumpOnHandlerThread(exinfo, NULL);
  UpdateNextID();
  return success;
}

// static
bool ExceptionHandler::WriteMinidump(const wstring &dump_path,
                                     MinidumpCallback callback,
                                     void* callback_context) {
  ExceptionHandler handler(dump_path, NULL, callback, callback_context,
                           HANDLER_NONE);
  return handler.WriteMinidump();
}

// static
bool ExceptionHandler::WriteMinidumpForChild(HANDLE child,
					     const wstring &dump_path,
					     MinidumpCallback callback,
					     void *callback_context) {
  DWORD childId = GetProcId(child);
  if (0 == childId)
    return false;

  ExceptionHandler handler(dump_path, NULL, callback, callback_context,
                           HANDLER_NONE);
  bool success = handler.WriteMinidumpWithExceptionForProcess(
      0, NULL, NULL, child, childId, false);

  if (callback) {
    success = callback(handler.dump_path_c_, handler.next_minidump_id_c_,
		       callback_context, NULL, NULL, success);
  }

  return success;
}

bool ExceptionHandler::WriteMinidumpWithException(
    DWORD requesting_thread_id,
    EXCEPTION_POINTERS* exinfo,
    MDRawAssertionInfo* assertion) {
  // Give user code a chance to approve or prevent writing a minidump.  If the
  // filter returns false, don't handle the exception at all.  If this method
  // was called as a result of an exception, returning false will cause
  // HandleException to call any previous handler or return
  // EXCEPTION_CONTINUE_SEARCH on the exception thread, allowing it to appear
  // as though this handler were not present at all.
  if (filter_ && !filter_(callback_context_, exinfo, assertion)) {
    return false;
  }

  bool success = false;
  if (IsOutOfProcess()) {
    // Use the EXCEPTION_POINTERS overload for RequestDump if
    // both exinfo and assertion are NULL.
    if (!assertion) {
      success = crash_generation_client_->RequestDump(exinfo);
    } else {
      success = crash_generation_client_->RequestDump(assertion);
    }
  } else {
    success = WriteMinidumpWithExceptionForProcess(requesting_thread_id,
                                                   exinfo,
                                                   assertion,
                                                   GetCurrentProcess(),
                                                   GetCurrentProcessId(),
                                                   true);
  }

  if (callback_) {
    // TODO(munjal): In case of out-of-process dump generation, both
    // dump_path_c_ and next_minidump_id_ will be NULL. For out-of-process
    // scenario, the server process ends up creating the dump path and dump
    // id so they are not known to the client.
    success = callback_(dump_path_c_, next_minidump_id_c_, callback_context_,
                        exinfo, assertion, success);
  }

  return success;
}

bool ExceptionHandler::WriteMinidumpWithExceptionForProcess(
    DWORD requesting_thread_id,
    EXCEPTION_POINTERS* exinfo,
    MDRawAssertionInfo* assertion,
    HANDLE process,
    DWORD processId,
    bool write_requester_stream) {
  bool success = false;
  if (minidump_write_dump_) {
    HANDLE dump_file = CreateFile(next_minidump_path_c_,
                                  GENERIC_WRITE,
                                  0,  // no sharing
                                  NULL,
                                  CREATE_NEW,  // fail if exists
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    if (dump_file != INVALID_HANDLE_VALUE) {
      MINIDUMP_EXCEPTION_INFORMATION except_info;
      except_info.ThreadId = requesting_thread_id;
      except_info.ExceptionPointers = exinfo;
      except_info.ClientPointers = FALSE;

      // Leave room in user_stream_array for possible breakpad and
      // assertion info streams.
      MINIDUMP_USER_STREAM user_stream_array[2];
      MINIDUMP_USER_STREAM_INFORMATION user_streams;
      user_streams.UserStreamCount = 0;
      user_streams.UserStreamArray = user_stream_array;

      if (write_requester_stream) {
        // Add an MDRawBreakpadInfo stream to the minidump, to provide
        // additional information about the exception handler to the
        // Breakpad processor.  The information will help the
        // processor determine which threads are relevant.  The
        // Breakpad processor does not require this information but
        // can function better with Breakpad-generated dumps when it
        // is present.  The native debugger is not harmed by the
        // presence of this information.
        MDRawBreakpadInfo breakpad_info;
        breakpad_info.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID |
                                 MD_BREAKPAD_INFO_VALID_REQUESTING_THREAD_ID;
        breakpad_info.dump_thread_id = GetCurrentThreadId();
        breakpad_info.requesting_thread_id = requesting_thread_id;

        int idx = user_streams.UserStreamCount;
        user_stream_array[idx].Type = MD_BREAKPAD_INFO_STREAM;
        user_stream_array[idx].BufferSize = sizeof(breakpad_info);
        user_stream_array[idx].Buffer = &breakpad_info;
        ++user_streams.UserStreamCount;
      }

      if (assertion) {
        int idx = user_streams.UserStreamCount;
        user_stream_array[idx].Type = MD_ASSERTION_INFO_STREAM;
        user_stream_array[idx].BufferSize = sizeof(MDRawAssertionInfo);
        user_stream_array[idx].Buffer = assertion;
        ++user_streams.UserStreamCount;
      }

      // The explicit comparison to TRUE avoids a warning (C4800).
      success = (minidump_write_dump_(process,
                                      processId,
                                      dump_file,
                                      dump_type_,
                                      exinfo ? &except_info : NULL,
                                      &user_streams,
                                      NULL) == TRUE);

      CloseHandle(dump_file);
    }
  }

  return success;
}

void ExceptionHandler::UpdateNextID() {
  assert(uuid_create_);
  UUID id = {0};
  if (uuid_create_) {
    uuid_create_(&id);
  }
  next_minidump_id_ = GUIDString::GUIDToWString(&id);
  next_minidump_id_c_ = next_minidump_id_.c_str();

  wchar_t minidump_path[MAX_PATH];
  swprintf(minidump_path, MAX_PATH, L"%s\\%s.dmp",
           dump_path_c_, next_minidump_id_c_);

  // remove when VC++7.1 is no longer supported
  minidump_path[MAX_PATH - 1] = L'\0';

  next_minidump_path_ = minidump_path;
  next_minidump_path_c_ = next_minidump_path_.c_str();
}

}  // namespace google_breakpad
