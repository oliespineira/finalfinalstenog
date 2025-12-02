#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "encryption.h"
#include "image_analysis.h"
#include "embedding.h"

#define ENCRYPTED_FOLDER "../encrypted"
#define IMAGE_FOLDER "../image"

typedef struct {
    uint8_t *encrypted;
    size_t enc_len;
    bool *mask;
    int ready_flags;
    pthread_mutex_t mutex;
} shared_data_t;

// encryption thread worker
static void *encrypt_worker(void *arg) {
    void **params = (void **)arg;
    const char *message = (const char *)params[0];
    const char *key = (const char *)params[1];
    shared_data_t *shared = (shared_data_t *)params[2];

    shared->enc_len = encrypt_message(message, key, &shared->encrypted);
    if (shared->enc_len == 0 || !shared->encrypted) {
        printf("❌ encryption failed (memory allocation error)\n");
        pthread_mutex_lock(&shared->mutex);
        shared->ready_flags |= 1;
        pthread_mutex_unlock(&shared->mutex);
        return NULL;
    }

    pthread_mutex_lock(&shared->mutex);
    shared->ready_flags |= 1;
    pthread_mutex_unlock(&shared->mutex);

    printf("✓ encryption complete (%zu bytes)\n", shared->enc_len);
    return NULL;
}

// image analysis thread worker
static void *analysis_worker(void *arg) {
    void **params = (void **)arg;
    uint8_t *image = (uint8_t *)params[0];
    int width = *(int *)params[1];
    int height = *(int *)params[2];
    int channels = *(int *)params[3];
    shared_data_t *shared = (shared_data_t *)params[4];

    shared->mask = find_low_contrast_regions(image, width, height, channels);
    if (!shared->mask) {
        printf("❌ image analysis failed (memory allocation error)\n");
        pthread_mutex_lock(&shared->mutex);
        shared->ready_flags |= 2;
        pthread_mutex_unlock(&shared->mutex);
        return NULL;
    }

    pthread_mutex_lock(&shared->mutex);
    shared->ready_flags |= 2;
    pthread_mutex_unlock(&shared->mutex);

    printf("✓ image analysis complete\n");
    return NULL;
}

// create encrypted folder if it doesn't exist
static void ensure_encrypted_folder(void) {
    struct stat st = {0};
    if (stat(ENCRYPTED_FOLDER, &st) == -1) {
        mkdir(ENCRYPTED_FOLDER, 0700);
    }
}

// get filename from path
static const char *get_filename(const char *path) {
    const char *filename = strrchr(path, '/');
    if (filename) {
        return filename + 1;
    }
    return path;
}

// encoding function
static int encode_image(const char *input_path,
                        const char *message,
                        const char *key,
                        const char *output_path) {
    printf("\n=== ENCODING ===\n");

    int width, height, channels;
    uint8_t *image = stbi_load(input_path, &width, &height, &channels, 3);
    if (!image) {
        printf("❌ failed to load image: %s\n", input_path);
        printf("   make sure the file exists and is a valid PNG/JPG\n");
        return 0;
    }
    channels = 3;

    printf("loaded image: %dx%d with %d channels\n", width, height, channels);

    shared_data_t shared = {0};
    pthread_mutex_init(&shared.mutex, NULL);

    void *encrypt_params[3] = {(void *)message, (void *)key, &shared};
    void *analysis_params[5] = {image, &width, &height, &channels, &shared};

    pthread_t encrypt_thread, analysis_thread;
    pthread_create(&encrypt_thread, NULL, encrypt_worker, encrypt_params);
    pthread_create(&analysis_thread, NULL, analysis_worker, analysis_params);

    pthread_join(encrypt_thread, NULL);
    pthread_join(analysis_thread, NULL);

    printf("✓ both threads completed\n");

    // verify both operations succeeded
    if (!shared.encrypted || shared.enc_len == 0) {
        printf("❌ encryption failed\n");
        if (shared.mask) free(shared.mask);
        stbi_image_free(image);
        pthread_mutex_destroy(&shared.mutex);
        return 0;
    }
    if (!shared.mask) {
        printf("❌ image analysis failed\n");
        free(shared.encrypted);
        stbi_image_free(image);
        pthread_mutex_destroy(&shared.mutex);
        return 0;
    }

    int mask_pixels = 0;
    for (int i = 0; i < width * height; i++) {
        if (shared.mask[i]) {
            mask_pixels++;
        }
    }

    size_t total_size = 4 + shared.enc_len;
    int bits_needed = (int)(total_size * 8);
    int bits_available = mask_pixels * 3;

    printf("embedding capacity: %d bits available, %d bits needed\n",
           bits_available, bits_needed);

    if (bits_available < bits_needed) {
        printf("❌ not enough low-contrast regions! need larger image\n");
        free(shared.encrypted);
        free(shared.mask);
        stbi_image_free(image);
        pthread_mutex_destroy(&shared.mutex);
        return 0;
    }

    embed_message(image, width, height, channels,
                  shared.encrypted, shared.enc_len, shared.mask);

    if (!stbi_write_png(output_path, width, height, channels,
                        image, width * channels)) {
        printf("❌ failed to write output image\n");
        free(shared.encrypted);
        free(shared.mask);
        stbi_image_free(image);
        pthread_mutex_destroy(&shared.mutex);
        return 0;
    }

    printf("✓ message hidden in %s\n", output_path);

    free(shared.encrypted);
    free(shared.mask);
    stbi_image_free(image);
    pthread_mutex_destroy(&shared.mutex);

    return 1;
}

