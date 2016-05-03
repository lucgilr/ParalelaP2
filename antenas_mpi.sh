#!/bin/bash
#$HOME/mpich2/bin/mpiexec -n $1 xterm -e ./antenas_mpi $2 $3 $4 $5 $6 $7
$HOME/mpich2/bin/mpiexec -n $1 ./antenas_mpi $2 $3 $4 $5 $6 $7
