#!/bin/sh
#SBATCH --reservation=fri
#SBATCH --job-name=openMP_compress_first
#SBATCH --output=first_log_vc_compress.log
#SBATCH --time=01:30:00
#SBATCH --constraint=AMD         #? vozlišča z AMD procesorji
#SBATCH --ntasks=1               #? ena instanca programa
#SBATCH --cpus-per-task=64       #? koliko proc jeder dodelimo nalogi --> ce to zelimo izkoristit mora bit OMP_NUM_THREADS enaka stevilka
#SBATCH --wait



#Multithreading
export OMP_PLACES=cores      #? razporejanje niti -> niti želimo razporediti na jedra, ne na sockete ##moznosti socket (fizicni procesor), threads (vsaka nit dobi svojo strojno nit hyperthreading 1 jedro več niti), cores jedro
export OMP_PROC_BIND=TRUE    #? ko se nit zažene na jedru tam tudi ostane
export OMP_NUM_THREADS=64    #? koliko niti bi želeli uporabit, lahko se znotraj pograma nastavi, ampak pol treba ponovno prevajat, tu pa z okoljsko spremenljivko zamenjamo






srun VC_compress_MP google.jpg 8 100
srun VC_compress_MP google.jpg 16 100
srun VC_compress_MP google.jpg 32 100
srun VC_compress_MP google.jpg 64 100
srun VC_compress_MP google.jpg 128 100



srun VC_compress_MP lena.png 8 100
srun VC_compress_MP lena.png 16 100
srun VC_compress_MP lena.png 32 100
srun VC_compress_MP lena.png 64 100
srun VC_compress_MP lena.png 128 100


srun VC_compress_MP livingRoom.png 8 100
srun VC_compress_MP livingRoom.png 16 100
srun VC_compress_MP livingRoom.png 32 100
srun VC_compress_MP livingRoom.png 64 100
srun VC_compress_MP livingRoom.png 128 100


