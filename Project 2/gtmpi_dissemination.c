#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdint.h>
#include "gtmpi.h"

/*
    From the MCS Paper: The scalable, distributed dissemination barrier with only local spinning.

    type flags = record
        myflags : array [0..1] of array [0..LogP - 1] of Boolean
		partnerflags : array [0..1] of array [0..LogP - 1] of ^Boolean
	
    processor private parity : integer := 0
    processor private sense : Boolean := true
    processor private localflags : ^flags

    shared allnodes : array [0..P-1] of flags
    //allnodes[i] is allocated in shared memory
	//locally accessible to processor i

    //on processor i, localflags points to allnodes[i]
    //initially allnodes[i].myflags[r][k] is false for all i, r, k
    //if j = (i+2^k) mod P, then for r = 0 , 1:
    //    allnodes[i].partnerflags[r][k] points to allnodes[j].myflags[r][k]

    procedure dissemination_barrier
        for instance : integer :0 to LogP-1
	    	localflags^.partnerflags[parity][instance]^ := sense
	    	repeat until localflags^.myflags[parity][instance] = sense
		if parity = 1
	    	sense := not sense
		parity := 1 - parity

	Note that your should only use MPI's point-to-point communication procedures 
	(i.e. MPI_Isend, MPI_Irecv, MPI_Send, MPI_Recv, NOT MPI_BCast, MPI_Gather, etc.)
*/

static int num_procs;
static int num_rounds;

void gtmpi_init(int num_threads){
	num_procs = num_threads;
	num_rounds = (int) ceil(log2(num_procs));
}

void gtmpi_barrier(){
	int my_id;
  	MPI_Comm_rank(MPI_COMM_WORLD, &my_id);

  	// int parity = 0;
  	int sense = 1; 

  	int i, next, prev;
  	//int buf[num_procs];

  	MPI_Request reqs[num_procs];
  	MPI_Status stat;

  	for (i=0; i < num_rounds; i++){
  		// i is the round
  		// my_id is the processor 
  		next = (my_id + (int) pow(2, i)) % num_procs;
  		prev = (my_id +num_procs - (int) pow(2, i)) % num_procs; 

  		MPI_Isend(&my_id, 1, MPI_INT, next, 0, MPI_COMM_WORLD, &reqs[my_id]);
  		// MPI_Send(&sense, 1, MPI_INT, next, 0, MPI_COMM_WORLD);
  		// MPI_Irecv(&buf[num_procs], 1, MPI_INT, prev, 0, MPI_COMM_WORLD, &reqs[my_id]);
  		MPI_Recv(&sense, 1, MPI_INT, prev, 0 ,MPI_COMM_WORLD, &stat);
  	}

}

void gtmpi_finalize(){

}
