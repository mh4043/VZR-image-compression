// C program to count primes smaller than or equal to N
// compile: gcc -O2 VC_compress_MP.c --openmp -o VC_compress_MP -lm
// example run: ./VC_compress_MP imageName [numberOfClusters] [numberOfIterations]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>



//? za branje slike
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"


#define DEBUG 1
#define INFO_LOG 0

void randomCentroids(unsigned char* centroids, int k, int cpp, unsigned char* img, int N);
void compressMP(unsigned char* img, unsigned char** out_id, unsigned char** out_tab, int N, int cpp, int k, int iter);

void saveCompressedImage(unsigned char *image_in, char *imgName, int width, int height, int channelsPerPixel);

void imgFromTable(unsigned char** img, unsigned char* id, unsigned char* tab, int N, int cpp);





int main(int argc,char* argv[])
{
  unsigned char *imgName, *o1_id, *o1_tab, *o1_img;
  int k, iter, width, height, cpp, totalThreads;
  if(argc != 4){
    fprintf(stderr, "Wrong input argument count!\nUse case: %s <imgName>.png <k-count> <iterations>\n", argv[0]);
    exit(1);
  }

  imgName = argv[1];
  k = atoi(argv[2]);
  iter = atoi(argv[3]);

  unsigned char *image_in = stbi_load(imgName, &width, &height, &cpp, 0);
  if(!image_in){
    printf("Problem with reading image %s\n", imgName);
    return 1;
  }

  totalThreads = omp_get_max_threads();
  if(DEBUG){
    printf("------------------------------------------------------------\n");
    printf("totalThreads: %d\n", totalThreads);
    printf("numberOfClusters: %d\n", k);
    printf("numberOfIterations: %d\n", iter);
    printf("Schedule type: static\n");
  }

  if(INFO_LOG)
    printf("Input arguments:\n  imgName: %s\n  k: %d\n  iter: %d\n", imgName, k, iter);

  double timeStart = omp_get_wtime();
  compressMP(image_in, &o1_id, &o1_tab, width * height, cpp, k, iter);
  double timeEnd = omp_get_wtime();
  double timeElapsed = timeEnd - timeStart;
  if(DEBUG)
    printf("Time elapsed: %.3f\n", timeElapsed);


  imgFromTable(&o1_img, o1_id, o1_tab, width * height, cpp);
  saveCompressedImage(o1_img, imgName, width, height, cpp);
}


//* array of "random" cluster centroids
// centroids = [r1, g1, b1, a1, r2, g2, b2, a2, ...]
// k = number of clusters
// byteSize = 3 for RGB, 4 for RGBA == CPP 
// img = image array
// N = number of pixels
void randomCentroids(unsigned char* centroids, int k, int cpp, unsigned char* img, int N){
  int seg = (N/k);

  for(int i=0; i<k; i++){
    centroids[i*cpp] = img[i*seg*cpp];
    centroids[i*cpp + 1] = img[i*seg*cpp+1];
    centroids[i*cpp + 2] =img[i*seg*cpp+2];
    if(cpp == 4) //? alpha channel
      centroids[i*cpp + 3] = 255;
  }
}


