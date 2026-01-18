#ifndef MATRIX_H
#define MATRIX_H

#include "common.h"
// struct of the matrix content
typedef struct {
    char name[MAX_NAME];
    int rows, cols;
    double *data; // row-major
} Matrix;
// struct of the matrix ino in regestry
typedef struct {
    Matrix **items;
    int count, cap;
} MatrixRegistry;
// helper function to make op on matrices
void registry_init(MatrixRegistry *r);
void registry_free(MatrixRegistry *r);
Matrix *registry_get(MatrixRegistry *r, const char *name);
int     registry_add(MatrixRegistry *r, Matrix *m);
int     registry_remove(MatrixRegistry *r, const char *name);

Matrix *matrix_create(const char *name, int rows, int cols);
void    matrix_free(Matrix *m);
static inline double matrix_get(const Matrix *m, int i, int j) {
    return m->data[(size_t)i * m->cols + j];
}
static inline void matrix_set(Matrix *m, int i, int j, double v) {
    m->data[(size_t)i * m->cols + j] = v;
}

#endif
