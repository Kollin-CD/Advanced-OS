#include <stdio.h>
#include <sys/utsname.h>
#include <mpi.h>

////////////////////////////////////////////////////////////
// Simplified Debug Macros
////////////////////////////////////////////////////////////
#include <stdio.h>  /* fprintf() */
#include <errno.h>  /* errno */
#include <string.h> /* strerror() */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <unistd.h>   /* usleep() */

#define FUNC_SUCCESS (0)
#define FUNC_FAILURE (-1)
#define _TRACE_   __FILE__, __func__, __LINE__
#define clean_strerror() (errno == 0 ? "None" : strerror(errno))
#define log_err(MSG, ...) fprintf(stderr, "[ERROR] (%s:%s:%d: errno: %s) " MSG "\n", _TRACE_, clean_strerror(), ##__VA_ARGS__)
#define log_warn(MSG, ...) fprintf(stderr, "[WARN] (%s:%s:%d: errno: %s) " MSG "\n", _TRACE_, clean_strerror(), ##__VA_ARGS__)
#define log_info(MSG, ...) fprintf(stderr, "[INFO] (%s:%s:%d) " MSG "\n", _TRACE_, ##__VA_ARGS__)
#define enforce(ASSERT_COND, MSG, ...) if(!(ASSERT_COND)) { log_err(MSG, ##__VA_ARGS__); exit(EXIT_FAILURE); }
#define enforce_mem(MEM_PTR) enforce((MEM_PTR), "Out of memory.")
#define __safefree(PTR, FREE_FUNC, ...) if((PTR)) { (*(FREE_FUNC))((void *)(PTR)); (PTR) = NULL; }
#define safefree(PTR, ...) __safefree((PTR), ##__VA_ARGS__, free )

#ifdef _DEBUG_MODE
# define debug(MSG, ...) fprintf(stderr, "[DEBUG] (%s:%s:%d) " MSG "\n", _TRACE_, ##__VA_ARGS__)
#else
# define debug(MSG, ...)
#endif /* _DEBUG_MODE */
//////////////////////////////////////////////////////////////
// End Debug Macros
//////////////////////////////////////////////////////////////

/* function prototypes */
extern void gtmpi_init(int num_threads);
extern void gtmpi_barrier();
extern void gtmpi_finalize();
static int strton(long *retval, char *numstr, int base);

#define END_TAG (9999)


int main(int argc, char **argv)
{
    int world_size, pid;
    long num_processes;
    int rounds = 1000;
    int send_to, recv_from;
    int recvd_end_msg;
    MPI_Request end_checker;

    /* parse num_processes */
    enforce(argc == 2, "This program takes exactly one argument: NUM_PROCESSES (an integer)");
    enforce(strton(&num_processes, argv[1], 10) == FUNC_SUCCESS,
        "Failed to parse NUM_PROCESSES command-line argument");

    /* Initialize */
    gtmpi_init(num_processes);
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);

    /* check num processes in world matches what we expect */
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    enforce(world_size == num_processes, "Mismatch between number of processes:\n\t"
        "World_size=%d, num_processors=%ld", world_size, num_processes);

    if (pid == 0)
        log_info("RUNNING mpi test: num_processes=%d", world_size);

    /* set up ring topo for passing end-messages */
    send_to = (pid + 1) % num_processes;
    recv_from = (pid + num_processes - 1) % num_processes;

    /* do tests */
    for (int r = 0; r < rounds; r++) {
        MPI_Irecv(NULL, 0, MPI_INT, recv_from, END_TAG, MPI_COMM_WORLD, &end_checker);

        /* check varying number of barriers (based on round number) */
        for (int b = 0; b <= r % 10; b++) {
            MPI_Test(&end_checker, &recvd_end_msg, MPI_STATUS_IGNORE);
            enforce(!recvd_end_msg, "PID %d detected Barrier Breakout by PID %d!", pid, recv_from);
            gtmpi_barrier();
        }

        /* send/receive ending message */
        MPI_Send(NULL, 0, MPI_INT, send_to, END_TAG, MPI_COMM_WORLD);
        MPI_Wait(&end_checker, MPI_STATUS_IGNORE);
    }

    /* Finalize */
    MPI_Finalize();
    gtmpi_finalize();

    if (pid == 0) log_info("+++++++ Completed Successfully +++++++");
    return EXIT_SUCCESS;
}

/* Wraps strtol. Stores converted long int in retval.
 * Returns:
 *   0 = success
 *   <num chars parsed> = partial failure (some chars parsed)
 *   -1 = complete failure (nothing parsed)
 */
static int strton(long *retval, char *numstr, int base)
{
    char *endptr;
    int chars_parsed;

    if (numstr && *numstr != '\0') {
        *retval = strtol(numstr, &endptr, base);
        if (*endptr == '\0') {
            /* all chars parsed */
            return FUNC_SUCCESS;
        } else {
            /* only some chars parsed */
            chars_parsed = endptr - numstr;
            return chars_parsed ? chars_parsed : FUNC_FAILURE;
        }
    } else {
        /* total failure, nothing parsed */
        return FUNC_FAILURE;
    }
}
