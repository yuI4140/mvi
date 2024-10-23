#pragma once
#define BYTE_SIZE 256
char *buffer_push_char(char *buffer, char ch);
char **split_into(const char *str, char delim);
char *read_file(const char *filename);
#ifdef YUIS_UTILS
#include <string.h>
#include <stdlib.h>
char *read_file(const char *filename){
    FILE *f=fopen(filename,"rb");
    fseek(f,0,SEEK_END);
    size_t fsz=ftell(f);
    fseek(f,0,SEEK_SET);
    char *buffer=malloc(fsz + 1);
    int r=fread(buffer,1,fsz,f);
    if (r==0) {
       fprintf(stderr, "Error reading file\n"); 
       return NULL;
    }
    return buffer;
}
char *buffer_push_char(char *buffer, char ch) {
    size_t idx = strlen(buffer);
    if (ch == '\0') {
        buffer[0] = ch;
    } else {
        buffer[idx] = ch;
        buffer[idx + 1] = '\0';
    }
    return buffer;
}

// Function to split a string into an array of strings based on a delimiter
char **split_into(const char *str, char delim) {
    char *buffer = malloc(BYTE_SIZE);
    char **result = malloc(BYTE_SIZE * 4096 * sizeof(char *));
    size_t idx_str = 0, idx_res = 0;
    while (1) {
        if (str[idx_str] == '\0') {
            result[idx_res] = strdup(buffer);
            break;
        } else if (str[idx_str] == delim) {
            result[idx_res++] = strdup(buffer);
            buffer_push_char(buffer, '\0');
            idx_str++;
        } else {
            buffer_push_char(buffer, str[idx_str++]);
        }
    }
    return result;
}
#endif /* ifdef YUIS_UTILS */
