#!/bin/sh

module load OpenMPI/4.1.0-GCC-10.2.0
mpicc mpi_knn.c -lm -o mpi_knn

# Part 2: Fixed K and varying parameters:
echo "Initiating tests with fixed K with varying parameters (MAX_ITERS=100, TASKS={64, 32, 16, 8, 4, 2})" > mpi_kmeans_results_part2.txt
MAX_ITERS=100
K=32

# Iterate TASKS from 64 to 2
for tasks in 64 32 16 8 4 2
do  
    srun --reservation=fri --mpi=pmix -n$tasks -N1 mpi_knn ../images/living_room.png $K $MAX_ITERS >> mpi_kmeans_results_part2.txt
    echo "" >> mpi_kmeans_results_part2.txt
done

