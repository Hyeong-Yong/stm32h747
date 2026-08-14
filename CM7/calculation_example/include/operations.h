#ifndef CALC_OPERATIONS_H
#define CALC_OPERATIONS_H

#include <stdbool.h>

#include "matrix.h"

bool matrix_add(const Matrix *left, const Matrix *right, Matrix *result);
bool matrix_sqr(Matrix *matrix);
bool matrix_vandermonde(const Matrix *x, const Matrix *y, Matrix *v);
bool matrix_least_squares(const Matrix *a, const Matrix *b, Matrix *x);
bool matrix_lsm(const Matrix *a, const Matrix *b, Matrix *x);
bool matrix_calibration(const Matrix *x, const Matrix *y, Matrix *params);
bool matrix_extract_phase(const Matrix *x, const Matrix *y, const Matrix *params, Matrix *phase);
bool matrix_meter_current(const Matrix *phase, double scale, Matrix *current);

#endif
