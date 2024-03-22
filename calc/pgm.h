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
    return (uint8_t*)buf;
}

#endif
