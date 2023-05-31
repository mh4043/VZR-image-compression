#!/bin/sh
# use: srun --reservation=fri --mpi=pmix -n<number of processes> -N<number of nodes> mpi_knn <path to image> <number of clusters> <number of iterations>
module load OpenMPI/4.1.0-GCC-10.2.0
mpicc mpi_knn.c -lm -o mpi_knn
srun --reservation=fri --mpi=pmix -n32 -N1 mpi_knn ../images/living_room.png 32 100 >> mpi_kmeans_results.txt
