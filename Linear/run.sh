#!/bin/sh
gcc linear.c -lm -o linear_knn
srun --reservation=fri -G1 -n1 linear_knn ../images/lena_512.png 10 1