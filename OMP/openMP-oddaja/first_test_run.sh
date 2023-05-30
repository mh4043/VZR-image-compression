#!/bin/sh
gcc -O2 VC_compress_MP.c --openmp -o VC_compress_MP -lm



sbatch first_tests_batch_VC_compress_MP.sh


rm VC_compress_MP


./run_all_VC_compress_MP.sh