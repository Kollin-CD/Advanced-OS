#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include "gtmp.h"
#include <math.h>

/*
    From the MCS Paper: A scalable, distributed tree-based barrier with only local spinning.

    type treenode = record
        parentsense : Boolean
        parentpointer : ^Boolean
	    childpointers : array [0..1] of ^Boolean
	    havechild : array [0..3] of Boolean
	    childnotready : array [0..3] of Boolean
	    dummy : Boolean //pseudo-data

    shared nodes : array [0..P-1] of treenode
        // nodes[vpid] is allocated in shared memory
        // locally accessible to processor vpid
    processor private vpid : integer // a unique virtual processor index
    processor private sense : Boolean

    // on processor i, sense is initially true
    // in nodes[i]:
    //    havechild[j] = true if 4 * i + j + 1 < P; otherwise false
    //    parentpointer = &nodes[floor((i-1)/4].childnotready[(i-1) mod 4],
    //        or dummy if i = 0
    //    childpointers[0] = &nodes[2*i+1].parentsense, or &dummy if 2*i+1 >= P
    //    childpointers[1] = &nodes[2*i+2].parentsense, or &dummy if 2*i+2 >= P
    //    initially childnotready = havechild and parentsense = false
	
    procedure tree_barrier
        with nodes[vpid] do
	    repeat until childnotready = {false, false, false, false}
	    childnotready := havechild //prepare for next barrier
	    parentpointer^ := false //let parent know I'm ready
	    // if not root, wait until my parent signals wakeup
	    if vpid != 0
	        repeat until parentsense = sense
	    // signal children in wakeup tree
	    childpointers[0]^ := sense
	    childpointers[1]^ := sense
	    sense := not sense
*/

typedef struct _treenode_t{
    int parentsense;
    int *parentpointer;
    int *childpointers[2];
    int havechild[4];
    int childnotready[4];
    int dummy; // pseudo-data
    int vpid; // a unique virtual processor index move?
    int sense;
} treenode_t;

static treenode_t *shared_nodes;
// nodes[vpid] is allocated in shared mem
// locally accessible to processor vpid

void gtmp_init(int num_threads){
    int num_nodes = num_threads;

    // Set up the tree
    shared_nodes = (treenode_t *) malloc(num_nodes * sizeof(treenode_t));

    int i, j;
    for(i=0; i < num_nodes; i++) {
        // Loop through processors
        // currnode = _gtmp_get_treenode(i);
        shared_nodes[i].dummy = 0; // pseudo-data
        shared_nodes[i].parentsense = 0;  // initially parentsense = false
        shared_nodes[i].sense = 1; // Sense is initially true for each processor

        for (j=0; j<4; j++) {
            if (4 * i + j + 1 < num_nodes) {
                shared_nodes[i].havechild[j] = 1;   
            } else {
                shared_nodes[i].havechild[j] = 0;
            }
            // initially childnotready = havechild - memcpy?
            shared_nodes[i].childnotready[j] = shared_nodes[i].havechild[j];
        }
        if (i==0) {
            shared_nodes[i].parentpointer = &(shared_nodes[i].dummy);
        } else {
            // printf("Reaches Here");
            shared_nodes[i].parentpointer = 
            &(shared_nodes[(int) floor((i-1)/4)].childnotready[(i-1)%4]);
        }
        for (int k=0; k < 2; k++) {

            if ((2*i+k+1) >= num_nodes) {
                shared_nodes[i].childpointers[k] = &(shared_nodes[i].dummy);
                // printf("Value of Child %d = %d\n", *(shared_nodes[i].childpointers[k]), k); 
            } else {
                shared_nodes[i].childpointers[k] = &(shared_nodes[2*i+k+1].parentsense);
                // printf("Value of *ptr = %d\n", *(shared_nodes[i].childpointers[k])); 
            }
        }
    }
}
/*
    procedure tree_barrier
        with nodes[vpid] do
        repeat until childnotready = {false, false, false, false}
        childnotready := havechild //prepare for next barrier
        parentpointer^ := false //let parent know I'm ready
        // if not root, wait until my parent signals wakeup
        if vpid != 0
            repeat until parentsense = sense
        // signal children in wakeup tree
        childpointers[0]^ := sense
        childpointers[1]^ := sense
        sense := not sense
*/
void gtmp_barrier(){
    int mytid = omp_get_thread_num();

    // repeat until childnotready = {false, false, false, false}
    while (shared_nodes[mytid].childnotready[0] || 
        shared_nodes[mytid].childnotready[1] ||
        shared_nodes[mytid].childnotready[2] ||
        shared_nodes[mytid].childnotready[3]);

    // printf("Thread %d has all its children ready\n", mytid);
    
    // childnotready := havechild //prepare for next barrier
    for(int i=0; i < 4; i++){
        shared_nodes[mytid].childnotready[i] = shared_nodes[mytid].havechild[i];
    }

    // printf("thread[%d] notify parent I'm ready\n", mytid);
    *(shared_nodes[mytid].parentpointer) = 0; // notify parent
    // if not root, wait until my parent signals wakeup
    if (mytid != 0) {
        // printf("thread[%d] wait for signal\n", mytid);
        while (shared_nodes[mytid].parentsense != shared_nodes[mytid].sense);
    }
    // signal children in wakeup tree
    // printf("thread[%d] wakes up child thread[%d]\n", mytid, 2*mytid);
    // printf("Value of *ptr = %d\n", *(shared_nodes[mytid].childpointers[0])); 
    *(shared_nodes[mytid].childpointers[0]) = shared_nodes[mytid].sense;
    // printf("thread[%d] wakes up child thread[%d]\n", mytid, 2*mytid+1);
    *(shared_nodes[mytid].childpointers[1]) = shared_nodes[mytid].sense;
    shared_nodes[mytid].sense = !shared_nodes[mytid].sense;
}

void gtmp_finalize() {
    free(shared_nodes);
}
