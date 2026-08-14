#include "operations.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

bool matrix_sqr(Matrix *matrix) {
    size_t count = matrix_element_count(matrix);

    for (size_t index = 0; index < count; index++) {
        matrix->values[index] *= matrix->values[index];
    }
    return true;
}

bool matrix_vandermonde(const Matrix *x, const Matrix *y, Matrix *v) {
    matrix_init(v);
    if (x->columns != 1 || y->columns != 1 || x->rows != y->rows || x->rows == 0) {
        fprintf(stderr, "x and y must be non-empty N x 1 column vectors of the same size.\n");
        return false;
    }
    if (!matrix_create(v, x->rows, 5)) {
        fprintf(stderr, "Could not allocate the Vandermonde matrix.\n");
        return false;
    }
    for (size_t row = 0; row < x->rows; row++) {
        double x_value = *matrix_const_value(x, row, 0);
        double y_value = *matrix_const_value(y, row, 0);

        *matrix_value(v, row, 0) = y_value * y_value;
        *matrix_value(v, row, 1) = x_value * y_value;
        *matrix_value(v, row, 2) = x_value;
        *matrix_value(v, row, 3) = y_value;
        *matrix_value(v, row, 4) = 1.0;
    }
    return true;
}

bool matrix_add(const Matrix *left, const Matrix *right, Matrix *result) {
    size_t index;

    matrix_init(result);
    if (left->rows != right->rows || left->columns != right->columns) {
        fprintf(stderr, "Matrix dimensions must match for addition.\n");
        return false;
    }
    if (!matrix_create(result, left->rows, left->columns)) {
        fprintf(stderr, "Could not allocate the result matrix.\n");
        return false;
    }
    for (index = 0; index < matrix_element_count(result); index++) {
        result->values[index] = left->values[index] + right->values[index];
    }
    return true;
}

static size_t vector_length(const Matrix *matrix) {
    if (matrix->rows == 1) {
        return matrix->columns;
    }
    if (matrix->columns == 1) {
        return matrix->rows;
    }
    return 0;
}

static double vector_value(const Matrix *vector, size_t index) {
    if (vector->rows == 1) {
        return *matrix_const_value(vector, 0, index);
    }
    return *matrix_const_value(vector, index, 0);
}

bool matrix_least_squares(const Matrix *a, const Matrix *b, Matrix *x) {
    // Solve the least-squares problem A x = b using the normal equations A^T A x = A^T b.
    size_t variables = a->columns;
    size_t augmented_columns;
    size_t row;
    size_t column;
    size_t pivot;
    size_t length = vector_length(b);
    Matrix normal;
    double scale = 0.0;

    matrix_init(x);
    matrix_init(&normal);
    if (length == 0 || length != a->rows) {
        fprintf(stderr, "b must be a row or column vector with one value per row of A.\n");
        return false;
    }
    if (variables == SIZE_MAX) {
        fprintf(stderr, "A has too many columns.\n");
        return false;
    }
    augmented_columns = variables + 1;
    if (!matrix_create(&normal, variables, augmented_columns)) {
        fprintf(stderr, "Could not allocate the least-squares system.\n");
        return false;
    }
    for (row = 0; row < a->rows; row++) {
        for (column = 0; column < variables; column++) {
            double coefficient = *matrix_const_value(a, row, column);
            if (fabs(coefficient) > scale) {
                scale = fabs(coefficient);
            }
            for (pivot = 0; pivot < variables; pivot++) {
                *matrix_value(&normal, column, pivot) +=
                    coefficient * *matrix_const_value(a, row, pivot);
            }
            *matrix_value(&normal, column, variables) += coefficient * vector_value(b, row);
        }
    }
    if (scale == 0.0) {
        fprintf(stderr, "A has no nonzero coefficients.\n");
        matrix_destroy(&normal);
        return false;
    }
    for (column = 0; column < variables; column++) {
        size_t best = column;
        double best_value = fabs(*matrix_const_value(&normal, column, column));

        for (row = column + 1; row < variables; row++) {
            double candidate = fabs(*matrix_const_value(&normal, row, column));
            if (candidate > best_value) {
                best = row;
                best_value = candidate;
            }
        }
        if (best_value <= DBL_EPSILON * scale * scale * 100.0) {
            fprintf(stderr, "A^T A is singular; a unique solution does not exist.\n");
            matrix_destroy(&normal);
            return false;
        }
        if (best != column) {
            for (pivot = column; pivot <= variables; pivot++) {
                double temporary = *matrix_value(&normal, column, pivot);
                *matrix_value(&normal, column, pivot) = *matrix_value(&normal, best, pivot);
                *matrix_value(&normal, best, pivot) = temporary;
            }
        }
        for (row = column + 1; row < variables; row++) {
            double factor = *matrix_const_value(&normal, row, column) /
                            *matrix_const_value(&normal, column, column);
            for (pivot = column; pivot <= variables; pivot++) {
                *matrix_value(&normal, row, pivot) -=
                    factor * *matrix_const_value(&normal, column, pivot);
            }
        }
    }
    if (!matrix_create(x, variables, 1)) {
        fprintf(stderr, "Could not allocate the solution vector.\n");
        matrix_destroy(&normal);
        return false;
    }
    for (row = variables; row-- > 0;) {
        double right = *matrix_const_value(&normal, row, variables);
        for (column = row + 1; column < variables; column++) {
            right -= *matrix_const_value(&normal, row, column) *
                     *matrix_const_value(x, column, 0);
        }
        *matrix_value(x, row, 0) = right / *matrix_const_value(&normal, row, row);
    }
    matrix_destroy(&normal);
    return true;
}

