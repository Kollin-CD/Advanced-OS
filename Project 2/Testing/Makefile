# Set this to where the project2 code is synced
# Please end path with a "/"
# For instance:
# PATH_TO_PROJECT2 = ../ # works great if you add this repo as a submodule!
# PATH_TO_PROJECT2 = /path/to/oms-aos-barrier-proj2/
PATH_TO_PROJECT2 = ../

CC = gcc
CPPFLAGS += -MMD -MP # enables automatic dependency tracking
CFLAGS += -Wall -Wextra -I$(PATH_TO_PROJECT2) -std=gnu11
LDLIBS =

COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
COMPILE_NOLINK = $(CC) -c $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@

#OpenMP Flags Etc.
OMPFLAGS = -fopenmp -DLEVEL1_DCACHE_LINESIZE=`getconf LEVEL1_DCACHE_LINESIZE`
OMPLIBS = -lgomp

#MPI Flags Etc (may need to customize)
MPICC = mpicc
#MPICC = /opt/openmpi-1.4.3-gcc44/bin/mpicc
#MPICH = /usr/lib64/openmpi/1.4-gcc
MPIFLAGS = #-I$(MPICH)/include
MPILIBS =


.PHONY: all
all: hello_openmp hello_mpi counter_openmp mcs_openmp tree_openmp counter_mpi \
	tournament_mpi dissemination_mpi


.PHONY: dev
dev: CFLAGS += -g -D_DEBUG_MODE -fsanitize=address -fno-omit-frame-pointer
dev: all


%_openmp: CFLAGS += $(OMPFLAGS)
%_openmp: LDLIBS += $(OMPLIBS)

%_mpi: CC = $(MPICC)
%_mpi: CFLAGS += $(MPIFLAGS)
%_mpi: LDLIBS += $(MPILIBS)
# MPI doesn't play well with Address Sanitizer, so remove it:
%_mpi: CFLAGS := $(filter-out -fsanitize=address,$(CFLAGS))
%_mpi: CFLAGS := $(filter-out -fno-omit-frame-pointer,$(CFLAGS))

hello_openmp: $(PATH_TO_PROJECT2)hello_openmp.c
	$(COMPILE)

hello_mpi: $(PATH_TO_PROJECT2)hello_mpi.c
	$(COMPILE)

counter_openmp: openmp_harness.o gtmp_counter.o
	$(COMPILE)

mcs_openmp: openmp_harness.o gtmp_mcs.o
	$(COMPILE)

tree_openmp: openmp_harness.o gtmp_tree.o
	$(COMPILE)

tree_slow_openmp: openmp_harness.o gtmp_tree_slow.o
	$(COMPILE)

counter_mpi: mpi_harness.o gtmpi_counter.o
	$(COMPILE)

counter_slow_mpi: mpi_harness.o gtmpi_counter_orig.o
	$(COMPILE)

dissemination_mpi: mpi_harness.o gtmpi_dissemination.o
	$(COMPILE)

tournament_mpi: mpi_harness.o gtmpi_tournament.o
	$(COMPILE)

%.o: $(PATH_TO_PROJECT2)%.c %.d
	$(COMPILE_NOLINK)


# Empty pattern rule to match dependency (*.d) files (i.e. makefiles),
# so make won't fail if dependency doesn't exist
%.d: ;

# Mark dependency files as precious so they won't be deleted as intermediates
.PRECIOUS: %.d

# The list of all source files I want to track dependencies for
SOURCES=$(wildcard *.c)

# Include any dependency files that exist, and
# suppress error message for ones that don't yet (via hyphen)
-include $(SOURCES:.c=.d)

.PHONY: clean
clean:
	rm -rf *.o *.d *_openmp *_mpi
