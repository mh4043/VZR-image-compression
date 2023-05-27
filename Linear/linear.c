#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../packages/stb_image_write.h"
#include "../packages/stb_image.h"

void k_means_clustering(char *image_original, int width, int height, int cpp, int clusters, int iterations) {
    // init an array of cluster centroids by assigning them random observations
    char clusters_centroids[clusters][cpp]; // array of centroids of clusters which hold pixels as arrays ([R, G, B, A])
    int random_pixel_w, random_pixel_h;
    for(int i = 0; i < clusters; i++) {
        random_pixel_w = rand() % width;
        random_pixel_h = rand() % height;
        printf("random W: %d, random H: %d\n", random_pixel_w, random_pixel_h);
    }

    /*
    for(int iter = 0; iter < iterations; iter++)
    {
        
    }
    */
}

int main(int argc, char* argv[]) {
    char *image_file;
    int number_of_clusters; // = targeted number of colors
    int number_of_iterations;

    srand(time(NULL));

    if (argc > 2) {
        image_file = argv[1];
        number_of_clusters = atoi(argv[2]);
        number_of_iterations = atoi(argv[3]);
    }
    else {
        fprintf(stderr, "Not enough arguments\n");
        fprintf(stderr, "Usage: %s <IMAGE_PATH> <number of colors> <number of iterations>\n", argv[0]);
        exit(1);
    }

    int width, height, cpp; //image width, image height, image cpp = channels per pixel (4)
    unsigned char *image_original = stbi_load(image_file, &width, &height, &cpp, 0);

    if(image_original){
        k_means_clustering(image_original, width, height, cpp, number_of_clusters, number_of_iterations);
    }
    else {
        fprintf(stderr, "Error loading image %s!\n", image_file);
    }

    return 0;
}