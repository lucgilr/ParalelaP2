

FLAGS=-std=c11 -O3
#FLAGS=-std=c11 -O0 -g -pg
LIBS=$(HOME)/mpich2/bin

antenas: antenas.c cputils.h
	gcc $(FLAGS)  antenas.c   -o antenas 

antenas_mpi: antenas_mpi.c cputils.h
	$(LIBS)/mpicc antenas_mpi.c -o antenas_mpi $(FLAGS)
