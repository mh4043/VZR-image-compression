#!/bin/sh
module load CUDA/10.1.243-GCC-8.3.0
#nvcc cuda_knn.cu -O2 -o cuda_knn
#srun --reservation=fri -G1 -n1 cuda_knn ../images/living_room.png 128 100 # Dimensions 7378 4924 3 - actually jpg
#srun --reservation=fri -G1 -n1 cuda_knn ../images/lena_512.png 128 100 # Dimensions 512 512 3 - actually jpg
#srun --reservation=fri -G1 -n1 cuda_knn ../images/lena.png 128 100 >> cuda_kmeans_results.txt # Dimensions  8096 4048 4

sed -i "s/#define THREADS_PER_BLOCK.*/#define THREADS_PER_BLOCK 128/g" cuda_knn.cu
nvcc cuda_knn.cu -O2 -o cuda_knn

echo "Initiating tests with varying K with fixed parameters (MAX_ITERS=100, BLOCK_SIZE=128)" > cuda_kmeans_results.txt
MAX_ITERS=100
# Iterate K from 8 to 128
for k in 8 16 32 64 128
do
    srun --reservation=fri -G1 -n1 cuda_knn ../images/living_room.png $k $MAX_ITERS >> cuda_kmeans_results.txt
    echo "" >> cuda_kmeans_results.txt
done

# Iterate BLOCK_SIZE from 32 to 1024
for block_size in 32 64 128 256 512 1024
do  
    # Change block size in cuda_knn.cu, in format #define THREADS_PER_BLOCK block_size
    sed -i "s/#define THREADS_PER_BLOCK.*/#define THREADS_PER_BLOCK $block_size/g" cuda_knn.cu
    nvcc cuda_knn.cu -O2 -o cuda_knn

    srun --reservation=fri -G1 -n1 cuda_knn ../images/living_room.png 32 $MAX_ITERS >> cuda_kmeans_results.txt
    echo "" >> cuda_kmeans_results.txt
done

