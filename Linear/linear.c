#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../packages/stb_image.h"
#include "../packages/stb_image_write.h"

int width, height, cpp; // Image qualities
unsigned char *image_original;

// Function parameters
char* image_file;
int number_of_clusters, number_of_iterations;


// Initialize cluster coordinates - pixel X and Y
void init_clusters(char init_clusters[number_of_clusters][cpp]) {
    for (int i = 0; i < number_of_clusters; i++) {
        int random_pixel_w = (i * width) / number_of_clusters;
        int random_pixel_h = (i * height) / number_of_clusters;

        for (int j = 0; j < cpp; j++) {
            init_clusters[i][j] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + j];
        }
        //printf("Init cluster %d\n", i);
    }
}


void k_means_clustering(char *image_original,char clusters_centroids[number_of_clusters][cpp]) {
    // init an array of cluster centroids by assigning them random observations
    int random_pixel_w, random_pixel_h;
    printf("Starting cluster initalization\n");
    fflush(stdout);

    printf("Clusters initialized\n");
    fflush(stdout);

    char *pixel_centroids = (char*)malloc(sizeof(char) * width*height);
    //char pixel_centroids[width * height];

    printf("Pixel centroids initialized with size %d\n", width*height);
    fflush(stdout);
    
    for (int iter = 0; iter < number_of_iterations; iter++) {
        printf("Iteration %d...\n", iter);
        fflush(stdout);
        for (int pxl = 0; pxl < width * height; pxl++) {
            char pxl_red = image_original[pxl * cpp];
            char pxl_green = image_original[pxl * cpp + 1];
            char pxl_blue = image_original[pxl * cpp + 2];
            float min_dst = 513;  // minimum distance from pixel to centroid
            int min_indx = 0;     // centroid index with min distance to pixel
            for (int centroid = 0; centroid < number_of_clusters; centroid++) {
                char cntr_red = clusters_centroids[centroid][0];
                char cntr_green = clusters_centroids[centroid][1];
                char cntr_blue = clusters_centroids[centroid][2];

                // Calculate Eucledeianean distance
                char delta_red = pxl_red - cntr_red;
                char delta_green = pxl_green - cntr_green;
                char delta_blue = pxl_blue - cntr_blue;
                float dst = sqrt(pow(delta_red, 2) + pow(delta_green, 2) + pow(delta_blue, 2));
                if (dst < min_dst) {
                    min_dst = dst;
                    min_indx = centroid;
                }
            }
            pixel_centroids[pxl] = min_indx;
        }
        printf("POU ITERACIJE\n");
        fflush(stdout);

        for (int centroid = 0; centroid < number_of_clusters; centroid++) {
            int sum_red = 0;
            int sum_green = 0;
            int sum_blue = 0;
            int count = 0;
            for (int pxl = 0; pxl < width * height; pxl++) {
                if (pixel_centroids[pxl] == centroid) {
                    sum_red += (int)image_original[pxl * cpp];
                    sum_green += (int)image_original[pxl * cpp + 1];
                    sum_blue += (int)image_original[pxl * cpp + 2];
                    count += 1;
                }
            }
            if (!count) {
                printf("Cluster %d has no pixels that belong to it\n", centroid);
                fflush(stdout);

                // Assign a random pixel to the cluster
                random_pixel_w = rand() % width;
                random_pixel_h = rand() % height;

                clusters_centroids[centroid][0] = image_original[(random_pixel_h * width + random_pixel_w) * cpp];
                clusters_centroids[centroid][1] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + 1];
                clusters_centroids[centroid][2] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + 2];

                pixel_centroids[random_pixel_h * width + random_pixel_w] = centroid;

                // Increase the count and colors 
                sum_red += (int)image_original[(random_pixel_h * width + random_pixel_w) * cpp];
                sum_green += (int)image_original[(random_pixel_h * width + random_pixel_w) * cpp + 1];
                sum_blue += (int)image_original[(random_pixel_h * width + random_pixel_w) * cpp + 2];
                count += 1;

                printf("Assigning random pixel to the cluster: %d\n", random_pixel_h * width + random_pixel_w);
            }
            //printf("NewRed %d, NewGreen %d, NewBlue %d, Count %d\n", sum_red, sum_green, sum_blue, count);
            //fflush(stdout);
            int red = sum_red / count;
            int green = sum_green / count;
            int blue = sum_blue / count;

            clusters_centroids[centroid][0] = red;
            clusters_centroids[centroid][1] = green;
            clusters_centroids[centroid][2] = blue;
        }
    }

    for (int i = 0; i < width * height; i++) {
        image_original[i * cpp] = clusters_centroids[pixel_centroids[i]][0];
        image_original[i * cpp + 1] = clusters_centroids[pixel_centroids[i]][1];
        image_original[i * cpp + 2] = clusters_centroids[pixel_centroids[i]][2];
    }
    printf("KNN Clustering done\n");
    fflush(stdout);
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
        printf("Image loaded with dimensions: %d %d %d\n", width, height, cpp);
        fflush(stdout);

        // Init clusters
        char clusters[number_of_clusters][cpp];
        init_clusters(clusters);

        k_means_clustering(image_original, clusters);

        char output[] = "output_compressed";
        if (cpp == 3) {
            strcat(output, ".jpg");
            printf("%s", output);
            stbi_write_jpg(output,
                           width, height, cpp, image_original, 100);  // channelsPer Pixel = comp channels of data per channel: 1=Y, 2=YA, 3=RGB, 4=RGBA
        }
        if (cpp == 4) {
            strcat(output, ".png");
            printf("%s", output);
            int stride_in_bytes = width * cpp;
            stbi_write_png(output,
                           width, height, cpp, image_original, stride_in_bytes);
        }
    } else {
        fprintf(stderr, "Error loading image %s!\n", image_file);
    }

    return 0;
}