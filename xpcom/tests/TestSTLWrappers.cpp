#include <stdio.h>

#include <algorithm>
#ifndef mozilla_algorithm_h
#  error "failed to wrap <algorithm>"
#endif

#include <vector>
#ifndef mozilla_vector_h
#  error "failed to wrap <vector>"
#endif

// gcc errors out if we |try ... catch| with -fno-exceptions, but we
// can still test on windows
#ifdef XP_WIN
#  define TRY       try
#  define CATCH(e)  catch (e)
#else
#  define TRY
#  define CATCH(e)  if (0)
#endif

int main() {
    std::vector<int> v;
    int rv = 1;

    TRY {
      // v.at(1) on empty v should abort; NOT throw an exception

      // (Do some arithmetic with result of v.at() to avoid
      // compiler warnings for unused variable/result.)
      rv += v.at(1) ? 1 : 2;
    } CATCH(const std::out_of_range& e) {
      fputs("TEST-FAIL | TestSTLWrappers.cpp | caught an exception?\n",
            stderr);
      return 1;
    }

    fputs("TEST-FAIL | TestSTLWrappers.cpp | didn't abort()?\n",
          stderr);
    return rv;
}
