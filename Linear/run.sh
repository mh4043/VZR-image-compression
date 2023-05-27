#!/bin/sh
gcc linear.c -lm -o linear_knn
srun --reservation=fri -G1 -n1 linear_knn ../images/living_room.png 32 10