// decoding function (recomputes mask from stego image)
static void decode_image(const char *input_path, const char *key) {
    printf("\n=== DECODING ===\n");

    int width, height, channels;
    uint8_t *image = stbi_load(input_path, &width, &height, &channels, 3);
    if (!image) {
        printf("❌ failed to load image: %s\n", input_path);
        printf("   make sure the file exists and is a valid PNG/JPG\n");
        return;
    }
    channels = 3;

    printf("loaded stego image: %dx%d\n", width, height);

    // recompute mask from stego image (LSB changes don't affect low-contrast detection much)
    printf("analyzing image to find embedding regions...\n");
    bool *mask = find_low_contrast_regions(image, width, height, channels);
    if (!mask) {
        printf("❌ failed to analyze image (memory allocation error)\n");
        stbi_image_free(image);
        return;
    }
    printf("✓ mask computed\n");

    uint8_t *encrypted = NULL;
    size_t enc_len = extract_message(image, width, height, channels,
                                     mask, &encrypted);

    if (enc_len == 0) {
        printf("❌ failed to extract message (image may not contain hidden data)\n");
        free(mask);
        stbi_image_free(image);
        return;
    }

    printf("✓ extracted %zu encrypted bytes\n", enc_len);

    char *message = decrypt_message(encrypted, enc_len, key);
    if (!message) {
        printf("❌ decryption failed (memory allocation error)\n");
        free(encrypted);
        free(mask);
        stbi_image_free(image);
        return;
    }

    printf("\n📩 DECODED MESSAGE:\n");
    printf("   \"%s\"\n", message);

    free(encrypted);
    free(message);
    free(mask);
    stbi_image_free(image);
}

// read a line from stdin (handles spaces)
static void read_line(char *buffer, size_t size) {
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    // remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
}

// check if file has image extension
static int is_image_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    ext++; // skip the dot
    return (strcasecmp(ext, "png") == 0 ||
            strcasecmp(ext, "jpg") == 0 ||
            strcasecmp(ext, "jpeg") == 0 ||
            strcasecmp(ext, "bmp") == 0);
}

// list images in folder and let user select
static int select_image_from_folder(const char *folder, char *selected_path, size_t path_size) {
    DIR *dir = opendir(folder);
    if (!dir) {
        printf("❌ cannot open folder '%s'\n", folder);
        return 0;
    }

    // first pass: count image files
    struct dirent *entry;
    int count = 0;
    char **files = NULL;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && is_image_file(entry->d_name)) {
            files = realloc(files, (count + 1) * sizeof(char *));
            if (!files) {
                closedir(dir);
                return 0;
            }
            files[count] = malloc(strlen(entry->d_name) + 1);
            if (!files[count]) {
                closedir(dir);
                for (int i = 0; i < count; i++) free(files[i]);
                free(files);
                return 0;
            }
            strcpy(files[count], entry->d_name);
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("❌ no image files found in '%s' folder\n", folder);
        if (files) {
            for (int i = 0; i < count; i++) free(files[i]);
            free(files);
        }
        return 0;
    }

    // display list
    printf("\nAvailable images in '%s' folder:\n", folder);
    for (int i = 0; i < count; i++) {
        printf("  %d. %s\n", i + 1, files[i]);
    }
    printf("  0. Enter custom path\n");
    printf("\nSelect image (1-%d, or 0 for custom): ", count);

    char choice[10];
    read_line(choice, sizeof(choice));
    int selection = atoi(choice);

    if (selection == 0) {
        // custom path
        printf("Enter full path to image: ");
        read_line(selected_path, path_size);
        for (int i = 0; i < count; i++) free(files[i]);
        free(files);
        return strlen(selected_path) > 0;
    }

    if (selection < 1 || selection > count) {
        printf("❌ invalid selection\n");
        for (int i = 0; i < count; i++) free(files[i]);
        free(files);
        return 0;
    }

    // build full path
    snprintf(selected_path, path_size, "%s/%s", folder, files[selection - 1]);
    
    // cleanup
    for (int i = 0; i < count; i++) free(files[i]);
    free(files);

    return 1;
}

