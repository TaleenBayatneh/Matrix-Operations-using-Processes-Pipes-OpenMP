#ifndef FILE_IO_H
#define FILE_IO_H
#include "matrix.h"
// functions in file_io.c , so when we include it in any , yhe file will have acess to call it
int read_matrix_file(const char *path, const char *name_override, Matrix **out); 
int write_matrix_file(const char *path, const Matrix *m);
int load_directory(const char *dir, MatrixRegistry *reg);
int save_all_to_dir(const char *dir, MatrixRegistry *reg);

#endif
