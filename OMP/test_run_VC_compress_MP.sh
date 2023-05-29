#!/bin/sh
gcc -O2 VC_compress_MP.c --openmp -o VC_compress_MP -lm



sbatch test_sbatch_VC_compress_MP.sh








# --EXPORT=OMP_PLACES=cores,OMP_PROC_BIND=TRUE,OMP_NUM_THREADS=64
# --constraint=AMD
# srun  --reservation=fri --time=00:05:00  --ntasks=1 --cpus-per-task=32 VC_compress_MP google.jpg 64 5 


# srun  --reservation=fri --time=00:01:00  --ntasks=1 --cpus-per-task=1 VC_compress_MP lena.png 64 5 


rm VC_compress_MP