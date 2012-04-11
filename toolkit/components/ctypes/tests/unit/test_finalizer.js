let TEST_SIZE = 100;

function run_test()
{
  let library = open_ctypes_test_lib();

  let start = library.declare("test_finalizer_start", ctypes.default_abi,
                          ctypes.void_t,
                          ctypes.size_t);
  let stop = library.declare("test_finalizer_stop", ctypes.default_abi,
                         ctypes.void_t);
  let status = library.declare("test_finalizer_resource_is_acquired",
                           ctypes.default_abi,
                           ctypes.bool,
                           ctypes.size_t);

  let samples = [];
  samples.push(
    {
      name: "size_t",
      acquire: library.declare("test_finalizer_acq_size_t",
                           ctypes.default_abi,
                           ctypes.size_t,
                           ctypes.size_t),
      release: library.declare("test_finalizer_rel_size_t",
                           ctypes.default_abi,
                           ctypes.void_t,
                           ctypes.size_t),
      compare: library.declare("test_finalizer_cmp_size_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.size_t,
                               ctypes.size_t),
      status: status
  });
  samples.push(
    {
      name: "size_t",
      acquire: library.declare("test_finalizer_acq_size_t",
                           ctypes.default_abi,
                           ctypes.size_t,
                           ctypes.size_t),
      release: library.declare("test_finalizer_rel_size_t_set_errno",
                           ctypes.default_abi,
                           ctypes.void_t,
                           ctypes.size_t),
      compare: library.declare("test_finalizer_cmp_size_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.size_t,
                               ctypes.size_t),
      status: status
  });
  samples.push(
    {
      name: "int32_t",
      acquire: library.declare("test_finalizer_acq_int32_t",
                           ctypes.default_abi,
                           ctypes.int32_t,
                           ctypes.size_t),
      release: library.declare("test_finalizer_rel_int32_t",
                           ctypes.default_abi,
                           ctypes.void_t,
                           ctypes.int32_t),
      compare: library.declare("test_finalizer_cmp_int32_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.int32_t,
                               ctypes.int32_t),
      status: status
    }
  );
  samples.push(
    {
      name: "int64_t",
      acquire: library.declare("test_finalizer_acq_int64_t",
                           ctypes.default_abi,
                           ctypes.int64_t,
                           ctypes.size_t),
      release: library.declare("test_finalizer_rel_int64_t",
                           ctypes.default_abi,
                           ctypes.void_t,
                           ctypes.int64_t),
      compare: library.declare("test_finalizer_cmp_int64_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.int64_t,
                               ctypes.int64_t),
      status: status
    }
  );
  samples.push(
    {
      name: "ptr",
      acquire: library.declare("test_finalizer_acq_ptr_t",
                           ctypes.default_abi,
                           ctypes.PointerType(ctypes.void_t),
                           ctypes.size_t),
      release: library.declare("test_finalizer_rel_ptr_t",
                           ctypes.default_abi,
                           ctypes.void_t,
                           ctypes.PointerType(ctypes.void_t)),
      compare: library.declare("test_finalizer_cmp_ptr_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.void_t.ptr,
                               ctypes.void_t.ptr),
      status: status
    }
  );
  samples.push(
    {
      name: "string",
      acquire: library.declare("test_finalizer_acq_string_t",
                               ctypes.default_abi,
                               ctypes.char.ptr,
                               ctypes.int),
      release: library.declare("test_finalizer_rel_string_t",
                               ctypes.default_abi,
                               ctypes.void_t,
                               ctypes.char.ptr),
      compare: library.declare("test_finalizer_cmp_string_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.char.ptr,
                               ctypes.char.ptr),
      status: status
    }
  );
  const rect_t = new ctypes.StructType("RECT",
                                       [{ top   : ctypes.int32_t },
                                        { left  : ctypes.int32_t },
                                        { bottom: ctypes.int32_t },
                                        { right : ctypes.int32_t }]);
  samples.push(
    {
      name: "struct",
      acquire: library.declare("test_finalizer_acq_struct_t",
                               ctypes.default_abi,
                               rect_t,
                               ctypes.int),
      release: library.declare("test_finalizer_rel_struct_t",
                               ctypes.default_abi,
                               ctypes.void_t,
                               rect_t),
      compare: library.declare("test_finalizer_cmp_struct_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               rect_t,
                               rect_t),
      status: status
    }
  );
  samples.push(
    {
      name: "size_t, release returns size_t",
      acquire: library.declare("test_finalizer_acq_size_t",
                           ctypes.default_abi,
                           ctypes.size_t,
                           ctypes.size_t),
      release: library.declare("test_finalizer_rel_size_t_return_size_t",
                           ctypes.default_abi,
                           ctypes.size_t,
                           ctypes.size_t),
      compare: library.declare("test_finalizer_cmp_size_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.size_t,
                               ctypes.size_t),
      status: status
    }
  );
  samples.push(
    {
      name: "null",
      acquire: library.declare("test_finalizer_acq_null_t",
                           ctypes.default_abi,
                           ctypes.PointerType(ctypes.void_t),
                           ctypes.size_t),
      release: library.declare("test_finalizer_rel_null_t",
                           ctypes.default_abi,
                           ctypes.void_t,
                           ctypes.PointerType(ctypes.void_t)),
      status: library.declare("test_finalizer_null_resource_is_acquired",
                             ctypes.default_abi,
                             ctypes.bool,
                             ctypes.size_t),
      compare: library.declare("test_finalizer_cmp_null_t",
                               ctypes.default_abi,
                               ctypes.bool,
                               ctypes.void_t.ptr,
                               ctypes.void_t.ptr)
    }
  );

  let tester = new ResourceTester(start, stop);
  samples.forEach(
    function(sample) {
      dump("Executing finalization test for data "+sample.name+"\n");
      tester.launch(TEST_SIZE, test_executing_finalizers, sample);
      tester.launch(TEST_SIZE, test_do_not_execute_finalizers_on_referenced_stuff, sample);
      tester.launch(TEST_SIZE, test_executing_dispose, sample);
      tester.launch(TEST_SIZE, test_executing_forget, sample);
    }
  );

  /*
   * Following test deactivated: Cycle collection never takes place
   * (see bug 727371)
  tester.launch(TEST_SIZE, test_cycles, samples[0]);
   */
  library.close();
}

