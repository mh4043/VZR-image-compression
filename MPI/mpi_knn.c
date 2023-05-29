#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../packages/stb_image.h"
#include "../packages/stb_image_write.h"

int main(int argc, char *argv[]) {
    // MPI vars
    int rank; // process rank 
	int num_p; // total number of processes 
	int source; // sender rank
	int tag = 0; // message tag
	int buffer[1]; // message buffer 
	MPI_Status status; // message status
	MPI_Request request; // MPI request
	int flag; // request status flag
    
    unsigned int *clusters_centroids;
    int width, height, cpp;  // Image qualities
    int img_qualities[3]; // width, height, cpp
    unsigned char *image_original;
    // Function parameters
    char *image_file;
    int number_of_clusters, number_of_iterations;

    srand(time(NULL));

    MPI_Init(&argc, &argv); // initialize MPI

    //MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get process rank 
    //MPI_Comm_size(MPI_COMM_WORLD, &num_p); // get number of processes

    /*
    if(argc < 4){
        fprintf(stderr, "Not enough arguments\n");
        fprintf(stderr, "Usage: %s <IMAGE_PATH> <number of colors> <number of iterations>\n", argv[0]);
        exit(1);
    }
    */

    //number_of_clusters = atoi(argv[2]);
    //number_of_iterations = atoi(argv[3]);

    /*
    if(rank == 0){
        image_file = argv[1];
        printf("Starting program with parameters %s, %d, %d\n", image_file, number_of_clusters, number_of_iterations);
        fflush(stdout);

        image_original = stbi_load(image_file, &width, &height, &cpp, 0);
        img_qualities[0] = width;
        img_qualities[1] = height;
        img_qualities[2] = cpp;

        if(image_original){
            printf("Image loaded with dimensions: %d %d %d\n", width, height, cpp);
            fflush(stdout);
        }else{
            fprintf(stderr, "Error loading image %s!\n", image_file);
            exit(1);
        }   
    }
    */

    // broadcast image qualities (width, height, cpp) to all other processes (Bcast is blocking comm, so we don't have to use barrier)
    //MPI_Bcast(img_qualities, 3, MPI_INT, 0, MPI_COMM_WORLD);

    //width = img_qualities[0];
    //height = img_qualities[1];
    //cpp = img_qualities[2];

    //printf("Rank: %d, width: %d, height: %d, cpp: %d\n", rank, width, height, cpp);
    //fflush(stdout);





        

        

            // Init clusters as 1D array
            /*
            clusters_centroids = (unsigned int *)malloc(number_of_clusters * cpp * sizeof(unsigned int));  // This will map cluster to pixel, each cluster will have a pixel
            for (int i = 0; i < number_of_clusters; i++) {
                int random_pixel_w = (i * width) / number_of_clusters;
                int random_pixel_h = (i * height) / number_of_clusters;

                for (int j = 0; j < cpp; j++) {
                    clusters_centroids[i * cpp + j] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + j];
                }
                // printf("X and Y for cluster: %d, %d, \n", random_pixel_w, random_pixel_h);
            }

            printf("Rank: %d, number of clusters: %d, first cluster centroid: %d\n", rank, number_of_clusters, clusters_centroids[0]);
            */

    
    MPI_Finalize(); 

        


    return 0;
}