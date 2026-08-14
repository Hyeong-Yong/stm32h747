#include "csv.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static bool read_number(const char **cursor, double *value) {
    char *end;

    while (isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }
    errno = 0;
    *value = strtod(*cursor, &end);
    if (end == *cursor || errno == ERANGE || !isfinite(*value)) {
        return false;
    }
    *cursor = end;
    return true;
}

static bool append_value(double **values, size_t *count, size_t *capacity, double value) {
    double *new_values;
    size_t new_capacity;

    if (*count < *capacity) {
        (*values)[(*count)++] = value;
        return true;
    }
    new_capacity = *capacity == 0 ? 16 : *capacity * 2;
    if (new_capacity < *capacity || new_capacity > SIZE_MAX / sizeof(double)) {
        return false;
    }
    new_values = (double *)realloc(*values, new_capacity * sizeof(double));
    if (new_values == NULL) {
        return false;
    }
    *values = new_values;
    *capacity = new_capacity;
    (*values)[(*count)++] = value;
    return true;
}

bool matrix_read_csv(const char *path, Matrix *matrix) {
    FILE *file;
    char line[1024 * 1024];
    double *values = NULL;
    size_t value_count = 0;
    size_t capacity = 0;
    size_t rows = 0;
    size_t columns = 0;

    matrix_init(matrix);
    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Cannot open '%s'.\n", path);
        return false;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        const char *cursor = line;
        size_t line_columns = 0;
        double value;

        if (strchr(line, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "Line in '%s' is too long.\n", path);
            goto failure;
        }
        while (true) {
            while (isspace((unsigned char)*cursor)) {
                cursor++;
            }
            if (*cursor == '\0') {
                break;
            }
            if (!read_number(&cursor, &value) ||
                !append_value(&values, &value_count, &capacity, value)) {
                fprintf(stderr, "Invalid number or out of memory in '%s'.\n", path);
                goto failure;
            }
            line_columns++;
            while (isspace((unsigned char)*cursor)) {
                cursor++;
            }
            if (*cursor == ',') {
                cursor++;
                continue;
            }
            if (*cursor != '\0') {
                fprintf(stderr, "Expected comma in '%s'.\n", path);
                goto failure;
            }
            break;
        }
        if (line_columns == 0) {
            continue;
        }
        if (columns == 0) {
            columns = line_columns;
        } else if (columns != line_columns) {
            fprintf(stderr, "Rows in '%s' have different lengths.\n", path);
            goto failure;
        }
        rows++;
    }
    if (ferror(file) || rows == 0 || columns == 0) {
        fprintf(stderr, "'%s' is empty or could not be read.\n", path);
        goto failure;
    }
    fclose(file);
    matrix->rows = rows;
    matrix->columns = columns;
    matrix->values = values;
    return true;

failure:
    fclose(file);
    free(values);
    return false;
}

bool matrix_write_csv(const char *path, const Matrix *matrix) {
    FILE *file = fopen(path, "w");
    size_t row;
    size_t column;

    if (file == NULL) {
        fprintf(stderr, "Cannot write '%s'.\n", path);
        return false;
    }
    for (row = 0; row < matrix->rows; row++) {
        for (column = 0; column < matrix->columns; column++) {
            if (column > 0) {
                fputs(", ", file);
            }
            fprintf(file, "%.17g", *matrix_const_value(matrix, row, column));
        }
        fputc('\n', file);
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "Cannot finish writing '%s'.\n", path);
        return false;
    }
    return true;
}
