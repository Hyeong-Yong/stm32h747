/*
 * oct_matrix.c
 */

#include "oct_matrix.h"

#include <stdint.h>
#include <stdlib.h>

void matrix_init(Matrix *matrix)
{
  matrix->rows    = 0;
  matrix->columns = 0;
  matrix->values  = NULL;
}

void matrix_destroy(Matrix *matrix)
{
  free(matrix->values);
  matrix_init(matrix);
}

bool matrix_create(Matrix *matrix, size_t rows, size_t columns)
{
  size_t count;

  matrix_init(matrix);
  if (rows == 0 || columns == 0 || rows > SIZE_MAX / columns)
  {
    return false;
  }
  count = rows * columns;
  if (count > SIZE_MAX / sizeof(float))
  {
    return false;
  }
  matrix->values = (float *)calloc(count, sizeof(float));
  if (matrix->values == NULL)
  {
    return false;
  }
  matrix->rows    = rows;
  matrix->columns = columns;
  return true;
}

bool matrix_copy(const Matrix *source, Matrix *destination)
{
  size_t count;

  if (source == destination)
  {
    return true;
  }
  matrix_destroy(destination);
  if (matrix_create(destination, source->rows, source->columns) == false)
  {
    return false;
  }
  count = matrix_element_count(source);
  for (size_t index = 0; index < count; index++)
  {
    destination->values[index] = source->values[index];
  }
  return true;
}

float *matrix_value(Matrix *matrix, size_t row, size_t column)
{
  return &matrix->values[row * matrix->columns + column];
}

const float *matrix_const_value(const Matrix *matrix, size_t row, size_t column)
{
  return &matrix->values[row * matrix->columns + column];
}

size_t matrix_element_count(const Matrix *matrix)
{
  return matrix->rows * matrix->columns;
}
