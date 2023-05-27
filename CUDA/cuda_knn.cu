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
#define THREADS_PER_BLOCK (1024)

__global__ void kmeans_clustering(
    unsigned char *image,
    unsigned int *clusters,
    int *cluster_counts,
    int *cluster_assignments,
    int number_of_clusters,
    int width,
    int height,
    int cpp,
    int iterations) {
    // Get thread id
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    for (int iteration = 0; iteration < iterations; iteration++) {
        // Get pixel
        unsigned char *pixel = &image[tid * cpp];

        // Find closest cluster with euclidean distance
        int closest_cluster = -1;
        int closest_distance = 999999999;

        for (int i = 0; i < number_of_clusters; i++) {
            int distance = 0;
            for (int j = 0; j < cpp; j++) {
                distance += (pixel[j] - clusters[i * cpp + j]) * (pixel[j] - clusters[i * cpp + j]);
            }
            if (distance < closest_distance) {
                closest_distance = distance;
                closest_cluster = i;
            }
        }

        // Assign pixel to cluster
        cluster_assignments[tid] = closest_cluster;

        // Add pixel to cluster count
        atomicAdd(&cluster_counts[closest_cluster], 1);

        // Add pixel to cluster sum
        for (int i = 0; i < cpp; i++) {
            atomicAdd(&clusters[closest_cluster * cpp + i], pixel[i]);
        }

        // Sync threads
        __syncthreads();

        if (tid < number_of_clusters) {
            /*
            // Check if cluster is empty - add the next closest pixel
            if (cluster_counts[tid] == 0) {
                int closest_distance = 999999999;
                int closest_pixel = -1;
                for (int j = 0; j < width * height; j++) {
                    int distance = 0;
                    for (int k = 0; k < cpp; k++) {
                        distance += (image[j * cpp + k] - clusters[tid * cpp + k]) * (image[j * cpp + k] - clusters[tid * cpp + k]);
                    }
                    if (distance < closest_distance) {
                        closest_distance = distance;
                        closest_pixel = j;
                    }
                }
                for (int j = 0; j < cpp; j++) {
                    clusters[tid * cpp + j] = image[closest_pixel * cpp + j];
                }
            }
            */

            for (int j = 0; j < cpp; j++) {
                //if (cluster_counts[tid] == 0)
                    //printf("Cluster %d has no count\n", tid);
                clusters[tid * cpp + j] = clusters[tid * cpp + j] / cluster_counts[tid];
            }
        }
        __syncthreads();
    }

    // Sync threads
    __syncthreads();

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

    printf("Starting program with parameters %s, %d, %d\n", image_file, number_of_clusters, number_of_iterations);
    fflush(stdout);
    image_original = stbi_load(image_file, &width, &height, &cpp, 0);

    if (image_original) {
// Define Thread number
#define SIZE (width * height)

        // Thread organization
        dim3 blockSize(THREADS_PER_BLOCK);
        dim3 gridSize((SIZE) / blockSize.x);

        printf("Image loaded with dimensions: %d %d %d\n", width, height, cpp);
        fflush(stdout);

        // Init clusters as 1D array
        unsigned int clusters_centroids[number_of_clusters * cpp];  // This will map cluster to pixel, each cluster will have a pixel
        for (int i = 0; i < number_of_clusters; i++) {
            int random_pixel_w = (i * width) / number_of_clusters;
            int random_pixel_h = (i * height) / number_of_clusters;

            for (int j = 0; j < cpp; j++) {
                clusters_centroids[i * cpp + j] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + j];
            }
            printf("X and Y for cluster: %d, %d, \n", random_pixel_w, random_pixel_h);
        }

        // Init cluster assignments, use malloc
        int *cluster_assignments = (int *)malloc(SIZE * sizeof(int));

        // Init cluster counts
        int cluster_counts[number_of_clusters];  // This will map cluster to number of pixels

        // Copy image to device
        unsigned char *image_device;
        checkCudaErrors(cudaMalloc((void **)&image_device, SIZE * cpp * sizeof(unsigned char)));
        checkCudaErrors(cudaMemcpy(image_device, image_original, SIZE * cpp * sizeof(unsigned char), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy image_original failed");

        // Copy clusters to device as 1D array
        unsigned int *clusters_device;
        checkCudaErrors(cudaMalloc((void **)&clusters_device, number_of_clusters * cpp * sizeof(unsigned int)));
        checkCudaErrors(cudaMemcpy(clusters_device, clusters_centroids, number_of_clusters * cpp * sizeof(unsigned char), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy clusters_centroids failed");

        // Copy cluster counts to device
        int *cluster_counts_device;
        checkCudaErrors(cudaMalloc((void **)&cluster_counts_device, number_of_clusters * sizeof(int)));
        checkCudaErrors(cudaMemcpy(cluster_counts_device, cluster_counts, number_of_clusters * sizeof(int), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy cluster_counts failed");

        // Copy cluster assignments to device
        int *cluster_assignments_device;
        checkCudaErrors(cudaMalloc((void **)&cluster_assignments_device, SIZE * sizeof(unsigned int)));
        checkCudaErrors(cudaMemcpy(cluster_assignments_device, cluster_assignments, SIZE * sizeof(unsigned int), cudaMemcpyHostToDevice));
        getLastCudaError("cudaMemcpy cluster_assignments failed");

        cudaEvent_t start, stop;
        float milliseconds = 0;
        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start);

        // Assign each pixel to a cluster
        kmeans_clustering<<<gridSize, blockSize>>>(
            image_device,
            clusters_device,
            cluster_counts_device,
            cluster_assignments_device,
            number_of_clusters,
            width,
            height,
            cpp,
            number_of_iterations);
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

        char output[] = "output_compressed";
        if (cpp == 3) {
            strcat(output, ".jpg");
            printf("%s", output);
            // Check if the image was written
            if (stbi_write_jpg(output,
                               width, height, cpp, image_original, 100)) {
                printf("Image written successfully\n");
            } else {
                printf("Error writing image\n");
            }
        }
        if (cpp == 4) {
            strcat(output, ".png");
            printf("%s", output);
            int stride_in_bytes = width * cpp;
            // Check if the image was written
            if (stbi_write_png(output,
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