#!/bin/sh

module load OpenMPI/4.1.0-GCC-10.2.0
mpicc mpi_knn.c -lm -o mpi_knn

# Part 1: Varying K and fixed parameters:
echo "Initiating tests with varying K with fixed parameters (MAX_ITERS=100, TASKS=64)" > mpi_kmeans_results_part1.txt
MAX_ITERS=100
TASKS=64

# Iterate K from 8 to 128
for k in 8 16 32 64 128
do
    srun --reservation=fri --mpi=pmix -n$TASKS -N1 mpi_knn ../images/living_room.png $k $MAX_ITERS >> mpi_kmeans_results_part1.txt
    echo "" >> mpi_kmeans_results_part1.txt
done