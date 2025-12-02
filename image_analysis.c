#include "image_analysis.h"

#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define NUM_THREADS 4
#define BLOCK_SIZE 8

typedef struct {
    uint8_t *image;
    uint8_t *gray;
    bool *mask;
    int width;
    int height;
    int channels;
    int start_row;
    int end_row;
    float global_median;
} thread_data_t;

// calculate median of a small array (in-place bubble sort, fine for 8x8 blocks)
static float calculate_small_median(float *arr, int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                float temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
    return arr[size / 2];
}

// calculate global median for grayscale image using histogram (0-255)
static float calculate_global_median(const uint8_t *gray, int width, int height) {
    unsigned long hist[256] = {0};
    unsigned long total = (unsigned long)width * (unsigned long)height;

    for (int i = 0; i < width * height; i++) {
        hist[gray[i]]++;
    }

    unsigned long mid = total / 2;
    unsigned long cum = 0;
    for (int v = 0; v < 256; v++) {
        cum += hist[v];
        if (cum >= mid) {
            return (float)v;
        }
    }
    return 0.0f;
}

// calculate standard deviation
static float calculate_std(float *arr, int size, float mean) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        float d = arr[i] - mean;
        sum += d * d;
    }
    return sqrtf(sum / (float)size);
}

// thread worker for analyzing image regions
static void *analyze_region(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    for (int y = data->start_row; y < data->end_row - BLOCK_SIZE; y++) {
        for (int x = 0; x < data->width - BLOCK_SIZE; x += BLOCK_SIZE) {
            // extract block
            float block[BLOCK_SIZE * BLOCK_SIZE];
            int idx = 0;

            for (int by = 0; by < BLOCK_SIZE; by++) {
                for (int bx = 0; bx < BLOCK_SIZE; bx++) {
                    int py = y + by;
                    int px = x + bx;
                    block[idx++] = (float)data->gray[py * data->width + px];
                }
            }

            // calculate statistics
            float sum = 0.0f;
            for (int i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                sum += block[i];
            }
            float mean = sum / (float)(BLOCK_SIZE * BLOCK_SIZE);

            float local_median = calculate_small_median(block, BLOCK_SIZE * BLOCK_SIZE);
            float local_std = calculate_std(block, BLOCK_SIZE * BLOCK_SIZE, mean);

            // check if low contrast
            if (fabsf(local_median - data->global_median) < 50.0f &&
                local_std < 20.0f && local_std > 5.0f) {
                // mark block as suitable
                for (int by = 0; by < BLOCK_SIZE; by++) {
                    for (int bx = 0; bx < BLOCK_SIZE; bx++) {
                        int py = y + by;
                        int px = x + bx;
                        data->mask[py * data->width + px] = true;
                    }
                }
            }
        }
    }

    return NULL;
}

bool *find_low_contrast_regions(uint8_t *image,
                                int width,
                                int height,
                                int channels) {
    // convert to grayscale
    uint8_t *gray = (uint8_t *)malloc((size_t)width * (size_t)height);
    if (!gray) {
        return NULL; // allocation failed
    }
    for (int i = 0; i < width * height; i++) {
        if (channels == 3) {
            gray[i] = (uint8_t)((image[i * 3] + image[i * 3 + 1] + image[i * 3 + 2]) / 3);
        } else {
            gray[i] = image[i];
        }
    }

    // calculate global median using histogram (no recursion / huge allocations)
    float global_median = calculate_global_median(gray, width, height);

    // create mask
    bool *mask = (bool *)calloc((size_t)width * (size_t)height, sizeof(bool));
    if (!mask) {
        free(gray);
        return NULL; // allocation failed
    }

    // create threads
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    int rows_per_thread = height / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].image = image;
        thread_data[i].gray = gray;
        thread_data[i].mask = mask;
        thread_data[i].width = width;
        thread_data[i].height = height;
        thread_data[i].channels = channels;
        thread_data[i].start_row = i * rows_per_thread;
        thread_data[i].end_row = (i == NUM_THREADS - 1)
                                     ? height
                                     : (i + 1) * rows_per_thread;
        thread_data[i].global_median = global_median;

        pthread_create(&threads[i], NULL, analyze_region, &thread_data[i]);
    }

    // wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    free(gray);

    return mask;
}


