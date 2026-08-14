#ifndef CALC_CSV_H
#define CALC_CSV_H

#include <stdbool.h>

#include "matrix.h"

bool matrix_read_csv(const char *path, Matrix *matrix);
bool matrix_write_csv(const char *path, const Matrix *matrix);

#endif
