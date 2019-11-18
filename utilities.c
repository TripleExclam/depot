#include "utilities.h"

/* Convert an integer into a string.
 *
 * @param num - The number to stringify.
 * @param line - The line to store a string in.
 */
char* string_of(int num, char** line) {
    int length = 1;
    // Calculate the number of digits.
    while ((num / (BASE * length)) != 0) {
        length++;
    }
    (*line) = malloc(sizeof(char) * (length + 1));
    sprintf(*line, "%d", num);
    return (*line);
}

/* Convert some characters into an integer.
 * Returns -1 if this fails.
 *
 * @param line - The characters to turn into an integer.
 */
int read_int(char* line) {
    if (line == NULL) {
        return -1;
    }
    char* error;
    int num;
    num = strtol(line, &error, BASE);
    if (strlen(error) > 0) {
        // Any non-integer characters read
        return -1;
    }
    return num;
}

/* Read a line of text
 *
 * @param f The stream to read from
 * @param line A variable to save to
 * @return The line that is read
 */
char* read_line(FILE* toRead, char** line) {
    int reading;
    int lineL = 0;
    int charCount = CHAR_BUFFER;
    *line = malloc(sizeof(char) * charCount);
    while ((reading = fgetc(toRead)) != '\n') {
        // Handle EOF seperately to \n
        if (reading == EOF) {
            free(*line);  
            return NULL;
        }
        (*line)[lineL++] = reading;
        // Check if more memory is needed.
        if (lineL + 1 >= charCount) {
            charCount *= 2;
            *line = realloc(*line, sizeof(char) * charCount);
        }
    }
    (*line)[lineL] = '\0';

    return *line;
}

/* Check if a name is valid
 *
 * @param name A name to check
 */
bool check_name(char* name) {
    for (int i = 0; i < strlen(name); i++) {
        if (name[i] == '\n' || name[i] == '\r' || name[i] == ' ' 
                || name[i] == ':') {
            return false;
        }
    } 
    return strlen(name) > 0;
}
