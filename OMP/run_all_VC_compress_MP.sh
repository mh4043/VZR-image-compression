#!/bin/sh
gcc -O2 VC_compress_MP.c --openmp -o VC_compress_MP -lm



# sbatch sbatch_VC_compress_MP_1.sh
# sbatch sbatch_VC_compress_MP_8.sh
# sbatch sbatch_VC_compress_MP_16.sh
# sbatch sbatch_VC_compress_MP_32.sh
# sbatch sbatch_VC_compress_MP_64.sh



#! drug test iz description
sbatch sbatch_VC_compress_MP_64.sh
sbatch sbatch_VC_compress_MP_32.sh
sbatch sbatch_VC_compress_MP_16.sh
sbatch sbatch_VC_compress_MP_8.sh
sbatch sbatch_VC_compress_MP_4.sh
sbatch sbatch_VC_compress_MP_2.sh

rm VC_compress_MP