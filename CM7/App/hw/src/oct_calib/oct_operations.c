/*
 * oct_operations.c
 */

#include "oct_operations.h"

#include <stddef.h>
#include <stdint.h>
#include <float.h>
#include <math.h>

bool matrix_sqr(Matrix *matrix)
{
  size_t count = matrix_element_count(matrix);

  for (size_t index = 0; index < count; index++)
  {
    matrix->values[index] *= matrix->values[index];
  }
  return true;
}

bool matrix_vandermonde(const Matrix *x, const Matrix *y, Matrix *v)
{
  matrix_init(v);
  if (x->columns != 1 || y->columns != 1 || x->rows != y->rows || x->rows == 0)
  {
    return false;
  }
  if (matrix_create(v, x->rows, 5) == false)
  {
    return false;
  }
  for (size_t row = 0; row < x->rows; row++)
  {
    float x_value = *matrix_const_value(x, row, 0);
    float y_value = *matrix_const_value(y, row, 0);

    *matrix_value(v, row, 0) = y_value * y_value;
    *matrix_value(v, row, 1) = x_value * y_value;
    *matrix_value(v, row, 2) = x_value;
    *matrix_value(v, row, 3) = y_value;
    *matrix_value(v, row, 4) = 1.0f;
  }
  return true;
}

// - 1xN 행벡터: N 반환, Nx1 열벡터: N 반환
// - 둘 다 아님(예: 정사각): 0 반환("벡터 아님" 표시)
static size_t vector_length(const Matrix *matrix)
{
  if (matrix->rows == 1)
  {
    return matrix->columns;
  }
  if (matrix->columns == 1)
  {
    return matrix->rows;
  }
  return 0;
}

// 행벡터/열벡터 무관 - index번째 값 읽기.
static float vector_value(const Matrix *vector, size_t index)
{
  if (vector->rows == 1)
  {
    return *matrix_const_value(vector, 0, index);
  }
  return *matrix_const_value(vector, index, 0);
}

bool matrix_least_squares(const Matrix *a, const Matrix *b, Matrix *x)
{
  // 정규방정식 A^T A x = A^T b 로 최소자승 문제 A x = b 풀이.
  size_t    variables = a->columns;
  size_t    augmentedColumns;
  size_t    length = vector_length(b);
  Matrix normal;
  float     scale = 0.0f;

  matrix_init(x);
  matrix_init(&normal);
  if (length == 0 || length != a->rows)
  {
    return false;
  }
  if (variables == SIZE_MAX)
  {
    return false;
  }
  augmentedColumns = variables + 1;
  if (matrix_create(&normal, variables, augmentedColumns) == false)
  {
    return false;
  }

  for (size_t row = 0; row < a->rows; row++)
  {
    for (size_t column = 0; column < variables; column++)
    {
      float coefficient = *matrix_const_value(a, row, column);

      if (fabsf(coefficient) > scale)
      {
        scale = fabsf(coefficient);
      }
      for (size_t pivot = 0; pivot < variables; pivot++)
      {
        *matrix_value(&normal, column, pivot) += coefficient * *matrix_const_value(a, row, pivot);
      }
      *matrix_value(&normal, column, variables) += coefficient * vector_value(b, row);
    }
  }

  if (scale == 0.0f)
  {
    // A에 0이 아닌 계수가 하나도 없음
    matrix_destroy(&normal);
    return false;
  }

  for (size_t column = 0; column < variables; column++)
  {
    size_t best      = column;
    float  bestValue = fabsf(*matrix_const_value(&normal, column, column));

    for (size_t row = column + 1; row < variables; row++)
    {
      float candidate = fabsf(*matrix_const_value(&normal, row, column));
      if (candidate > bestValue)
      {
        best      = row;
        bestValue = candidate;
      }
    }
    if (bestValue <= FLT_EPSILON * scale * scale * 100.0f)
    {
      // A^T A가 특이행렬 - 유일해가 존재하지 않음
      matrix_destroy(&normal);
      return false;
    }
    if (best != column)
    {
      for (size_t pivot = column; pivot <= variables; pivot++)
      {
        float temporary = *matrix_value(&normal, column, pivot);
        *matrix_value(&normal, column, pivot) = *matrix_value(&normal, best, pivot);
        *matrix_value(&normal, best, pivot)   = temporary;
      }
    }
    for (size_t row = column + 1; row < variables; row++)
    {
      float factor = *matrix_const_value(&normal, row, column) / *matrix_const_value(&normal, column, column);

      for (size_t pivot = column; pivot <= variables; pivot++)
      {
        *matrix_value(&normal, row, pivot) -= factor * *matrix_const_value(&normal, column, pivot);
      }
    }
  }

  if (matrix_create(x, variables, 1) == false)
  {
    matrix_destroy(&normal);
    return false;
  }
  for (size_t row = variables; row-- > 0;)
  {
    float right = *matrix_const_value(&normal, row, variables);

    for (size_t column = row + 1; column < variables; column++)
    {
      right -= *matrix_const_value(&normal, row, column) * *matrix_const_value(x, column, 0);
    }
    *matrix_value(x, row, 0) = right / *matrix_const_value(&normal, row, row);
  }

  matrix_destroy(&normal);
  return true;
}

