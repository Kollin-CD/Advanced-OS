#include <omp.h>
#include "gtmp.h"

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

static int n_threads;
int count;
int shared_sense; 

void gtmp_init(int num_threads){
	n_threads = num_threads;
	count = num_threads; // shared count : integer := P
	shared_sense = 1; // shared sense : Boolean := true
}

void gtmp_barrier(){
	int mytid = omp_get_thread_num(); // Thread ID for debugging
	int local_sense = !shared_sense; // Toggle the sense based on processor

	if(__sync_fetch_and_sub(&count, 1) == 1) {
		// use omp_get_thread_num again? and critical?
		count = omp_get_num_threads();
		shared_sense = local_sense;
	} else {
		// use volatile?
		while (shared_sense != local_sense);
	}

}

void gtmp_finalize(){
	// Nothing to free
}
