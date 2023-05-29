#!/bin/sh

module load OpenMPI/4.1.0-GCC-10.2.0
mpicc mpi_knn.c -lm -o mpi_knn
srun --reservation=fri --mpi=pmix -n4 -N1 mpi_knn ../images/lena.png 32 1