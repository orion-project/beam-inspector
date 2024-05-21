#ifndef _PGM_H_
#define _PGM_H_

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://netpbm.sourceforge.net/doc/pgm.html
uint8_t* read_pgm(const char* filename, int *w, int *h, int *offset) {
    printf("Image: %s\n", filename);

    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    rewind(f);

    char* buf = (char*)malloc(size);
    if (!buf) {
        perror("Unable to allocate");
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, size, f) != size) {
        printf("Not all bytes read\n");
    }
    fclose(f);

    char tmp[16];
    char *ch = buf;
    char *tch = tmp;

    while (!isspace(*ch) && ch-buf < size && tch-tmp < 15) {
        *tch = *ch;
        ch++;
        tch++;
    }
    *tch = '\0';
    if (strcmp(tmp, "P5") != 0) {
        printf("Invalid file format\n");
        free(buf);
        return NULL;
    }
    
    while (isspace(*ch) && ch-buf < size && ch-buf < size) ch++;
    tch = tmp;
    while (!isspace(*ch) && tch-tmp < 15) {
        *tch = *ch;
        ch++;
        tch++;
    }
    *tch = '\0';
    *w = atoi(tmp);
    
    while (isspace(*ch) && ch-buf < size) ch++;
    tch = tmp;
    while (!isspace(*ch) && ch-buf < size && tch-tmp < 15) {
        *tch = *ch;
        ch++;
        tch++;
    }
    *tch = '\0';
    *h = atoi(tmp);

    if (*w <= 0 || *h <= 0) {
        printf("Invalid image size: %dx%d\n", *w, *h);
        free(buf);
        return NULL;
    }
    printf("Image size: %dx%d\n", *w, *h);

    while (isspace(*ch) && ch-buf < size) ch++;
    tch = tmp;
    while (!isspace(*ch) && ch-buf < size && tch-tmp < 15) {
        *tch = *ch;
        ch++;
        tch++;
    }
    *tch = '\0';
    int max_val = atoi(tmp);
    if (max_val <= 0 || max_val >= 65536) {
        printf("Invalid max gray: %d\n", max_val);
        free(buf);
        return NULL;
    }
    printf("Max gray: %d\n", max_val);

    ch++;

    *offset = ch - buf;

    if (max_val > 255) {
        // The most significant byte is first in PGM file
        // while the memory has the opposite layout
        // so we need to swap bytes before processing 16-bit images
        char *d = buf + *offset;
        for (int i = 0; i < (*w)*(*h); i++) {
            char c = d[2*i];
            d[2*i] = d[2*i+1];
            d[2*i+1] = c;
        }
    }

    return (uint8_t*)buf;
}

int write_pgm(const char* filename, uint8_t *buf, int w, int h, int bits) {
    FILE *f = fopen(filename, "wb");
    if (!f) return 0;
    fprintf(f, "P5\n%d %d\n%d\n", w, h, (1<<bits)-1);
    if (bits > 8) {
        // The most significant byte is first in PGM file
        // while the memory has the opposite layout
        // so we need to swap bytes before writing 16-bit images
        uint16_t *b = (uint16_t*)buf;
        for (int i = 0; i < w*h; i++) {
            uint16_t v = (b[i] >> 8) | (b[i] << 8);
            fwrite(&v, 2, 1, f);
        }
    } else {
        fwrite(buf, 1, w*h, f);
    }
    fclose(f);
    return 1;
}

#endif
