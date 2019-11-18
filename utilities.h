#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define NORMAL_EXIT 0

#define BASE 10
#define CHAR_BUFFER 80
#define ARRAY_BUFFER 10

#define READ_END 0
#define WRITE_END 1


/* Utilities */
char* string_of(int num, char** line);
int read_int(char* line);
char* read_line(FILE* toRead, char** line);
bool check_name(char* name);

#endif // _UTILITIES_H_