#ifndef FILELIB_H
#define FILELIB_H

char *read_file(const char *filename);

#ifdef FILELIB_IMPLEMENTATION

#include<stdio.h>
#include<stdlib.h>

// Reads the entire contents of a file into a dynamically allocated string.
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        perror("ftell");
        fclose(file);
        return NULL;
    }

    rewind(file);

    // Allocate buffer (+1 for '\0')
    char *buffer = malloc(size + 1);
    if (!buffer) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, file);
    if (read != (size_t)size) {
        perror("fread");
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[size] = '\0'; // null-terminate it
    fclose(file);
    return buffer;
}

#endif // FILELIB_IMPLEMENTATION

#endif // FILELIB_H
