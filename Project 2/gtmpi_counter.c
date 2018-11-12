#include <stdlib.h>
#include <mpi.h>
#include <stdio.h>
#include "gtmpi.h"

/*
    From the MCS Paper: A sense-reversing centralized barrier

    shared count : integer := P
    shared sense : Boolean := true
    processor private local_sense : Boolean := true

    procedure central_barrier
        local_sense := not local_sense // each processor toggles its own sense
	if fetch_and_decrement (&count) = 1
	    count := P
	    sense := local_sense // last processor toggles global sense
        else
           repeat until sense = local_sense
*/


static MPI_Request req;
static int P;

void gtmpi_init(int num_threads){
  P = num_threads;
  // printf("Num Threads: %d\n", P);
  // status_array = (MPI_Status*) malloc((P - 1) * sizeof(MPI_Status));
  // I dont need to waste memory
}

void gtmpi_barrier(){
  int vpid, i;

  static MPI_Status stats;

  MPI_Comm_rank(MPI_COMM_WORLD, &vpid);

  // Use MPI_Isend/MPI_Irecv what about sendrecv?
  // turn a single node into a bottleneck this should reduce network contention
  
  if (vpid == 0) {
    // block on this bottleneck node then let everyone know that everyone reached
    for(i = 1; i < P; i++)
      MPI_Recv(NULL, 0, MPI_INT, i, 1, MPI_COMM_WORLD, &stats);
    for(i = vpid + 1; i < P; i++)
      MPI_Isend(NULL, 0, MPI_INT, i, 1, MPI_COMM_WORLD, &req);

  } else {
    // notify the bottleneck node that it reached the barrier
    MPI_Isend(NULL, 0, MPI_INT, 0, 1, MPI_COMM_WORLD, &req);
    MPI_Recv(NULL, 0, MPI_INT, 0, 1, MPI_COMM_WORLD, &stats);
  }
}

void gtmpi_finalize(){
}

