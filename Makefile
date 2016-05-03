

FLAGS=-std=c11 -O3
#FLAGS=-std=c11 -O0 -g -pg
LIBS=$(HOME)/mpich2/bin
all: antenas

antenas_mpi: antenas_mpi.c cputils.h
	$(LIBS)/mpicc antenas_mpi.c -o antenas_mpi $(FLAGS)

clean:
	rm -f antenas *.o *~