bool matrix_calibration(const Matrix *x, const Matrix *y, Matrix *params)
{
  Matrix vandermonde;
  Matrix xSquared;
  Matrix solution;
  float     b, c, d, f, g;
  float     denominator;
  float     radicand;
  float     sqrtB;
  float     phaseArgument;
  float     tolerance;
  bool      success = false;

  matrix_init(params);
  matrix_init(&vandermonde);
  matrix_init(&xSquared);
  matrix_init(&solution);

  if (x->columns != 1 || y->columns != 1 || x->rows != y->rows || x->rows < 5)
  {
    goto cleanup;
  }
  if (matrix_copy(x, &xSquared) == false || matrix_sqr(&xSquared) == false)
  {
    goto cleanup;
  }
  {
    size_t count = matrix_element_count(&xSquared);
    for (size_t index = 0; index < count; index++)
    {
      xSquared.values[index] = -xSquared.values[index];
    }
  }
  if (matrix_vandermonde(x, y, &vandermonde) == false ||
      matrix_least_squares(&vandermonde, &xSquared, &solution) == false)
  {
    goto cleanup;
  }

  b = *matrix_const_value(&solution, 0, 0);
  c = *matrix_const_value(&solution, 1, 0);
  d = *matrix_const_value(&solution, 2, 0);
  f = *matrix_const_value(&solution, 3, 0);
  g = *matrix_const_value(&solution, 4, 0);

  // 타원 조건 검사 (상대허용오차 방식, FLT_EPSILON 기준)
  denominator = 4.0f * b - c * c;
  tolerance   = FLT_EPSILON * fmaxf(1.0f, fabsf(4.0f * b) + fabsf(c * c)) * 100.0f;
  if (fabsf(denominator) <= tolerance || b <= 0.0f)
  {
    // 타원 조건(B>0, 4B-C^2 != 0)을 만족하지 않음 - 입력 신호/배선 확인 필요
    goto cleanup;
  }

  radicand  = b * d * d - c * d * f + f * f - g * denominator;
  tolerance = FLT_EPSILON * fmaxf(1.0f, fabsf(b * d * d) + fabsf(c * d * f) +
                                          fabsf(f * f) + fabsf(g * denominator)) * 100.0f;
  if (radicand < -tolerance)
  {
    // 반지름의 제곱이 음수 - 캘리브레이션에 쓸 수 없는 데이터
    goto cleanup;
  }
  if (radicand < 0.0f)
  {
    radicand = 0.0f;
  }

  sqrtB         = sqrtf(b);
  phaseArgument = -c / (2.0f * sqrtB);
  if (phaseArgument < -1.0f - tolerance || phaseArgument > 1.0f + tolerance)
  {
    // asin에 넣을 수 없는 값 - 캘리브레이션에 쓸 수 없는 데이터
    goto cleanup;
  }
  phaseArgument = fmaxf(-1.0f, fminf(1.0f, phaseArgument));

  if (matrix_create(params, 5, 1) == false)
  {
    goto cleanup;
  }
  *matrix_value(params, 0, 0) = (c * f - 2.0f * b * d) / denominator;
  *matrix_value(params, 1, 0) = 2.0f * sqrtB / denominator * sqrtf(radicand);
  *matrix_value(params, 2, 0) = (c * d - 2.0f * f) / denominator;
  *matrix_value(params, 3, 0) = 2.0f / denominator * sqrtf(radicand);
  *matrix_value(params, 4, 0) = asinf(phaseArgument);
  success = true;

cleanup:
  matrix_destroy(&vandermonde);
  matrix_destroy(&xSquared);
  matrix_destroy(&solution);
  if (success == false)
  {
    matrix_destroy(params);
  }
  return success;
}

bool matrix_extract_phase(float a0, float a1, float b0, float b1, float eps,
                         float x, float y, float *out_phase)
{
  float cosine;
  float xNormalized;
  float yNormalized;
  float yCorrected;

  if (out_phase == NULL)
  {
    return false;
  }
  cosine = cosf(eps);
  if (fabsf(a1) <= FLT_EPSILON || fabsf(b1) <= FLT_EPSILON || fabsf(cosine) <= FLT_EPSILON)
  {
    return false;
  }

  xNormalized = (x - a0) / a1;
  yNormalized = (y - b0) / b1;
  yCorrected  = (yNormalized - xNormalized * sinf(eps)) / cosine;

  *out_phase = atan2f(yCorrected, xNormalized);
  return true;
}

float matrix_meter_current(float phase, float phaseOffset, float scale)
{
  return (phase - phaseOffset) * scale;
}