bool matrix_lsm(const Matrix *a, const Matrix *b, Matrix *x) {
    return matrix_least_squares(a, b, x);
}

bool matrix_calibration(const Matrix *x, const Matrix *y, Matrix *params) {
    Matrix vandermonde;
    Matrix x_squared;
    Matrix solution;
    double b;
    double c;
    double d;
    double f;
    double g;
    double denominator;
    double radicand;
    double sqrt_b;
    double phase_argument;
    double tolerance;
    bool success = false;

    matrix_init(params);
    matrix_init(&vandermonde);
    matrix_init(&x_squared);
    matrix_init(&solution);
    if (x->columns != 1 || y->columns != 1 || x->rows != y->rows || x->rows < 5) {
        fprintf(stderr, "x and y must be N x 1 column vectors with N >= 5.\n");
        goto cleanup;
    }
    if (!matrix_copy(x, &x_squared) || !matrix_sqr(&x_squared)) {
        goto cleanup;
    }
    for (size_t index = 0; index < matrix_element_count(&x_squared); index++) {
        x_squared.values[index] = -x_squared.values[index];
    }
    if (!matrix_vandermonde(x, y, &vandermonde) ||
        !matrix_lsm(&vandermonde, &x_squared, &solution)) {
        goto cleanup;
    }

    b = *matrix_const_value(&solution, 0, 0);
    c = *matrix_const_value(&solution, 1, 0);
    d = *matrix_const_value(&solution, 2, 0);
    f = *matrix_const_value(&solution, 3, 0);
    g = *matrix_const_value(&solution, 4, 0);
    denominator = 4.0 * b - c * c;
    tolerance = DBL_EPSILON * fmax(1.0, fabs(4.0 * b) + fabs(c * c)) * 100.0;
    if (fabs(denominator) <= tolerance || b <= 0.0) {
        fprintf(stderr, "Calibration parameters are degenerate.\n");
        goto cleanup;
    }

    radicand = b * d * d - c * d * f + f * f - g * denominator;
    tolerance = DBL_EPSILON * fmax(1.0, fabs(b * d * d) + fabs(c * d * f) +
                                           fabs(f * f) + fabs(g * denominator)) * 100.0;
    if (radicand < -tolerance) {
        fprintf(stderr, "Calibration parameters produce an invalid square root.\n");
        goto cleanup;
    }
    if (radicand < 0.0) {
        radicand = 0.0;
    }
    sqrt_b = sqrt(b);
    phase_argument = -c / (2.0 * sqrt_b);
    if (phase_argument < -1.0 - tolerance || phase_argument > 1.0 + tolerance) {
        fprintf(stderr, "Calibration parameters produce an invalid phase.\n");
        goto cleanup;
    }
    phase_argument = fmax(-1.0, fmin(1.0, phase_argument));
    if (!matrix_create(params, 5, 1)) {
        fprintf(stderr, "Could not allocate the calibration output.\n");
        goto cleanup;
    }
    *matrix_value(params, 0, 0) = (c * f - 2.0 * b * d) / denominator;
    *matrix_value(params, 1, 0) = 2.0 * sqrt_b / denominator * sqrt(radicand);
    *matrix_value(params, 2, 0) = (c * d - 2.0 * f) / denominator;
    *matrix_value(params, 3, 0) = 2.0 / denominator * sqrt(radicand);
    *matrix_value(params, 4, 0) = asin(phase_argument);
    success = true;

cleanup:
    matrix_destroy(&vandermonde);
    matrix_destroy(&x_squared);
    matrix_destroy(&solution);
    if (!success) {
        matrix_destroy(params);
    }
    return success;
}

