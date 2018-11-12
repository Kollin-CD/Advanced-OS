# CS6210: Advanced OS - Testing framework for Barrier algorithms (Project 2) #

This is a testing harness for OMSCS CS6210: Advanced Operating Systems, Project 2 (Barriers).

The skeleton.c files are used to build test executables for your barrier implementations.

## Building ##

Update the Makefile to make the variable `PATH_TO_PROJECT2` point to the location of
your project2 code repo, and then run `make`.

To include debugging information, run `make dev` instead.

## Testing OpenMP Implementations ##

Simply run the test executable, e.g. `./counter_openmp`

## Testing MPI Implementations ##

Run something equivalent to the following:

```bash
NUMPROCS=32
mpirun -np ${NUMPROCS} ./counter_mpi ${NUMPROCS}
```

_NOTE:_ it is important that the number of processes passed to `mpirun` be equivalent to the number passed to your testing executable. If those numbers do not match, your test will abort.