void compressMP(unsigned char* img, unsigned char** out_id, unsigned char** out_tab, int N, int cpp, int k, int iter){

  double min_d = -1, dist, cnt;
  int min_c;
  long long sum[4];

  *out_id = (unsigned char*)malloc(sizeof(unsigned char) * N);
  *out_tab = (unsigned char*)malloc(sizeof(unsigned char) * cpp * k);
  randomCentroids(*out_tab, k, cpp, img, N); //* init centroids

  double timeStartIter, timeEndIter, timeElapsedIter;
 
  for(int i=0; i<iter; i++){
    timeStartIter = omp_get_wtime();
    //* firstPrivate = value is copied and each thread gets its own copy

    #pragma omp parallel for firstprivate(min_d) private(dist, min_c) schedule(static)
    for(int x=0; x<N; x++){ // for each pixel
      min_d = -1;
      for(int c=0; c<k; c++){ // for each cluster
        if(cpp == 4) //? alpha channel
          dist = sqrt(pow(img[x*cpp] - (*out_tab)[c*cpp], 2) + 
                      pow(img[x*cpp+1] - (*out_tab)[c*cpp+1], 2) +
                      pow(img[x*cpp+2] - (*out_tab)[c*cpp+2], 2) +
                      pow(img[x*cpp+3] - (*out_tab)[c*cpp+3], 2));
        else
          dist = sqrt(pow(img[x*cpp] - (*out_tab)[c*cpp], 2) + 
                      pow(img[x*cpp+1] - (*out_tab)[c*cpp+1], 2) +
                      pow(img[x*cpp+2] - (*out_tab)[c*cpp+2], 2));
        if(min_d < 0 || dist < min_d){
          min_d = dist;
          min_c = c;
        }
      }

      (*out_id)[x] = min_c; //assign cluster (with minimum distance) to pixel 
    }
        
    #pragma omp for private(cnt, sum) schedule(static)
    for(int c=0; c<k; c++){
      cnt = 0.0f;
      sum[0] = 0;
      sum[1] = 0;
      sum[2] = 0;
      if(cpp == 4)
        sum[3] = 0;
      
      for(int x=0; x<N; x++){
        if((*out_id)[x] == c){
          sum[0] += img[x*cpp];
          sum[1] += img[x*cpp+1];
          sum[2] += img[x*cpp+2];
          if(cpp == 4)
            sum[3] += img[x*cpp+3]; 
          cnt++;   
        }
      }

      if(cnt == 0.0f){ //? if no pixels in cluster
        continue; //TODO - randomize centroid
        printf("No pixels in cluster %d\n", c);
      }

      (*out_tab)[c*cpp] = round(sum[0] / cnt);
      (*out_tab)[c*cpp+1] = round(sum[1] / cnt);
      (*out_tab)[c*cpp+2] = round(sum[2] / cnt);
      if(cpp == 4)
        (*out_tab)[c*cpp+3] = round(sum[3] / cnt);
    }

    timeEndIter = omp_get_wtime();
    if(DEBUG){
      timeElapsedIter = timeEndIter - timeStartIter;
      printf("Time elapsed for iteration %d: %.3f\n", i, timeElapsedIter);
      fflush(stdout);
    }
  }
}



void saveCompressedImage(
  unsigned char *image_in,
  char *imgName,
  int width,
  int height,
  int channelsPerPixel
){
  char *dotChar = strstr(imgName, ".");
  int iend = dotChar - imgName;
  char newImageFileName[iend+15]; //+ _compressed.jpg 
  strncpy(newImageFileName, imgName, iend);
  if(channelsPerPixel == 3){
    strcat(newImageFileName, "_compressed.jpg");
    printf("newImageFileName %s\n", newImageFileName);
    stbi_write_jpg(newImageFileName,
      width, height, channelsPerPixel, image_in, 100); //channelsPer Pixel = comp channels of data per channel: 1=Y, 2=YA, 3=RGB, 4=RGBA
  }
  if(channelsPerPixel == 4){
    strcat(newImageFileName, "_compressed.png");
    printf("newImageFileName %s\n", newImageFileName);
    int stride_in_bytes = width * channelsPerPixel;
    // printf("stride_in_bytes %d\n", stride_in_bytes);
    stbi_write_png(newImageFileName,
      width, height, channelsPerPixel, image_in, stride_in_bytes);
  }
}


void imgFromTable(unsigned char** img, unsigned char* id, unsigned char* tab, int N, int cpp){
  *img = (unsigned char*)malloc(N * cpp * sizeof(unsigned char));

  for(int i=0; i<N; i++){
    (*img)[i*cpp] = tab[id[i]*cpp];
    (*img)[i*cpp+1] = tab[id[i]*cpp+1];
    (*img)[i*cpp+2] = tab[id[i]*cpp+2];
    if(cpp == 4)
      (*img)[i*cpp+3] = 255;
  }
}




