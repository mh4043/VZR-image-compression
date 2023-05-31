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
    
    unsigned int *clusters_centroids; // centroids = [r1, g1, b1, a1, r2, g2, b2, a2, ...]
    int width, height, cpp;  // Image qualities
    int img_qualities[3]; // width, height, cpp
    unsigned char *image_original, *image_original_part; // image_original_part is for individual process
    char *image_file;
    int number_of_clusters, number_of_iterations; // number of clusters = number of colors, we get both values from input
    int block_size; // size of a block of pixels that go to one process
    int *sendcounts, *recvcounts_centr_index; // sizes of blocks for each process (used in Scatter and Gather)
	int *displs, *displs_centr_index; // offsets in array for each process (used in Scatter and Gather)
    int noOfPixels; // number of pixels that each process gets (last process might have different number)
    int *pixel_centroid_index; // array that holds index of the closest centroid to each pixel (per process)
    long *partial_sum; // sum of pixel RGBA when updating centroids for individual centroids (partial sum has +1 space for count - comes in effect when reduce happens)
    long *sums; //complete sums of RGBA for new centroid
    long count, partial_count; // count of pixels when updating centroids for individual centroids
    double initTime, totalTime;

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
        printf("Starting program with parameters: %s, %d, %d\n", image_file, number_of_clusters, number_of_iterations);
        printf("Number of processes: %d\n", num_p);
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

    // broadcast image qualities (width, height, cpp) to all other processes (Bcast is blocking comm, so we don't have to use barrier)
    MPI_Bcast(img_qualities, 3, MPI_INT, 0, MPI_COMM_WORLD);

    width = img_qualities[0];
    height = img_qualities[1];
    cpp = img_qualities[2];

    clusters_centroids = (unsigned int *)malloc(number_of_clusters * cpp * sizeof(unsigned int));  // An array of cluster centroids (each cluster has one pixel (RGBA))
    partial_sum = (long *)malloc((cpp + 1) * sizeof(long)); // +1 so that when reducing, count is also in this array
    sums = (long *) malloc((cpp + 1) * sizeof(long));

    // setting up for Scatterv for pixels
    sendcounts = (int*)malloc(num_p * sizeof(int)); // number of elements to send to each process
    displs = (int*)malloc(num_p * sizeof(int)); // displacement from original data for process i (number of elements to jump over)
    recvcounts_centr_index = (int*)malloc(width * height * sizeof(int));
    displs_centr_index = (int*)malloc(width * height * sizeof(int));   
    int b_size_part = ceil((width * height * cpp) / (num_p * 1.0));
    block_size = b_size_part + (b_size_part % cpp); // size of one block to go to one process (we want it to be divisible by cpp - so that it can't happent that p1 gets red of a pixel and p2 gets green of the same pixel)
    int last_process_block_size = (width * height * cpp) - (block_size * (num_p - 1)); // last block might have a different size
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
    if(rank == num_p - 1){
        image_original_part = (unsigned char*)malloc(last_process_block_size * sizeof(unsigned char));
        block_size = last_process_block_size;
    }
    // scatterv pixels of an image to all processes
    MPI_Scatterv(image_original, sendcounts, displs, MPI_UNSIGNED_CHAR, image_original_part, block_size, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    
    MPI_Barrier(MPI_COMM_WORLD);

    pixel_centroid_index = (int *)malloc(noOfPixels * sizeof(int));

    if(rank == 0){
        // Init clusters as 1D array ([r1, g1, b1, (a1), r2, b2, g2, (a2), ...])
        for (int i = 0; i < number_of_clusters; i++) {
            int random_pixel_w = (i * width) / number_of_clusters;
            int random_pixel_h = (i * height) / number_of_clusters;

            for (int j = 0; j < cpp; j++) {
                clusters_centroids[i * cpp + j] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + j];
            }
        }   
    }

    // start the timer
    if(rank == 0){
        initTime = MPI_Wtime();
    }

    for(int iter = 0; iter < number_of_iterations; iter ++){
        if(rank == 0){
            printf("ITERATION: %d\n", iter);
            fflush(stdout);
        }
        
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
                    dst += (delta * delta);
                }

                // Get euclidean distance
                // dst = sqrtf(dst);

                // Check if distance is smaller than min_dst
                if (dst < min_dst) {
                    min_dst = dst;
                    min_indx = centroid;
                }
            }
            pixel_centroid_index[pxl] = min_indx;
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if(rank == 0){
            printf("ITERATION: %d, 50\%\n", iter);
            fflush(stdout);
        }

        // 2. For each centroid compute the mean value of all pixels, where pixels' centroid index is current centroid index
        //    And then set the centroid value to the mean value.
        //    parallelize: 
        //        - the inner loop - when one centroid is fully searched, MPI_Reduce is used to SUM all the partial sums and calculate its mean
        //        - each process gets (block_size / cpp) pixels to go over and calculate the partial sum based on current centroid (pixel_centroid_index)
        //          then, reduce is called with operation SUM, to sum all red colors, green colors, blue colors and counts individually from all processes
        //          aftr that, root proces checks if there were no pixels for the current centroid - in this case, a random pixel is chosen from the image, as its value
        //          else, calculates the mean value of each color and updates centroid values with the mean ones
        for(int centroid = 0; centroid < number_of_clusters; centroid++){
            // reset partial sum values
            for(int j = 0; j < cpp; j++){
                partial_sum[j] = 0;
            }
            partial_count = 0;

            // go through all pixels and check if current centroid corresponds to pixel centroid
            for(int pxl = 0; pxl < noOfPixels; pxl ++){
                if(centroid == pixel_centroid_index[pxl]){
                    // sum values for each of the RGBA channels
                    for(int j = 0; j < cpp; j++){
                        partial_sum[j] += image_original_part[pxl * cpp + j];
                    }
                    partial_count += 1;
                }
            }

            // the last element in this array is the number of pixels that belong to current centroid
            partial_sum[cpp] = partial_count;
        
            // reduce partial sums and counts of processes with sum into sums and calculate averages. Replace centroid values with these averaged ones
            MPI_Reduce(partial_sum, sums, cpp + 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

            if(rank == 0){
                count = sums[cpp]; // extract the count (last element)
                // if count is 0, assign a random pixel as centroid to that (current) cluster, else average and replace
                if(count == 0){
                    int random_pxl = rand() % (width * height);
                    for(int j = 0; j < cpp; j++){
                        clusters_centroids[centroid * cpp + j] = image_original[random_pxl * cpp + j];
                    }
                }else{
                    for(int j = 0; j < cpp; j++){
                        clusters_centroids[centroid * cpp + j] = round(sums[j] / count);
                    }
                }
            }

            MPI_Barrier(MPI_COMM_WORLD);
        }

        MPI_Barrier(MPI_COMM_WORLD);
    }

    // overwrite pixel values with cluster centroid values
    MPI_Bcast(clusters_centroids, number_of_clusters * cpp, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    // each process overwrites its part of an image with cluster centroid values
    for (int pxl = 0; pxl < noOfPixels; pxl ++){
        for (int j = 0; j < cpp; j++) {
            image_original_part[pxl * cpp + j] = clusters_centroids[pixel_centroid_index[pxl] * cpp + j];
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // gather every partial image back into full image
    if(rank == num_p - 1){
        block_size = last_process_block_size;
    }
    MPI_Gatherv(image_original_part, block_size, MPI_UNSIGNED_CHAR, image_original, sendcounts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    // stop the timer
    if(rank == 0){
        totalTime = MPI_Wtime() - initTime;
        printf("MPI time: %.3f\n", totalTime);
        fflush(stdout);
    }

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

    free(clusters_centroids);
    free(image_original_part);
    free(image_original);
    
    MPI_Finalize(); 

    return 0;
}