// read message (can be multiple lines, ends with empty line)
static char *read_message(void) {
    printf("Enter your message (press Enter twice to finish):\n");
    printf("> ");

    char *message = NULL;
    size_t total_size = 0;
    char line[1024];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t line_len = strlen(line);
        if (line_len == 1 && line[0] == '\n') {
            break; // empty line signals end
        }

        message = realloc(message, total_size + line_len + 1);
        if (!message) {
            printf("❌ memory allocation failed\n");
            return NULL;
        }
        memcpy(message + total_size, line, line_len);
        total_size += line_len;
        printf("> ");
    }

    if (message && total_size > 0) {
        // remove trailing newline if present
        if (message[total_size - 1] == '\n') {
            message[total_size - 1] = '\0';
        } else {
            message[total_size] = '\0';
        }
    }

    return message;
}

int main(void) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║   LSB STEGANOGRAPHY (PNG SUPPORT)     ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    ensure_encrypted_folder();
    
    // ensure image folder exists
    struct stat st = {0};
    if (stat(IMAGE_FOLDER, &st) == -1) {
        mkdir(IMAGE_FOLDER, 0700);
        printf("ℹ️  Created '%s' folder - place your images there!\n\n", IMAGE_FOLDER);
    }

    while (1) {
        printf("Select an option:\n");
        printf("  1. Encrypt (hide message in image)\n");
        printf("  2. Decrypt (extract message from image)\n");
        printf("  3. Exit\n");
        printf("\nEnter choice (1-3): ");

        char choice[10];
        read_line(choice, sizeof(choice));

        if (strcmp(choice, "1") == 0) {
            // ENCRYPT MODE
            char image_path[512];
            char key[256];
            char *message = NULL;

            printf("\n--- ENCRYPT MODE ---\n");
            if (!select_image_from_folder(IMAGE_FOLDER, image_path, sizeof(image_path))) {
                printf("\n");
                continue;
            }

            message = read_message();
            if (!message || strlen(message) == 0) {
                printf("❌ no message provided\n\n");
                if (message) free(message);
                continue;
            }

            printf("Enter encryption key: ");
            read_line(key, sizeof(key));

            if (strlen(key) == 0) {
                printf("❌ no key provided, using default\n");
                strcpy(key, "mysecretkey12345");
            }

            // create output filename
            const char *filename = get_filename(image_path);
            char output_path[512];
            snprintf(output_path, sizeof(output_path), "%s/encrypted_%s",
                     ENCRYPTED_FOLDER, filename);

            if (encode_image(image_path, message, key, output_path)) {
                printf("\n✅ Success! Encrypted image saved to: %s\n", output_path);
            } else {
                printf("\n❌ Encryption failed\n");
            }

            free(message);
            printf("\n");

        } else if (strcmp(choice, "2") == 0) {
            // DECRYPT MODE
            char image_path[512];
            char key[256];

            printf("\n--- DECRYPT MODE ---\n");
            printf("Select image to decrypt:\n");
            printf("  1. From '%s' folder\n", ENCRYPTED_FOLDER);
            printf("  2. From '%s' folder\n", IMAGE_FOLDER);
            printf("  3. Enter custom path\n");
            printf("\nEnter choice (1-3): ");
            
            char folder_choice[10];
            read_line(folder_choice, sizeof(folder_choice));
            
            if (strcmp(folder_choice, "1") == 0) {
                if (!select_image_from_folder(ENCRYPTED_FOLDER, image_path, sizeof(image_path))) {
                    printf("\n");
                    continue;
                }
            } else if (strcmp(folder_choice, "2") == 0) {
                if (!select_image_from_folder(IMAGE_FOLDER, image_path, sizeof(image_path))) {
                    printf("\n");
                    continue;
                }
            } else if (strcmp(folder_choice, "3") == 0) {
                printf("Enter full path to encrypted image: ");
                read_line(image_path, sizeof(image_path));
                if (strlen(image_path) == 0) {
                    printf("❌ no image path provided\n\n");
                    continue;
                }
            } else {
                printf("❌ invalid choice\n\n");
                continue;
            }

            printf("Enter encryption key: ");
            read_line(key, sizeof(key));

            if (strlen(key) == 0) {
                printf("❌ no key provided, using default\n");
                strcpy(key, "mysecretkey12345");
            }

            decode_image(image_path, key);
            printf("\n");

        } else if (strcmp(choice, "3") == 0) {
            printf("\nGoodbye!\n");
            break;

        } else {
            printf("❌ invalid choice. Please enter 1, 2, or 3.\n\n");
        }
    }

    return 0;
}
