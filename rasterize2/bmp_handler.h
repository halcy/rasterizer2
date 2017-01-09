#ifndef __BMP_HANDLER_H__
#define __BMP_HANDLER_H__

/**
* BMP output and input. 24 bit colour only.
*
* (c) L. Diener 2016
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// BMP file info struct
typedef struct {
    // Internal use
    FILE* bmp_file;
    int line_pos;
    int line_max;
    
    // For external checking
    int x_size;
    int y_size;
} bmp_info;

// BMP header
#pragma pack (1)
typedef struct bmp_header {
    uint8_t file_type[2]; // "BM"
    uint32_t file_size;
    uint16_t reserved_1; // 0
    uint16_t reserved_2; // 0
    uint32_t pixel_offset; // 54
    uint32_t header_size; // 40
    uint32_t x_size;
    uint32_t y_size;
    uint16_t planes; // 1
    uint16_t bpp; // 24
    uint32_t compression; // 0
    uint32_t image_size; // 0
    uint32_t x_ppm; // 0
    uint32_t y_ppm; // 0
    uint32_t used_colors; // 0
    uint32_t important_colors; // 0
} bmp_header;

// Open BMP for writing and write header
bmp_info* bmp_open_write(const char* file_name, int x_size, int y_size) {
    // Make us a header.
    bmp_header file_head;
    file_head.file_type[0] = 'B';
    file_head.file_type[1] = 'M';
    file_head.file_size = x_size * y_size * 3 + 54;
    file_head.reserved_1 = 0;
    file_head.reserved_2 = 0;
    file_head.pixel_offset = 54;
    file_head.header_size = 40;
    file_head.x_size = x_size;
    file_head.y_size = y_size;
    file_head.planes = 1;
    file_head.bpp = 24;
    file_head.compression = 0;
    file_head.image_size = 0;
    file_head.x_ppm = 0;
    file_head.y_ppm = 0;
    file_head.used_colors = 0;
    file_head.important_colors = 0;

    // Create info struct
    bmp_info* info = (bmp_info*)malloc(sizeof(bmp_info));

    // Write file header.
#ifdef _WIN32
    fopen_s(&info->bmp_file, file_name, "wb");
#else
    info->bmp_file = fopen(file_name, "wb");
#endif
    fwrite((char*)(&file_head), 1, sizeof(file_head), info->bmp_file);

    // Fill info struct and return
    info->line_max = x_size * 3;
    info->line_pos = 0;

    info->x_size = x_size;
    info->y_size = y_size;

    return info;
}


// Open a BMP file for reading and get size
bmp_info* bmp_open_read(const char* file_name) {
    // Create info struct
    bmp_info* info = (bmp_info*)malloc(sizeof(bmp_info));

    // Read file header.
#ifdef _WIN32
    fopen_s(&info->bmp_file, file_name, "rb");
#else
    info->bmp_file = fopen(file_name, "rb");
#endif
    bmp_header file_head;
    if(fread((char*)(&file_head), 1, sizeof(file_head), info->bmp_file) == 0) {
        printf("Warning: Error in bmp read.\n");
    }

    // Set up data for reading
    info->line_max = file_head.x_size * 3;
    info->line_pos = 0;

    // Return
    info->x_size = file_head.x_size;
    info->y_size = file_head.y_size;

    return info;
}

// Write a single pixel. r, g and b between 0 and 255.
void bmp_write_pixel(bmp_info* info, int r, int g, int b) {
    char tmp_r = (char)r;
    char tmp_g = (char)g;
    char tmp_b = (char)b;
    fwrite(&tmp_b, 1, 1, info->bmp_file);
    fwrite(&tmp_g, 1, 1, info->bmp_file);
    fwrite(&tmp_r, 1, 1, info->bmp_file);

    info->line_pos += 3;
    if (info->line_pos == info->line_max) {
        while (info->line_pos % 4 != 0) {
            fwrite(&tmp_b, 1, 1, info->bmp_file);
            info->line_pos++;
        }
        info->line_pos = 0;
    }
}

// Read a single BMP pixel. r, g and b between 0 and 255.
void bmp_read_pixel(bmp_info* info, int* r, int* g, int* b) {
    unsigned char tmp_r;
    unsigned char tmp_g;
    unsigned char tmp_b;

    size_t read = 0;
    read += fread(&tmp_b, 1, 1, info->bmp_file);
    read += fread(&tmp_g, 1, 1, info->bmp_file);
    read += fread(&tmp_r, 1, 1, info->bmp_file);

    if(read != 3) {
        printf("Warning: Error in pixel read.\n");
    }

    info->line_pos += 3;
    int tmp_x;
    if (info->line_pos == info->line_max) {
        while(info->line_pos % 4 != 0) {
            if(fread(&tmp_x, 1, 1, info->bmp_file) == 0) {
                printf("Warning: Error in pixel read.\n");
            }
            info->line_pos++;
        }
        info->line_pos = 0;
    }

    *r = tmp_r;
    *g = tmp_g;
    *b = tmp_b;
}

// Flush file, close and free
void bmp_close(bmp_info* info) {
    fflush(info->bmp_file);
    fclose(info->bmp_file);

    free(info);
}

#endif