#include "kernel.h"
#include <string.h>
#include <sys/mman.h>

int generate_pagefault() {
    char* path = "fault.bin";
    struct image img = {NULL, 1024, 1024};
    size_t bytes = (size_t)img.width * img.height * sizeof(struct pixel);
    img.pixels = calloc(1, bytes);
    if (!img.pixels) return -1;
    if (saveimage_mmap(path, &img) != 0) {
        free(img.pixels);
        return -1;
    }
    free(img.pixels);

    // Evict the file from the page cache so touching the mapping needs disk I/O.
    int fd = open(path, O_RDONLY);
    if (fd != -1) {
        fsync(fd);
        posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        close(fd);
    }

    struct image mapped = {NULL, 1024, 1024};
    if (loadimage_mmap(path, &mapped) != 0) return -1;
    volatile long sum = 0;
    for (size_t i = 0; i < (size_t)mapped.width * mapped.height; i += 1024) {
        sum += mapped.pixels[i].r;
    }
    munmap((char*)mapped.pixels - sizeof(struct image), sizeof(struct image) + bytes);
    unlink(path);
    return 0;
}

int main(int argc, char** argv){
    // TODO: parse the arguments in argv. 
    // You can expect argv[1] to be the mode
    // You can expect argv[2] to be the filepath
    // You can expect argv[3] to be the integer width
    // You can expect argv[4] to be the integer height
    // You can expect argv[5] to be the output filepath.
    char* mode = argv[1];
    char* input_filepath = argv[2];
    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    char* output_filepath = argv[5];

    if(argc != 6) {
        printf("Incorrect number of arguments. Expected: ./cli <MODE=kernel|mmap|convert|uconvert|fault> <input_image> <width> <height> <output_image_path>\n");
        return -1;
    }

    // TODO: call correct function based on mode
    if (strcmp(mode, "fault") == 0) {
        return generate_pagefault();
    }
    int is_mmap = strcmp(mode, "mmap") == 0;
    int is_convert = strcmp(mode, "convert") == 0;
    int is_uconvert = strcmp(mode, "uconvert") == 0;
    if (strcmp(mode, "kernel") != 0 && !is_mmap && !is_convert && !is_uconvert) {
        printf("Unknown mode: %s\n", mode);
        return -1;
    }

    // TODO: allocate the space needed for one image and load the image
    struct image* img = malloc(sizeof(struct image));
    if (!img) return -1;
    img->width = width;
    img->height = height;
    img->pixels = NULL;
    // loadimage allocates the pixels itself; the mmap loader maps them instead.
    int mapped = is_mmap || is_uconvert;
    int rc = mapped ? loadimage_mmap(input_filepath, img) : loadimage(input_filepath, img);
    if (rc != 0) {
        free(img);
        return -1;
    }
    
    size_t map_len = sizeof(struct image) + (size_t)width * height * sizeof(struct pixel);
    void* map_base = (char*)img->pixels - sizeof(struct image);
    int result = 0;

    int kernel[3][3] = {{1,1,1},{1,1,1},{1,1,1}};

    // TODO: call apply kernel with 1/9 (as a float) as the normalization value
    if (is_convert) {
        result = saveimage_mmap(output_filepath, img);
    } else if (is_uconvert) {
        result = saveimage(output_filepath, img);
    } else {
        struct image* out = apply_kernel(img, (int*)kernel, 3, 1.0f / 9.0f);
        if (!out) {
            result = -1;
        } else {
            result = saveimage(output_filepath, out);
            free(out->pixels);
            free(out);
        }
    }

    if (mapped) {
        munmap(map_base, map_len);
    } else {
        free(img->pixels);
    }
    free(img);
    return result == 0 ? 0 : -1;
}
