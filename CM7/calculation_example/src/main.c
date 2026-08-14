#include "csv.h"
#include "operations.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s add A.txt B.txt C.txt\n", program);
    fprintf(stderr, "  %s copy A.txt B.txt\n", program);
    fprintf(stderr, "  %s sqr A.txt B.txt\n", program);
    fprintf(stderr, "  %s vandermonde x.txt y.txt V.txt\n", program);
    fprintf(stderr, "  %s calibration x.txt y.txt params.txt\n", program);
    fprintf(stderr, "  %s extract_phase x.txt y.txt params.txt phase.txt\n", program);
    fprintf(stderr, "  %s meter_current phase.txt scale current.txt\n", program);
    fprintf(stderr, "  %s lsm A.txt b.txt x.txt\n", program);
}

static int parse_scale(const char *text, double *scale) {
    char *end;

    errno = 0;
    *scale = strtod(text, &end);
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    return end != text && *end == '\0' && errno != ERANGE && isfinite(*scale);
}

static int run_add(const char *left_path, const char *right_path, const char *output_path) {
    Matrix left;
    Matrix right;
    Matrix result;
    int success;

    matrix_init(&left);
    matrix_init(&right);
    matrix_init(&result);
    if (!matrix_read_csv(left_path, &left) || !matrix_read_csv(right_path, &right)) {
        matrix_destroy(&left);
        matrix_destroy(&right);
        return 1;
    }
    success = matrix_add(&left, &right, &result) && matrix_write_csv(output_path, &result);
    matrix_destroy(&left);
    matrix_destroy(&right);
    matrix_destroy(&result);
    return success ? 0 : 1;
}

static int run_least_squares(const char *a_path, const char *b_path, const char *output_path) {
    Matrix a;
    Matrix b;
    Matrix x;
    int success;

    matrix_init(&a);
    matrix_init(&b);
    matrix_init(&x);
    if (!matrix_read_csv(a_path, &a) || !matrix_read_csv(b_path, &b)) {
        matrix_destroy(&a);
        matrix_destroy(&b);
        return 1;
    }
    success = matrix_least_squares(&a, &b, &x) && matrix_write_csv(output_path, &x);
    matrix_destroy(&a);
    matrix_destroy(&b);
    matrix_destroy(&x);
    return success ? 0 : 1;
}

static int run_copy(const char *source_path, const char *destination_path) {
    Matrix source;
    Matrix destination;
    int success;

    matrix_init(&source);
    matrix_init(&destination);
    if (!matrix_read_csv(source_path, &source)) {
        matrix_destroy(&source);
        return 1;
    }
    success = matrix_copy(&source, &destination) &&
              matrix_write_csv(destination_path, &destination);
    matrix_destroy(&source);
    matrix_destroy(&destination);
    return success ? 0 : 1;
}

static int run_sqr(const char *input_path, const char *output_path) {
    Matrix matrix;
    int success;

    matrix_init(&matrix);
    if (!matrix_read_csv(input_path, &matrix)) {
        matrix_destroy(&matrix);
        return 1;
    }
    success = matrix_sqr(&matrix) && matrix_write_csv(output_path, &matrix);
    matrix_destroy(&matrix);
    return success ? 0 : 1;
}

static int run_vandermonde(const char *x_path, const char *y_path, const char *output_path) {
    Matrix x;
    Matrix y;
    Matrix vandermonde;
    int success;

    matrix_init(&x);
    matrix_init(&y);
    matrix_init(&vandermonde);
    if (!matrix_read_csv(x_path, &x) || !matrix_read_csv(y_path, &y)) {
        matrix_destroy(&x);
        matrix_destroy(&y);
        return 1;
    }
    success = matrix_vandermonde(&x, &y, &vandermonde) &&
              matrix_write_csv(output_path, &vandermonde);
    matrix_destroy(&x);
    matrix_destroy(&y);
    matrix_destroy(&vandermonde);
    return success ? 0 : 1;
}

static int run_calibration(const char *x_path, const char *y_path, const char *params_path) {
    Matrix x;
    Matrix y;
    Matrix calibration;
    int success;

    matrix_init(&x);
    matrix_init(&y);
    matrix_init(&calibration);
    if (!matrix_read_csv(x_path, &x) || !matrix_read_csv(y_path, &y)) {
        matrix_destroy(&x);
        matrix_destroy(&y);
        return 1;
    }
    success = matrix_calibration(&x, &y, &calibration) &&
              matrix_write_csv(params_path, &calibration);
    matrix_destroy(&x);
    matrix_destroy(&y);
    matrix_destroy(&calibration);
    return success ? 0 : 1;
}

static int run_extract_phase(const char *x_path, const char *y_path,
                             const char *params_path, const char *phase_path) {
    Matrix x;
    Matrix y;
    Matrix params;
    Matrix phase;
    int success;

    matrix_init(&x);
    matrix_init(&y);
    matrix_init(&params);
    matrix_init(&phase);
    if (!matrix_read_csv(x_path, &x) || !matrix_read_csv(y_path, &y) ||
        !matrix_read_csv(params_path, &params)) {
        matrix_destroy(&x);
        matrix_destroy(&y);
        matrix_destroy(&params);
        return 1;
    }
    success = matrix_extract_phase(&x, &y, &params, &phase) &&
              matrix_write_csv(phase_path, &phase);
    matrix_destroy(&x);
    matrix_destroy(&y);
    matrix_destroy(&params);
    matrix_destroy(&phase);
    return success ? 0 : 1;
}

static int run_meter_current(const char *phase_path, const char *scale_text,
                             const char *current_path) {
    Matrix phase;
    Matrix current;
    double scale;
    int success;

    matrix_init(&phase);
    matrix_init(&current);
    if (!parse_scale(scale_text, &scale)) {
        fprintf(stderr, "scale must be a finite number.\n");
        return 1;
    }
    if (!matrix_read_csv(phase_path, &phase)) {
        matrix_destroy(&phase);
        return 1;
    }
    success = matrix_meter_current(&phase, scale, &current) &&
              matrix_write_csv(current_path, &current);
    matrix_destroy(&phase);
    matrix_destroy(&current);
    return success ? 0 : 1;
}

int main(int argc, char **argv) {
    int expected_argc;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "copy") == 0 || strcmp(argv[1], "sqr") == 0) {
        expected_argc = 4;
    } else if (strcmp(argv[1], "extract_phase") == 0) {
        expected_argc = 6;
    } else {
        expected_argc = 5;
    }
    if (argc != expected_argc) {
        print_usage(argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "add") == 0) {
        return run_add(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "lsm") == 0) {
        return run_least_squares(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "copy") == 0) {
        return run_copy(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "sqr") == 0) {
        return run_sqr(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "vandermonde") == 0) {
        return run_vandermonde(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "calibration") == 0) {
        return run_calibration(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "extract_phase") == 0) {
        return run_extract_phase(argv[2], argv[3], argv[4], argv[5]);
    }
    if (strcmp(argv[1], "meter_current") == 0) {
        return run_meter_current(argv[2], argv[3], argv[4]);
    }
    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}
