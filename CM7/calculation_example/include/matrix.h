#ifndef CALC_MATRIX_H
#define CALC_MATRIX_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t rows;
    size_t columns;
    double *values;
} Matrix;

void matrix_init(Matrix *matrix);
void matrix_destroy(Matrix *matrix);
bool matrix_create(Matrix *matrix, size_t rows, size_t columns);
bool matrix_copy(const Matrix *source, Matrix *destination);
double *matrix_value(Matrix *matrix, size_t row, size_t column);
const double *matrix_const_value(const Matrix *matrix, size_t row, size_t column);
size_t matrix_element_count(const Matrix *matrix);

#endif
