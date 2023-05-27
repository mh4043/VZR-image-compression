#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"


void k_means_clustering(char *image_original, int width, int height, int cpp, int clusters, int iterations){
    // init an array of cluster centroids by assigning them random observations
    char clusters_centroids[clusters][cpp]; // array of centroids of clusters which hold pixels as arrays ([R, G, B, A])
    int random_pixel_w, random_pixel_h;
    for(int i = 0; i < clusters; i++){
        random_pixel_w = rand() % width;
        random_pixel_h = rand() % height;
        for(int j = 0; j < cpp; j++){
            clusters_centroids[i][j] = image_original[(random_pixel_h * width + random_pixel_w) * cpp + j];
        }
    }
    
    int pixel_centroids[width * height];
    for(int iter = 0; iter < iterations; iter++){
        for(int pxl = 0; pxl < width * height; pxl++){
            char pxl_red = image_original[pxl * cpp];
            char pxl_green = image_original[pxl * cpp + 1];
            char pxl_blue = image_original[pxl * cpp + 2];
            float min_dst = 513; // minimum distance from pixel to centroid
            int min_indx = 0; // centroid index with min distance to pixel
            for(int centroid = 0; centroid < clusters; centroid ++){
                char cntr_red = clusters_centroids[centroid][0];
                char cntr_green = clusters_centroids[centroid][1];
                char cntr_blue = clusters_centroids[centroid][2];

                // Calculate Eucledeianean distance
                char delta_red = pxl_red - cntr_red;
                char delta_green = pxl_green - cntr_green;
                char delta_blue = pxl_blue - cntr_blue;
                float dst = sqrt(pow(delta_red, 2) + pow(delta_green, 2) + pow(delta_blue, 2));
                if(dst < min_dst){
                    min_dst = dst;
                    min_indx = centroid;
                }
            }
            pixel_centroids[pxl] = min_indx;
        }
        printf("POU ITERACIJE\n");
        for(int centroid = 0; centroid < clusters; centroid ++){
            int sum_red = 0;
            int sum_green = 0;
            int sum_blue = 0;
            int count = 0;
            for(int pxl = 0; pxl < width * height; pxl ++){
                if(pixel_centroids[pxl] == centroid){
                    sum_red += image_original[pxl * cpp];
                    sum_green += image_original[pxl * cpp + 1];
                    sum_blue += image_original[pxl * cpp + 2];
                    count += 1;
                }
            }
            int red = sum_red / count;
            int green = sum_green / count;
            int blue = sum_blue / count;

            clusters_centroids[centroid][0] = red;
            clusters_centroids[centroid][1] = green;
            clusters_centroids[centroid][2] = blue;
        }   
    }

    for(int i = 0; i < width * height; i++){
        image_original[i*cpp] = clusters_centroids[pixel_centroids[i]][0];
        image_original[i*cpp + 1] = clusters_centroids[pixel_centroids[i]][1];
        image_original[i*cpp + 2] = clusters_centroids[pixel_centroids[i]][2];
    }
}

int main(int argc, char* argv[]){
    char *image_file;
    int number_of_clusters; // = targeted number of colors
    int number_of_iterations;

    srand(time(NULL));

    if (argc > 3){
        image_file = argv[1];
        number_of_clusters = atoi(argv[2]);
        number_of_iterations = atoi(argv[3]);
    }
    else{
        fprintf(stderr, "Not enough arguments\n");
        fprintf(stderr, "Usage: %s <IMAGE_PATH> <number of colors> <number of iterations>\n", argv[0]);
        exit(1);
    }

    int width, height, cpp; //image width, image height, image cpp = channels per pixel (4)
    unsigned char *image_original = stbi_load(image_file, &width, &height, &cpp, 0);

    if(image_original){
        k_means_clustering(image_original, width, height, cpp, number_of_clusters, number_of_iterations);

        char *dotChar = strstr(image_file, ".");
        int iend = dotChar - image_file;
        char newImageFileName[iend+15]; //+ _compressed.jpg 
        strncpy(newImageFileName, image_file, iend);
        if(cpp == 3){
            strcat(newImageFileName, "_compressed.jpg");
            printf("newImageFileName %s\n", newImageFileName);
            stbi_write_jpg(newImageFileName,
            width, height, cpp, image_original, 100); //channelsPer Pixel = comp channels of data per channel: 1=Y, 2=YA, 3=RGB, 4=RGBA
        }
        if(cpp == 4){
            strcat(newImageFileName, "_compressed.png");
            printf("newImageFileName %s\n", newImageFileName);
            int stride_in_bytes = width * cpp;
            printf("stride_in_bytes %d\n", stride_in_bytes);
            stbi_write_png(newImageFileName,
            width, height, cpp, image_original, stride_in_bytes);
        }
    }
    else{
        fprintf(stderr, "Error loading image %s!\n", image_file);
    }

    return 0;
}