// If only I could have Promises to test this :)
// There is only so much we can do at this stage,
// if we want to avoid tests overlapping.
function test_cycles(size, tc) {
  // Now, restart this with unreferenced cycles
  for (i = 0; i < size/2; ++i) {
    let a = {
      a: ctypes.CDataFinalizer(tc.acquire(i*2), tc.release),
      b: {
        b: ctypes.CDataFinalizer(tc.acquire(i*2+1), tc.release)
      }
    };
    a.b.a = a;
  }
  do_test_pending();

  Components.utils.schedulePreciseGC(
    function() {
      // Check that _something_ has been finalized
      do_check_true(count_finalized(size, tc) > 0);
      do_test_finished();
    }
  );

  do_timeout(10000, do_throw);
}


function count_finalized(size, tc) {
  let finalizedItems = 0;
  for (let i = 0; i < size; ++i) {
    if (!tc.status(i)) {
      ++finalizedItems;
    }
  }
  return finalizedItems;
}

/**
 * Test:
 * - that (some) finalizers are executed;
 * - that no finalizer is executed twice (this is done on the C side).
 */
function test_executing_finalizers(size, tc)
{
  dump("test_executing_finalizers\n");
  // Allocate |size| items without references
  for (let i = 0; i < size; ++i) {
    ctypes.CDataFinalizer(tc.acquire(i), tc.release);
  }
  trigger_gc(); // This should trigger some finalizations, hopefully all

  // Check that _something_ has been finalized
  do_check_true(count_finalized(size, tc) > 0);
}


/**
 * Check that
 * - |dispose| is executed properly
 * - finalizers are not executed after |dispose|
 */
function test_executing_dispose(size, tc)
{
  dump("test_executing_dispose\n");
  let ref = [];
  // Allocate |size| items with references
  for (let i = 0; i < size; ++i) {
    ref.push(ctypes.CDataFinalizer(tc.acquire(i), tc.release));
  }
  do_check_eq(count_finalized(size, tc), 0);

  // Dispose of everything and make sure that everything has been cleaned up
  ref.forEach(
    function(v) {
      v.dispose();
    }
  );
  do_check_eq(count_finalized(size, tc), size);

  // Remove references
  ref = [];

  // Re-acquireialize data and make sure that everything has been reinialized
  for (i = 0; i < size; ++i) {
    tc.acquire(i);
  }

  do_check_eq(count_finalized(size, tc), 0);


  // Attempt to trigger finalizations, ensure that they do not take place
  trigger_gc();

  do_check_eq(count_finalized(size, tc), 0);
}


/**
 * Check that
 * - |forget| does not dispose
 * - |forget| has the right content
 * - finalizers are not executed after |forget|
 */
function test_executing_forget(size, tc)
{
  dump("test_executing_forget\n");
  let ref = [];
  // Allocate |size| items with references
  for (let i = 0; i < size; ++i) {
    let original = tc.acquire(i);
    let finalizer = ctypes.CDataFinalizer(original, tc.release);
    ref.push(
      {
        original: original,
        finalizer: finalizer
      }
    );
    do_check_true(tc.compare(original, finalizer));
  }
  do_check_eq(count_finalized(size, tc), 0);

  // Forget everything, making sure that we recover the original info
  ref.forEach(
    function(v) {
      let original  = v.original;
      let recovered = v.finalizer.forget();
      // Note: Cannot use do_check_eq on Uint64 et al.
      do_check_true(tc.compare(original, recovered));
      do_check_eq(original.constructor, recovered.constructor);
    }
  );

  // Also make sure that we have not performed any clean up
  do_check_eq(count_finalized(size, tc), 0);

  // Remove references
  ref = [];

  // Attempt to trigger finalizations, ensure that they have no effect
  trigger_gc();

  do_check_eq(count_finalized(size, tc), 0);
}


/**
 * Check that finalizers are not executed
 */
function test_do_not_execute_finalizers_on_referenced_stuff(size, tc)
{
  dump("test_do_not_execute_finalizers_on_referenced_stuff\n");

  let ref = [];
  // Allocate |size| items without references
  for (let i = 0; i < size; ++i) {
    ref.push(ctypes.CDataFinalizer(tc.acquire(i), tc.release));
  }
  trigger_gc(); // This might trigger some finalizations, but it should not

  // Check that _nothing_ has been finalized
  do_check_eq(count_finalized(size, tc), 0);

  // Clean up manually, lest leftover data interferes with following tests
  ref.forEach(function(v) {
    v.dispose();
  });
  ref = [];
  trigger_gc();
}

