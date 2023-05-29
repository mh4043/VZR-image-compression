#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <math.h>

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
    
    unsigned int *clusters_centroids; // centroids = [r1, g1, b1, a1, r2, g2, b2, a2, ...]
    int width, height, cpp;  // Image qualities
    int img_qualities[3]; // width, height, cpp
    unsigned char *image_original, *image_original_part; // image_original_part is for individual process
    // Function parameters
    char *image_file;
    int number_of_clusters, number_of_iterations;
    int block_size;
    int *sendcounts, *recvcounts_centr_index; // sizes of blocks for each process
	int *displs, *displs_centr_index; // offsets in array for each process
    int noOfPixels;
    int *pixel_centroid_index; // array that holds index of the closest centroid to each pixel (per process)
    int *all_pixel_centroid_index; // array that holds indexe of the closest centroid to each pixel from all processes
    long *partial_avg; // sum/partial avg of pixel RGBA when updating centroids for individual centroids
    long *averages; //complete averages of RGBA for new centroid
    int count; // count of pixels when updating centroids for individual centroids

    srand(time(NULL));

    MPI_Init(&argc, &argv); // initialize MPI

    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get process rank 
    MPI_Comm_size(MPI_COMM_WORLD, &num_p); // get number of processes

    if(argc < 4){
        fprintf(stderr, "Not enough arguments\n");
        fprintf(stderr, "Usage: %s <IMAGE_PATH> <number of colors> <number of iterations>\n", argv[0]);
        exit(1);
    }

    number_of_clusters = atoi(argv[2]);
    number_of_iterations = atoi(argv[3]);

    if(rank == 0){
        // read image and its dimensions and cpp
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

        all_pixel_centroid_index = (int *)malloc(width * height * sizeof(int));
    }

    // broadcast image qualities (width, height, cpp) to all other processes (Bcast is blocking comm, so we don't have to use barrier)
    MPI_Bcast(img_qualities, 3, MPI_INT, 0, MPI_COMM_WORLD);

    width = img_qualities[0];
    height = img_qualities[1];
    cpp = img_qualities[2];

    clusters_centroids = (unsigned int *)malloc(number_of_clusters * cpp * sizeof(unsigned int));  // An array of cluster centroids (each cluster has one pixel (RGBA))
    partial_avg = (long *)malloc(cpp * sizeof(long));
    averages = (long *)malloc(cpp * sizeof(long));

    // setting up for Scatterv for pixels
    sendcounts = (int*)malloc(num_p * sizeof(int)); // number of elements to send to each process
    displs = (int*)malloc(num_p * sizeof(int)); // displacement from original data for process i (number of elements to jump over)
    recvcounts_centr_index = (int*)malloc(width * height * sizeof(int));
    displs_centr_index = (int*)malloc(width * height * sizeof(int));   
    int b_size_part = ceil((width * height * cpp) / (num_p * 1.0));
    block_size = b_size_part + (b_size_part % cpp); // size of one block to go to one process (we want it to be divisible by cpp - so that it can't happent that p1 gets red of a pixel and p2 gets green of the same pixel)
    int last_process_block_size = (width * height * cpp) - (block_size * (num_p - 1));
        for(int i = 0; i < num_p; i ++){
        displs[i] = i * block_size;
        displs_centr_index[i] = i * (block_size / cpp);
        if(i + 1 >= num_p){
            sendcounts[i] = last_process_block_size;
            recvcounts_centr_index[i] = (last_process_block_size / cpp);
        }else{
            sendcounts[i] = block_size;
            recvcounts_centr_index[i] = (block_size / cpp);
        }
    }
    // last process can have less pixels than others
    noOfPixels = block_size / cpp;
    if(rank == num_p - 1){
        noOfPixels = last_process_block_size / cpp;
    }
    image_original_part = (unsigned char*)malloc(block_size * sizeof(unsigned char));
    // scatterv pixels of an image to all processes
    MPI_Scatterv(image_original, sendcounts, displs, MPI_UNSIGNED_CHAR, image_original_part, block_size, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    pixel_centroid_index = (int *)malloc(noOfPixels * sizeof(int));

    if(rank == 0){
        // Init clusters as 1D array
        for (int i = 0; i < number_of_clusters; i++) {
            int random_pixel_w = (i * width) / number_of_clusters;
            int random_pixel_h = (i * height) / number_of_clusters;

            for (int j = 0; j < cpp; j++) {
                clusters_centroids[i * cpp + j] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + j];
            }
            // printf("X and Y for cluster: %d, %d, \n", random_pixel_w, random_pixel_h);
        }   
    }

    for(int iter = 0; iter < number_of_iterations; iter ++){

        // broadcast cluster centroids array to all other processes
        MPI_Bcast(clusters_centroids, number_of_clusters * cpp, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

        // 1. Each process gets part of all observations/pixels, finds centroid with minimum euclidean distance to current obs/pixel and stores it.
        // each process goes through all of their pixels and centroids and finds the centroid with min eucledean distance in terms of RGB
        for (int pxl = 0; pxl < noOfPixels; pxl ++){
            float min_dst = 1000;  // minimum distance from pixel to centroid
            int min_indx = 0;      // centroid index with min distance to pixel
            for (int centroid = 0; centroid < number_of_clusters; centroid++) {
                float dst = 0.0;

                // Iterate cpp to get RGB values, accumulate distance
                for (int j = 0; j < cpp; j++) {
                    char cntr = clusters_centroids[centroid * cpp + j];
                    char delta = image_original_part[pxl * cpp + j] - cntr;
                    dst += powf(delta, 2);
                }

                // Get euclidean distance
                dst = sqrtf(dst);

                // Check if distance is smaller than min_dst
                if (dst < min_dst) {
                    min_dst = dst;
                    min_indx = centroid;
                }
            }
            pixel_centroid_index[pxl] = min_indx;
        }

        // gathers pixels centroid indexes from all processes
        // MPI_Gatherv(pixel_centroid_index, noOfPixels, MPI_INT, all_pixel_centroid_index, recvcounts_centr_index, displs_centr_index, MPI_INT, 0, MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);

        // 2. For each centroid compute the mean value of all pixels, where pixels' centroid index (from all_pixel_centroid_index) is current centroid index
        //    And then set the centroid value to the mean value.
        // parallelize: 1. the outer loop - each process checks a certain number of centroids OR 2. the inner loop (outer loop is serial) - each process calculates a portion of a mean value and then sums it together
        //    2. the inner loop - when one centroid is fully searched, MPI_Reduce is used to SUM all the partial means
        //       each process gets (block_size / cpp) pixels to go over and calculate the partial mean based on current centroid (pixel_centroid_index)
        
        for(int centroid = 0; centroid < number_of_clusters; centroid++){
            // reset partial avg values
            for(int j = 0; j < cpp; j++){
                partial_avg[j] = 0;
            }
            count = 0;
            for(int pxl = 0; pxl < noOfPixels; pxl ++){
                if(centroid == pixel_centroid_index[pxl]){
                    // sum values for each of the RGBA channels
                    for(int j = 0; j < cpp; j++){
                        partial_avg[j] += image_original_part[pxl * cpp + j];
                    }
                    count += 1;
                }
            }

            // if no pixels in cluster - put random value in centroid (TODO)
            if(count == 0){
                continue;
            }

            for(int j = 0; j < cpp; j++){
                partial_avg[j] = round(partial_avg[j] / count);
            }

            // reduce partial averages of processes with sum into averages and replace centroid values with these averaged ones
            MPI_Reduce(partial_avg, averages, cpp, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

            if(rank == 0){
                for(int j = 0; j < cpp; j++){
                    clusters_centroids[centroid * cpp + j] = averages[j];
                }
            }

            MPI_Barrier(MPI_COMM_WORLD);
        }

        if(rank == 0){
            for(int i = 0; i < number_of_clusters; i++){
                printf("i: %d, new cluster values: R %d G %d B %d\n", i, clusters_centroids[i * cpp], clusters_centroids[i * cpp + 1], clusters_centroids[i * cpp + 2]);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);
    }

    // overwrite pixel values with cluster centroid values
    MPI_Bcast(clusters_centroids, number_of_clusters * cpp, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    for (int pxl = 0; pxl < noOfPixels; pxl ++){
        for (int j = 0; j < cpp; j++) {
            image_original_part[pxl * cpp + j] = clusters_centroids[pixel_centroid_index[pxl] * cpp + j];
        }
    }    

    // gather every partial image back into full image
    MPI_Gatherv(image_original_part, block_size, MPI_UNSIGNED_CHAR, image, sendcounts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    // output image
    if(rank == 0){
        if (cpp == 3) {
            // Check if the image was written
            if (stbi_write_jpg("compressed_image.jpg",
                               width, height, cpp, image_original, 100)) {
                printf("Image written successfully\n");
            } else {
                printf("Error writing image\n");
            }
        }
        if (cpp == 4) {
            int stride_in_bytes = width * cpp;
            // Check if the image was written
            if (stbi_write_png("compressed_image.png",
                               width, height, cpp, image_original, stride_in_bytes)) {
                printf("Image written successfully\n");
            } else {
                printf("Error writing image\n");
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    free(clusters_centroids);
    free(image_original_part);
    free(image);
    
    MPI_Finalize(); 


    return 0;
}