bool matrix_extract_phase(const Matrix *x, const Matrix *y, const Matrix *params, Matrix *phase) {
    double a0;
    double a1;
    double b0;
    double b1;
    double phi0;
    double cosine;

    matrix_init(phase);
    if (x->columns != 1 || y->columns != 1 || x->rows != y->rows || x->rows == 0 ||
        params->rows != 5 || params->columns != 1) {
        fprintf(stderr, "x and y must be matching N x 1 vectors and params must be 5 x 1.\n");
        return false;
    }
    a0 = *matrix_const_value(params, 0, 0);
    a1 = *matrix_const_value(params, 1, 0);
    b0 = *matrix_const_value(params, 2, 0);
    b1 = *matrix_const_value(params, 3, 0);
    phi0 = *matrix_const_value(params, 4, 0);
    cosine = cos(phi0);
    if (fabs(a1) <= DBL_EPSILON || fabs(b1) <= DBL_EPSILON ||
        fabs(cosine) <= DBL_EPSILON) {
        fprintf(stderr, "Calibration parameters cannot be used for phase extraction.\n");
        return false;
    }
    if (!matrix_create(phase, x->rows, 1)) {
        fprintf(stderr, "Could not allocate the phase vector.\n");
        return false;
    }
    for (size_t row = 0; row < x->rows; row++) {
        double x_normalized = (*matrix_const_value(x, row, 0) - a0) / a1;
        double y_normalized = (*matrix_const_value(y, row, 0) - b0) / b1;
        double y_corrected = (y_normalized - x_normalized * sin(phi0)) / cosine;

        *matrix_value(phase, row, 0) = atan2(y_corrected, x_normalized);
    }
    return true;
}

bool matrix_meter_current(const Matrix *phase, double scale, Matrix *current) {
    double phase_offset = 0.0;

    matrix_init(current);
    if (phase->columns != 1 || phase->rows == 0 || !isfinite(scale)) {
        fprintf(stderr, "phase must be a non-empty N x 1 vector and scale must be finite.\n");
        return false;
    }
    for (size_t row = 0; row < phase->rows; row++) {
        phase_offset += *matrix_const_value(phase, row, 0);
    }
    phase_offset /= (double)phase->rows;
    printf("phase_offset = %.17g\n", phase_offset);
    if (!matrix_create(current, phase->rows, 1)) {
        fprintf(stderr, "Could not allocate the current vector.\n");
        return false;
    }
    for (size_t row = 0; row < phase->rows; row++) {
        *matrix_value(current, row, 0) =
            (*matrix_const_value(phase, row, 0) - phase_offset) * scale;
    }
    return true;
}
