#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// STB
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../packages/stb_image.h"
#include "../packages/stb_image_write.h"

// CUDA
#include <cuda.h>
#include <cuda_runtime.h>

#include "../packages/helper_cuda.h"

int width, height, cpp;  // Image qualities
unsigned char *image_original;

// Function parameters
char *image_file;
int number_of_clusters, number_of_iterations;

#define BINS 256
#define UNIFIED_MEMORY
#define THREADS_PER_BLOCK 256

__device__ int get_closest_cluster(unsigned char *pixel, int number_of_clusters, unsigned int *clusters, int cpp) {
    // Iterate number of clusters, check which one is closest in terms of euclidean distance of RGB
    float min_dst = 1000;  // minimum distance from pixel to centroid
    int min_indx = 0;      // centroid index with min distance to pixel
    for (int centroid = 0; centroid < number_of_clusters; centroid++) {
        float dst = 0.0;

        // Iterate cpp to get RGB values, accumulate distance
        for (int j = 0; j < cpp; j++) {
            char cntr = clusters[centroid * cpp + j];
            char delta = pixel[j] - cntr;
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
    return min_indx;
}

__global__ void fill_closest_clusters(
    unsigned char *image,
    unsigned int *clusters,
    int *cluster_assignments,
    int number_of_clusters,
    int width,
    int height,
    int cpp) {

    // Get thread id
    int tid = blockIdx.x * blockDim.x + threadIdx.x;

    // Get pixel

    unsigned char *pixel = &image[tid * cpp];

    // Assign pixel to cluster
    cluster_assignments[tid] = get_closest_cluster(pixel, number_of_clusters, clusters, cpp);
}

__global__ void update_centroids(unsigned char *image,
                                 unsigned int *clusters,
                                 int *cluster_assignments,
                                 int number_of_clusters,
                                 int width, int height, int cpp) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;

    int block_tid = threadIdx.x;
    
    __shared__ unsigned char shared_image[THREADS_PER_BLOCK * 4];
    // Create shared memory
    if (cpp == 3)
        __shared__ unsigned char shared_image[THREADS_PER_BLOCK * 3];
    
    for (int j = 0; j < cpp; j++) 
        shared_image[block_tid *cpp + j] = image[tid * cpp + j];

    // Save cluster assignments to shared memory
    __shared__ int cluster_assignments_shared[THREADS_PER_BLOCK];
    cluster_assignments_shared[block_tid] = cluster_assignments[tid];

    // Wait for all threads to finish
    __syncthreads();

    // If block_tid is 0, then we are in the first thread of the block, so we can do the reduction
    if (block_tid == 0) {
        // Cluster counts - count how many pixels have cluster[i] as closest, store in shared memory, define size as number_of_clusters
        int cluster_counts_shared[THREADS_PER_BLOCK] = {0};

        // Cluster centroids - sum all pixels that have cluster[i] as closest, store in shared memory, define size as number_of_clusters * cpp
        unsigned int clusters_shared[THREADS_PER_BLOCK * 4] = {0};
        if (cpp == 3)
            unsigned int clusters_shared[THREADS_PER_BLOCK * 3] = {0};


        // Iterate over all threads
        for (int i = 0; i < THREADS_PER_BLOCK; i++) {
            // Get cluster assignment
            int cluster_assignment = cluster_assignments_shared[i];
            // Increment cluster count
            cluster_counts_shared[cluster_assignment] += 1;
            // Increment cluster centroid
            for (int j = 0; j < cpp; j++) 
                clusters_shared[cluster_assignment * cpp + j] += shared_image[i *cpp + j];
        }

        // Save cluster counts to global memory
        for (int i = 0; i < number_of_clusters; i++) {
            cluster_assignments[i] = cluster_counts_shared[i];
        }
    }
    // Wait for all threads to finish
    __syncthreads();

    if (tid < number_of_clusters) {
        clusters[tid] /= cluster_assignments[tid];
    }
}

__global__ void save_images(unsigned char *image, unsigned int *clusters, int *cluster_assignments, int cpp) {
    // Get thread id
    int tid = blockIdx.x * blockDim.x + threadIdx.x;

    // Save image
    for (int j = 0; j < cpp; j++) {
        image[tid * cpp + j] = clusters[cluster_assignments[tid] * cpp + j];
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc > 3) {
        image_file = argv[1];
        number_of_clusters = atoi(argv[2]);
        number_of_iterations = atoi(argv[3]);
    } else {
        fprintf(stderr, "Not enough arguments\n");
        fprintf(stderr, "Usage: %s <IMAGE_PATH> <number of colors> <number of iterations>\n", argv[0]);
        exit(1);
    }

    fflush(stdout);
    image_original = stbi_load(image_file, &width, &height, &cpp, 0);

    if (image_original) {
        // Define Thread number
        #define SIZE (width * height)

        // Thread organization
        dim3 blockSize(THREADS_PER_BLOCK);
        dim3 gridSize((SIZE + blockSize.x - 1) / blockSize.x);

        printf("Image size: %d, Image width: %d, Image height: %d, Image channels: %d, Number of clusters: %d, Number of iterations: %d\n", SIZE, width, height, cpp, number_of_clusters, number_of_iterations);
        fflush(stdout);
        printf("Block size: %d, Grid size: %d\n", blockSize.x, gridSize.x);
        fflush(stdout);

        // Init clusters as 1D array
        unsigned int clusters_centroids[number_of_clusters * cpp];  // This will map cluster to pixel, each cluster will have a pixel
        for (int i = 0; i < number_of_clusters; i++) {
            int random_pixel_w = (i * width) / number_of_clusters;
            int random_pixel_h = (i * height) / number_of_clusters;

            for (int j = 0; j < cpp; j++) {
                clusters_centroids[i * cpp + j] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + j];
            }
        }

        // Init cluster assignments, use malloc
        int *cluster_assignments = (int *)malloc(SIZE * sizeof(int));

        // Init cluster counts
        int cluster_counts[number_of_clusters];  // This will map cluster to number of pixels

        // Copy image to device
        unsigned char *image_device;
        checkCudaErrors(cudaMalloc(&image_device, SIZE * cpp * sizeof(unsigned char)));
        checkCudaErrors(cudaMemcpy(image_device, image_original, SIZE * cpp * sizeof(unsigned char), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy image_original failed");

        // Copy clusters to device as 1D array
        unsigned int *clusters_device;
        checkCudaErrors(cudaMalloc(&clusters_device, number_of_clusters * cpp * sizeof(unsigned int)));
        checkCudaErrors(cudaMemcpy(clusters_device, clusters_centroids, number_of_clusters * cpp * sizeof(unsigned int), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy clusters_centroids failed");

        // Copy cluster counts to device
        int *cluster_counts_device;
        checkCudaErrors(cudaMalloc(&cluster_counts_device, number_of_clusters * sizeof(int)));
        checkCudaErrors(cudaMemcpy(cluster_counts_device, cluster_counts, number_of_clusters * sizeof(int), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy cluster_counts failed");

        // Copy cluster assignments to device
        int *cluster_assignments_device;
        checkCudaErrors(cudaMalloc(&cluster_assignments_device, SIZE * sizeof(int)));
        checkCudaErrors(cudaMemcpy(cluster_assignments_device, cluster_assignments, SIZE * sizeof(int), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy cluster_assignments failed");

        cudaEvent_t start, stop;
        float milliseconds = 0;
        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start);

        for (int iter = 0; iter < number_of_iterations; iter++) {
            // Assign each pixel to a cluster
            fill_closest_clusters<<<gridSize, blockSize>>>(image_device,
                                                           clusters_device,
                                                           cluster_assignments_device,
                                                           number_of_clusters,
                                                           width,
                                                           height,
                                                           cpp);

            // Update centroids
            update_centroids<<<gridSize, blockSize>>>(image_device,
                                                      clusters_device,
                                                      cluster_assignments_device,
                                                      number_of_clusters,
                                                      width,
                                                      height,
                                                      cpp);

        }
        // Save image
        save_images<<<gridSize, blockSize>>>(image_device, clusters_device, cluster_assignments_device, cpp);

        getLastCudaError("kmeans_clustering failed");

        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&milliseconds, start, stop);

        printf("GPU Time: %0.3f milliseconds \n", milliseconds);

        // Copy clusters to host
        checkCudaErrors(cudaMemcpy(clusters_centroids, clusters_device, number_of_clusters * cpp * sizeof(unsigned int), cudaMemcpyDeviceToHost));
        getLastCudaError("cudaMemcpy clusters_centroids failed");

        // Copy cluster counts to host
        checkCudaErrors(cudaMemcpy(cluster_counts, cluster_counts_device, number_of_clusters * sizeof(int), cudaMemcpyDeviceToHost));
        getLastCudaError("cudaMemcpy cluster_counts failed");

        // Copy image to host
        checkCudaErrors(cudaMemcpy(image_original, image_device, SIZE * cpp * sizeof(unsigned char), cudaMemcpyDeviceToHost));
        getLastCudaError("cudaMemcpy image_original failed");

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

        // Free memory
        checkCudaErrors(cudaFree(image_device));
        checkCudaErrors(cudaFree(clusters_device));
        checkCudaErrors(cudaFree(cluster_counts_device));

    } else {
        fprintf(stderr, "Error loading image %s!\n", image_file);
    }
}