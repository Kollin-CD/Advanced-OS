#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdint.h>
#include "gtmpi.h"

/*
    From the MCS Paper: A scalable, distributed tournament barrier with only local spinning

    type round_t = record
        role : (winner, loser, bye, champion, dropout)
	opponent : ^Boolean
	flag : Boolean
    shared rounds : array [0..P-1][0..LogP] of round_t
        // row vpid of rounds is allocated in shared memory
	// locally accessible to processor vpid

    processor private sense : Boolean := true
    processor private vpid : integer // a unique virtual processor index

    //initially
    //    rounds[i][k].flag = false for all i,k
    //rounds[i][k].role = 
    //    winner if k > 0, i mod 2^k = 0, i + 2^(k-1) < P , and 2^k < P
    //    bye if k > 0, i mode 2^k = 0, and i + 2^(k-1) >= P
    //    loser if k > 0 and i mode 2^k = 2^(k-1)
    //    champion if k > 0, i = 0, and 2^k >= P
    //    dropout if k = 0
    //    unused otherwise; value immaterial
    //rounds[i][k].opponent points to 
    //    round[i-2^(k-1)][k].flag if rounds[i][k].role = loser
    //    round[i+2^(k-1)][k].flag if rounds[i][k].role = winner or champion
    //    unused otherwise; value immaterial
    procedure tournament_barrier
        round : integer := 1
	loop   //arrival
	    case rounds[vpid][round].role of
	        loser:
	            rounds[vpid][round].opponent^ :=  sense
		    repeat until rounds[vpid][round].flag = sense
		    exit loop
   	        winner:
	            repeat until rounds[vpid][round].flag = sense
		bye:  //do nothing
		champion:
	            repeat until rounds[vpid][round].flag = sense
		    rounds[vpid][round].opponent^ := sense
		    exit loop
		dropout: // impossible
	    round := round + 1
	loop  // wakeup
	    round := round - 1
	    case rounds[vpid][round].role of
	        loser: // impossible
		winner:
		    rounds[vpid[round].opponent^ := sense
		bye: // do nothing
		champion: // impossible
		dropout:
		    exit loop
	sense := not sense
*/

static int P;
static int num_rounds;
static MPI_Request req;

void gtmpi_init(int num_threads){
	P = num_threads;
	num_rounds = (int) ceil(log2(P));

	if (num_rounds == 0 || P % 2 != 0) {
  		printf("Nodes/Processors must be a multiple of 2\n");
  		return;
  	}
}

void gtmpi_barrier(){
	int my_id, round, step, i, compete;
	int destep = 0;

	MPI_Comm_rank(MPI_COMM_WORLD, &my_id);

	static MPI_Status stats;

  	if (num_rounds == 0 || P % 2 != 0) {
  		printf("Nodes/Processors must be a multiple of 2\n");
  		return;
  	}

  	if(my_id % 2) {
  		compete = 0;
  	} else {
  		compete = 1;
  	}

  	for (round = 0; round < num_rounds; round++) {
  		printf("Round %d, Thread %d\n", round, my_id);
  		step = pow(2, round);
  		// P0 always wins the tournament
	  	if (my_id == 0) {
	  		// Block then basically broadcast
	  		printf("Thread %d waiting to receive 1 from thread %d\n", my_id, my_id+step);
	  		MPI_Recv(NULL, 0, MPI_INT, my_id + step, 1, MPI_COMM_WORLD, &stats);

	  		if (num_rounds - 1 == round) {
	  			for(i = pow(2, round); i > 0; i>>=1) {
	  				printf("Round: %d Thread %d sending 1 signal to thread %d\n", round, my_id, i);
	      			MPI_Isend(NULL, 0, MPI_INT, i, 1, MPI_COMM_WORLD, &req);	
	  			}

	  			// break;
	  				
	  		}

	  	} else {
	  		
	  		// First round notify processor - 1
	  		// Second round notify processor - 2
	  		// Third round notify processor - 4

	  		
	  		// Spin on waiting from the loser
	  		if ( (my_id % (step+1)) == 0) {
	  			printf("Thread %d waiting to receive 2 from %d\n", my_id, (my_id+step) % P);
	  			if (my_id + step < P)
	  				MPI_Recv(NULL, 0, MPI_INT, (my_id + step) % P, 1, MPI_COMM_WORLD, &stats);
	  			destep = pow(2, round + 1);
	  		}

	  		// Notify winner
	  		if (destep == 0){
	  			if (my_id - step >= 0) {
	  				printf("Thread %d sending signal 2 to thread %d\n", my_id, my_id-step);
			  		MPI_Isend(NULL, 0, MPI_INT, my_id - step, 1, MPI_COMM_WORLD, &req);
				  		// Block to wait to hear from the winner
		  			printf("Thread %d waiting to receive 3 from thread %d\n", my_id, my_id-step);
		  			MPI_Recv(NULL, 0, MPI_INT, my_id - step, 1, MPI_COMM_WORLD, &stats);
	  			}
	  		} else {
	  			printf("Thread %d sending signal 3 to thread %d\n", my_id, my_id-destep);
		  		MPI_Isend(NULL, 0, MPI_INT, my_id - destep, 1, MPI_COMM_WORLD, &req);
		  		// Block to wait to hear from the winner
		  		printf("Thread %d waiting to receive 4 from thread %d\n", my_id, my_id-destep);
		  		MPI_Recv(NULL, 0, MPI_INT, my_id - destep, 1, MPI_COMM_WORLD, &stats);
	  		}
  		}
  	}

  	for (round = num_rounds - 1; round >=0; round--){
  		printf("New Round %d, Thread %d\n", round, my_id);
  	}
}

void gtmpi_finalize(){

}
