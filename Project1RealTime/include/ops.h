#ifndef OPS_H
#define OPS_H

#include "matrix.h"   
#include "pool.h"   

//Single-process routines (or with openmp if enabled) 

// Matrix addition (single-process or with OpenMP)
Matrix *op_add_single(const Matrix *A, const Matrix *B, const char *outname);
// Matrix subtraction (single-process or with OpenMP)
Matrix *op_sub_single(const Matrix *A, const Matrix *B, const char *outname);
// Matrix multiplication (single-process or with OpenMP)
Matrix *op_mul_single(const Matrix *A, const Matrix *B, const char *outname);
// Determinant computation (single-process)
double  op_det_single(const Matrix *A);
// Power iteration to find dominant eigenvalue and eigenvector, returns number of iterations and outputs lambda and vector
int op_eigen_power(const Matrix *A, double tol, int maxit, double *lambda_out, double **vec_out);



//Multi-process routines, may use OpenMP inside workers

// Matrix addition using multiple processes
Matrix *op_add_processes(Pool *p, const Matrix *A, const Matrix *B, const char *tmp_outname);
// Matrix subtraction using multiple processes
Matrix *op_sub_processes(Pool *p, const Matrix *A, const Matrix *B, const char *tmp_outname);
// Matrix multiplication using multiple processes
Matrix *op_mul_processes(Pool *p, const Matrix *A, const Matrix *B, const char *tmp_outname);
// Determinant computation using multiple processes
double  op_det_processes(Pool *p, const Matrix *A);
// Power iteration to find dominant eigenvalue and eigenvector, returns number of iterations and outputs lambda and vector
int op_eigen_processes(Pool *p, const Matrix *A, double tol, int maxit, double *lambda_out, double **vec_out);

#endif
