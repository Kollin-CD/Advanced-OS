////////////////////////////////////////////////////////////
// Simplified Debug Macros
////////////////////////////////////////////////////////////
#include <stdio.h>  /* fprintf() */
#include <errno.h>  /* errno */
#include <string.h> /* strerror() */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <unistd.h> /* usleep() */
#include <time.h>   /* clock() */

#define FUNC_SUCCESS (0)
#define FUNC_FAILURE (-1)
#define STRINGIFY(X) #X
#define _TRACE_   __FILE__, __func__, __LINE__
#define clean_strerror() (errno == 0 ? "None" : strerror(errno))
#define log_err(MSG, ...) fprintf(stderr, "[ERROR] (%s:%s:%d: errno: %s) " MSG "\n", _TRACE_, clean_strerror(), ##__VA_ARGS__)
#define log_warn(MSG, ...) fprintf(stderr, "[WARN] (%s:%s:%d: errno: %s) " MSG "\n", _TRACE_, clean_strerror(), ##__VA_ARGS__)
#define log_info(MSG, ...) fprintf(stderr, "[INFO] (%s:%s:%d) " MSG "\n", _TRACE_, ##__VA_ARGS__)
#define enforce(ASSERT_COND, MSG, ...) if(!(ASSERT_COND)) { log_err(MSG, ##__VA_ARGS__); exit(EXIT_FAILURE); }
#define enforce_mem(MEM_PTR) enforce((MEM_PTR), "Out of memory.")
#define __safefree(PTR, FREE_FUNC, ...) if((PTR)) { (*(FREE_FUNC))((void *)(PTR)); (PTR) = NULL; }
#define safefree(PTR, ...) __safefree((PTR), ##__VA_ARGS__, free )
#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR);

#ifdef _DEBUG_MODE
# define debug(MSG, ...) fprintf(stderr, "[DEBUG] (%s:%s:%d) " MSG "\n", _TRACE_, ##__VA_ARGS__)
#else
# define debug(MSG, ...)
#endif /* _DEBUG_MODE */

# define concr_jitter() NO_EINTR(usleep((int)(((double)(random())/(double)(RAND_MAX * 0.9)) * 10000)))
#define log_time(FUNC, ...) do { clock_t start=clock(); \
                            (*(FUNC))(__VA_ARGS__); \
                            clock_t end=clock(); \
                            double elapsed = (double)(end-start)*1000.0/CLOCKS_PER_SEC; \
                            log_info("%s() took %.3f ms to run", STRINGIFY(FUNC), elapsed); } while (0)
//////////////////////////////////////////////////////////////
// End Debug Macros
//////////////////////////////////////////////////////////////

#ifndef LEVEL1_DCACHE_LINESIZE
#define LEVEL1_DCACHE_LINESIZE 8
#endif

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <omp.h>
#include "gtmp.h"

static int do_stuff(int iters);
static void do_test(int num_threads, int sets, int rounds);

static int volatile add_jitter = 0;
static int volatile test_number = 1;

int main(int argc, char **argv)
{
  (void) argc;
  (void) argv;

  omp_set_dynamic(0);
  if (omp_get_dynamic())
    log_warn("Dynamic adjustment of threads has been set");

  debug("LEVEL1_DCACHE_LINESIZE=%d", LEVEL1_DCACHE_LINESIZE);

  /* unit test */
  do_test(1, 1, 1);

  /* multiple init()/finalize() sets, one thread */
  do_test(1, 2, 1);

  /* even # of threads, multiple sets*/
  do_test(2, 2, 1);

  /* one thread, many rounds */
  log_time(do_test, 1, 1, 10000);

  /* even number of threads */
  log_time(do_test, 2, 1, 10000);

  /* prime number of threads */
  log_time(do_test, 3, 1, 10000);

  /* testing 4-core machine */
  log_time(do_test, 4, 1, 10000);

  /* testing 8-core machine */
  // log_time(do_test, 8, 1, 10000);

  /* LARGE number of threads */
  log_time(do_test, 37, 1, 1);

  add_jitter = 1;
  log_info("---- Adding random delays to threads");

  /* even # with jitter */
  do_test(2, 1, 100);

  /* prime with jitter */
  do_test(3, 1, 100);

  log_info("+++++++ COMPLETED SUCCESSFULLY +++++++++");

  return 0;
}


static int do_stuff(int iters) {
  /* sums all integers in range 1..iters */
  int total = 0;
  int i;

  for (i = 1; i <= iters; i++) {
    total += i;
  }

  return total;
}


static void do_test(int num_threads, int sets, int rounds) {
  /* 'rounds' means the number of times we check for barrier breakout
     'sets' means how many times we do the sequence init..breakout-check..finalize */
  int s, iters = 1000;
  int expected = (iters / 2) * (1 + iters);
  int *totals = calloc(num_threads, sizeof(int));
  enforce_mem(totals);

  omp_set_num_threads(num_threads);

  log_info("Test[%d]: threads=%d, sets=%d, rounds=%d", test_number++,
    num_threads, sets, rounds);

  for (s = 0; s < sets; s++) {
    gtmp_init(num_threads);
    #pragma omp parallel
    {
      int tid = omp_get_thread_num();
      int i, rnd, total = 0;

      for (rnd = 1; rnd <= rounds; rnd++) {
        total = do_stuff(iters);

        if (add_jitter) {
          concr_jitter();
        }

        totals[tid] = total;

        gtmp_barrier();

        #pragma omp single
        for (i = 0; i < num_threads; i++) {
          enforce(totals[i] == expected, "Detected Barrier Breakout! round=%d", rnd);
          totals[i] = 0;
        }
        gtmp_barrier();
      }
    } // implied barrier
    gtmp_finalize();
  }

  safefree(totals);
}
