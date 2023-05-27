#!/bin/sh
module load CUDA/10.1.243-GCC-8.3.0
nvcc cuda_knn.cu -O2 -o cuda_knn
srun --reservation=fri -G1 -n1 cuda_knn ../images/lena_512.png 64 5
