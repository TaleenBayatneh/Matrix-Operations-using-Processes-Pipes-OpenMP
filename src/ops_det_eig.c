#include "common.h"
#include "ops.h"
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <string.h>

//if we have omp we use the global flag
#ifdef HAVE_OMP
#include <omp.h>
extern int g_omp_enabled;//global variable that tells if omp is enabled
#else
static int g_omp_enabled = 0;//if omp not available set it to 0
#endif

//return absolute value of a double number
static inline double dabs(double x){
    if (x < 0)
        return -x;
    else
        return x;
     }

//copy the matrix data into a 1D array like row by row 
static double *copy_matrix_dense(const Matrix *A) {
    int n = A->rows, m = A->cols;

    //allocate memory for the new array
    double *M = (double *)xmalloc((size_t)n * (size_t)m * sizeof(double));
    
    //fill the array with the matrix values that we pass to this function
    //basically flatten the 2d matrix into a single long 1d array
    for (int i=0;i<n;i++)
        for (int j=0;j<m;j++)
            M[(size_t)i*(size_t)m + (size_t)j] = matrix_get(A,i,j);
    return M;
}

//decide how many workers -threads/processes- to use
static int choose_workers(int nrows) {
    //get number of CPU cores that available
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    //use system cores if available else use 4 -default-
    int w;
    if (cores > 0) {
        w = (int)cores;
    } else {
        w = 4;
    }
    
    //this is to make sure that we dont assign more workers than rows
    if (w > nrows) w = nrows;
    //to make sure that at least one worker
    if (w < 1) w = 1;

    return w;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// SINGLE-PROCESS (determinant) using gaussian elimination -lu method-
double op_det_single(const Matrix *A) {
    //check if the matrix is valid and if square 
    if (!A || A->rows != A->cols) return NAN;

    int n = A->rows; 
    double *M = copy_matrix_dense(A);//make a copy of the matrix in 1d array 
    int sign = 1;//since swaps change sign of determinant this will track if we swap rows


    //this is the main loop for the gaussian elimination
    for (int k=0; k<n; ++k) {

        int piv = k;//pivot row it start with the current row
        //get the absolute value of pivot element
        double maxv = dabs(M[(size_t)k*(size_t)n + (size_t)k]);

        //this loop to find the largest pivot in current column
        for (int i=k+1;i<n;i++) {
            double v = dabs(M[(size_t)i*(size_t)n + (size_t)k]);
            //found a bigger pivot
            if (v > maxv) {
                 maxv = v; 
                 piv = i; 
                }
        }

        //if pivot is small(which make floating point errors) so the det is 0
        if (maxv < 1e-12) {
             free(M); 
             return 0.0;
             }
        
        //if pivot row is different from current row so we swap them  
        if (piv != k) {
            for (int j=k;j<n;j++) {
                double tmp = M[(size_t)k*(size_t)n + (size_t)j];
                M[(size_t)k*(size_t)n + (size_t)j] = M[(size_t)piv*(size_t)n + (size_t)j];
                M[(size_t)piv*(size_t)n + (size_t)j] = tmp;
            }
            sign = -sign;//the swap changes the determinate sign
        }

        //if this is the last row we stop
        if (k == n-1) break;

        const double pivot = M[(size_t)k*(size_t)n + (size_t)k];//this is to get the value of pivot
        double *prow = &M[(size_t)k*(size_t)n + (size_t)(k+1)];//this is a pointer to next columns in pivot row


#ifdef HAVE_OMP
        //if omp ia available and enabled we run parallel processing
        if (g_omp_enabled) {
#pragma omp parallel for schedule(static)
            for (int i=k+1;i<n;i++) {
                double *row = &M[(size_t)i*(size_t)n + (size_t)k];
                double aik = row[0] / pivot;//ratio of pivot
                row[0] = 0.0;//set current column element to 0

                //update row elements after pivot
                for (int j=1;j<(n-k);j++) row[j] -= aik * prow[j-1];
            }
        } else
#endif
        //if omp not used we run sequentially
        {
            for (int i=k+1;i<n;i++) {
                double *row = &M[(size_t)i*(size_t)n + (size_t)k];
                double aik = row[0] / pivot;
                row[0] = 0.0;

                //update remaining row elements
                for (int j=1;j<(n-k);j++) row[j] -= aik * prow[j-1];
            }
        }
    }
    //multiply all diagonal elements to get determinant
    double det = (double)sign;
    for (int i=0;i<n;i++) det *= M[(size_t)i*(size_t)n + (size_t)i];
   
    free(M);//free memory
    return det;//return the determinant
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//SINGLE-PROCESS: (dominant eigen) power iteration
int op_eigen_power(const Matrix *A, double tol, int maxit,
                   double *lambda_out, double **vec_out) {
    //check if matrix exists, is square, and inputs are valid
    if (!A || A->rows != A->cols || tol <= 0.0 || maxit <= 0) return -1;
    int n = A->rows;
    
    //allocate memory for 3 vectors
    double *x  = (double *)xmalloc((size_t)n*sizeof(double));//current vector
    double *y  = (double *)xmalloc((size_t)n*sizeof(double));//result vector A*x
    double *xn = (double *)xmalloc((size_t)n*sizeof(double));//normalized vector
    
    // start with x = [1, 1, 1, ...]
    for (int i=0;i<n;i++) x[i] = 1.0;

    int it;//iteration counter
    double lambda = 0.0;//dominant eigenvalue

    //repeat power iteration process
    for (it=1; it<=maxit; ++it) {
     //compute y= A*x 
#ifdef HAVE_OMP
        if (g_omp_enabled) {
#pragma omp parallel for schedule(static)
            for (int i=0;i<n;i++) {
                double s = 0.0;
                //multiply each row of A with vector x
                for (int j=0;j<n;j++) s += matrix_get(A,i,j)*x[j];
                y[i] = s;//save result
            }
        } else
#endif
        {
            //sequential version if omp is off
            for (int i=0;i<n;i++) {
                double s = 0.0;
                for (int j=0;j<n;j++) s += matrix_get(A,i,j)*x[j];
                y[i] = s;
            }
        }

        //find the length of vector y
        double norm2 = 0.0;
#ifdef HAVE_OMP
        if (g_omp_enabled) {
#pragma omp parallel for reduction(+:norm2) schedule(static)
            for (int i=0;i<n;i++) norm2 += y[i]*y[i];//sum of squares
        } else
#endif
        {
            for (int i=0;i<n;i++) norm2 += y[i]*y[i];
        }
        double norm = sqrt(norm2);//sqrt to get the actual norm length

        //if norm is almost 0 then it means that the matrix have issues
        if (norm < 1e-20) { 
            free(x); free(y); free(xn);
            return -1; }
        //normalize vector xn=y/norm
        for (int i=0;i<n;i++) xn[i] = y[i] / norm;


        //find lambda
        double rnum = 0.0;
#ifdef HAVE_OMP
        if (g_omp_enabled) {
#pragma omp parallel for reduction(+:rnum) schedule(static)
            for (int i=0;i<n;i++) rnum += xn[i]*y[i];
        } else
#endif
        {
            for (int i=0;i<n;i++) rnum += xn[i]*y[i];
        }

        lambda = rnum;

        //check diff between new and old vectors
        double diff = 0.0;
        for (int i=0;i<n;i++) {
            double d = dabs(xn[i] - x[i]);
            if (d > diff) diff = d;//here keeps the biggest change
        }

        //copy the new vector into x for the next iteration 
        memcpy(x, xn, (size_t)n*sizeof(double));
        //if the diff is very small stop
        if (diff < tol) break;
    }

    //save the results to the output values 
    *lambda_out = lambda;
    *vec_out = xn;
    
    free(x); free(y);
    return it;//return num of iterations 
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//MULTI-PROCESS: determinant row-chunk LU step 
double op_det_processes(Pool *p, const Matrix *A) {
    
    (void)p;//this is to avoid compiler warning pool not used here
    //vaild and square matrix
    if (!A || A->rows != A->cols) return NAN;

    int n = A->rows;
    double *M = copy_matrix_dense(A);//make a 1d copy of matrix
    int sign = 1;// keep track of sign changes from row swaps

    //go through each column to do elimination
    for (int k = 0; k < n; ++k) {
        int piv = k;//assume current row is pivot
        double maxv = dabs(M[(size_t)k*(size_t)n + (size_t)k]);//get current pivot abs value
        
        //find pivot row (the bigest value in the current column)
        for (int i=k+1;i<n;i++) {
            double v = dabs(M[(size_t)i*(size_t)n + (size_t)k]);
            if (v > maxv) { 
                maxv = v;
                piv = i; }
        }
        //if pivot is almost 0 return the determinat =0
        if (maxv < 1e-12) { free(M); return 0.0; }

        //swap row if pivot row is different
        if (piv != k) {
            for (int j=k;j<n;j++) {
                double tmp = M[(size_t)k*(size_t)n + (size_t)j];
                M[(size_t)k*(size_t)n + (size_t)j] = M[(size_t)piv*(size_t)n + (size_t)j];
                M[(size_t)piv*(size_t)n + (size_t)j] = tmp;
            }
            sign = -sign;//swapping flips determinant sign
        }
        //if this is the last row stop
        if (k == n-1) break;

        //remeaning rows after pivot
        int rows_rem = n - (k+1);
        //choose how many worker to use
        int W = choose_workers(rows_rem);

        //if small matrix or only one process(worker) we do normal elimination
        if (rows_rem <= 8 || W == 1) {
            const double pivot = M[(size_t)k*(size_t)n + (size_t)k];
            double *prow = &M[(size_t)k*(size_t)n + (size_t)(k+1)];
            for (int i=k+1;i<n;i++) {
                double aik = M[(size_t)i*(size_t)n + (size_t)k] / pivot;
                for (int j=k+1;j<n;j++) {
                    //eliminate below the pivot
                    M[(size_t)i*(size_t)n + (size_t)j] -= aik * prow[j-(k+1)];
                }
                M[(size_t)i*(size_t)n + (size_t)k] = 0.0;//set eliminated cell to 0
            }
            continue;
        }

        //divide the remaining rows into chunks for each worker process
        int chunk = (rows_rem + W - 1) / W;
        const double pivot = M[(size_t)k*(size_t)n + (size_t)k];
        double *prow = &M[(size_t)k*(size_t)n + (size_t)(k+1)];

        //arrays for communication pipes between parent and child
        int p2c[64][2], c2p[64][2];//parent to child and child to parent
        pid_t kids[64];//this is to store child process ids
        if (W > 64) W = 64;//this is a limit to workers (64 worker)

        int active = 0;//number of active child processes

        //create multiple worker processes
        for (int w=0; w<W; ++w) {
            int i0 = k+1 + w*chunk;//start row for this worker
            int i1 = i0 + chunk;//end row
            if (i0 >= n) break;
            if (i1 > n) i1 = n;

            //create pipes for data exchange
            if (pipe(p2c[w]) < 0 || pipe(c2p[w]) < 0) { perror("pipe"); free(M); return NAN; }
            
            //create a new process
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); free(M); return NAN; }


            //child process code
            if (pid == 0) {
                close(p2c[w][1]); close(c2p[w][0]);//close write and read end 

                //set number of threads for this process 
#ifdef HAVE_OMP
                if (g_omp_enabled) {
                    int cores = omp_get_num_procs();
                    int threads = cores / W; if (threads < 1) threads = 1;
                    omp_set_dynamic(0);
                    omp_set_num_threads(threads);
                } else {
                    omp_set_dynamic(0);
                    omp_set_num_threads(1);
                }
#endif
                
                int hdr[4];
                //read header: start/end rows, piovt index, matrix size
                if (read_all(p2c[w][0], hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) _exit(111);
                int H_i0 = hdr[0], H_i1 = hdr[1], H_k = hdr[2], H_n = hdr[3];

                double H_pivot;
                //read pivot value
                if (read_all(p2c[w][0], &H_pivot, sizeof(double)) != (ssize_t)sizeof(double)) _exit(112);

                //read pivot row segment
                int seg = H_n - H_k - 1;
                if (seg < 0) seg = 0;
                double *H_prow = (double *)xmalloc((size_t)seg*sizeof(double));
                if (seg > 0) {
                    if (read_all(p2c[w][0], H_prow, (size_t)seg*sizeof(double)) != (ssize_t)((size_t)seg*sizeof(double))) _exit(113);
                }

                //read assigned rows for this child
                size_t rowlen = (size_t)(H_n - H_k);
                size_t bufcount = (size_t)(H_i1 - H_i0) * rowlen;
                double *buf = (double *)xmalloc(bufcount * sizeof(double));
                if (read_all(p2c[w][0], buf, bufcount*sizeof(double)) != (ssize_t)(bufcount*sizeof(double))) _exit(114);

                //perform gussian elimination on this child part 
#pragma omp parallel for if(g_omp_enabled) schedule(static)
                for (int r=0;r<(H_i1 - H_i0);r++) {
                    double *row = &buf[(size_t)r*rowlen];
                    double aik = row[0] / H_pivot;//compute factor  
                    row[0] = 0.0;//eliminate first column element
                    //update rest ot the row
                    for (int j=1;j<=(int)seg;j++) row[j] -= aik * H_prow[j-1];
                }

                //send back the modified rows to parent
                if (write_all(c2p[w][1], buf, bufcount*sizeof(double)) != (ssize_t)(bufcount*sizeof(double))) _exit(115);
                free(buf); free(H_prow);
                _exit(0);//end child
            }

            //parent code
            kids[active++] = pid;//store child id

            close(p2c[w][0]); close(c2p[w][1]);//parent closes read and write end

            //send work header and pivot info to the child
            int hdr[4] = { i0, i1, k, n };
            if (write_all(p2c[w][1], hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) { perror("write hdr"); free(M); return NAN; }
            if (write_all(p2c[w][1], &pivot, sizeof(double)) != (ssize_t)sizeof(double)) { perror("write pivot"); free(M); return NAN; }

            //send pivot row segment
            int seg = n - k - 1;
            if (seg < 0) seg = 0;
            if (seg > 0) {
                if (write_all(p2c[w][1], prow, (size_t)seg*sizeof(double)) != (ssize_t)((size_t)seg*sizeof(double))) { perror("write prow"); free(M); return NAN; }
            }

            //send the rows chunk to the child 
            size_t rowlen = (size_t)(n - k);
            size_t bufcount = (size_t)(i1 - i0) * rowlen;
            double *buf = (double *)xmalloc(bufcount*sizeof(double));
            for (int ii=i0; ii<i1; ++ii) {
                memcpy(&buf[(size_t)(ii-i0)*rowlen], &M[(size_t)ii*(size_t)n + (size_t)k], rowlen*sizeof(double));
            }
            if (write_all(p2c[w][1], buf, bufcount*sizeof(double)) != (ssize_t)(bufcount*sizeof(double))) { perror("write rows"); free(buf); free(M); return NAN; }
            free(buf);
            close(p2c[w][1]);//close write after sending data
        }

        //parent reads results back from children
        for (int w=0; w<active; ++w) {
            int hdr[4]; (void)hdr; //already known 
            int i0 = k+1 + w*chunk;
            int i1 = i0 + chunk;
            if (i0 >= n) break;
            if (i1 > n) i1 = n;

            size_t rowlen = (size_t)(n - k);
            size_t bufcount = (size_t)(i1 - i0) * rowlen;
            double *buf = (double *)xmalloc(bufcount*sizeof(double));
            if (read_all(c2p[w][0], buf, bufcount*sizeof(double)) != (ssize_t)(bufcount*sizeof(double))) { perror("read rows"); free(buf); free(M); return NAN; }

            //copy the updated rows back into main matrix 
            for (int ii=i0; ii<i1; ++ii) {
                memcpy(&M[(size_t)ii*(size_t)n + (size_t)k], &buf[(size_t)(ii-i0)*rowlen], rowlen*sizeof(double));
            }
            free(buf);
            close(c2p[w][0]);
        }

        //wait for all child processes to finish 
        for (int w=0; w<active; ++w) {
            int status; (void)waitpid(kids[w], &status, 0);
        }
    }

    //compute determinant by multiplying diagonal elemants
    double det = (double)sign;
    for (int i=0;i<n;i++) det *= M[(size_t)i*(size_t)n + (size_t)i];
    
    free(M);
    return det;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//MULTI-PROCESS: dominant eigen (per-iteration row chunks)
int op_eigen_processes(Pool *p, const Matrix *A, double tol, int maxit,
                       double *lambda_out, double **vec_out) {
    
    (void)p;//this is to avoid compiler warning pool not used here
    //check if matrix exists, is square, and inputs are valid
    if (!A || A->rows != A->cols || tol <= 0.0 || maxit <= 0) return -1;

    int n = A->rows;
    //allocate memory for vectors
    double *x  = (double *)xmalloc((size_t)n*sizeof(double));//normalized vector
    double *y  = (double *)xmalloc((size_t)n*sizeof(double));//result vector A*x
    double *xn = (double *)xmalloc((size_t)n*sizeof(double));//normalized vector
    
    //start with x = [1, 1, 1, ...]
    for (int i=0;i<n;i++) x[i] = 1.0;

    //choose number of worker processes
    int W = choose_workers(n);
    if (W < 1) W = 1;
    if (W > 64) W = 64;

    int it;//iteration counter
    double lambda = 0.0;//eigenvalue estimate

    //main iteration loop
    for (it=1; it<=maxit; ++it) {
        int c2p[64][2];//child to parent pipes
        pid_t kids[64];//store child ids
        int chunk = (n + W - 1) / W;//how many rows each process handles
        int active = 0;

        //this is to create worker processes for matrix vector multiplication
        for (int w=0; w<W; ++w) {
            int i0 = w*chunk;
            int i1 = i0 + chunk;
            if (i0 >= n) break;
            if (i1 > n) i1 = n;

            int pipefd[2];
            if (pipe(pipefd) < 0) { perror("pipe"); free(x); free(y); free(xn); return -1; }
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); free(x); free(y); free(xn); return -1; }
            
            //child process
            if (pid == 0) {
                close(pipefd[0]);//child closes read end only 

#ifdef HAVE_OMP
                
                //set up threads if omp is on
                if (g_omp_enabled) {
                    int cores = omp_get_num_procs();
                    int threads = cores / W; if (threads < 1) threads = 1;
                    omp_set_dynamic(0);
                    omp_set_num_threads(threads);
                } else {
                    omp_set_dynamic(0);
                    omp_set_num_threads(1);
                }
#endif
                //perform part ot y =A*x
#pragma omp parallel for if(g_omp_enabled) schedule(static)
                for (int i=i0;i<i1;i++) {
                    double s = 0.0;
                    for (int j=0;j<n;j++) s += matrix_get(A,i,j)*x[j];//dot product of row i with x
                    y[i] = s; 
                }

                //sed this part ot y back to parent 
                if (write_all(pipefd[1], &y[i0], (size_t)(i1 - i0)*sizeof(double)) != (ssize_t)((size_t)(i1-i0)*sizeof(double))) _exit(121);
                close(pipefd[1]);
                _exit(0);
            }


            //parent process
            kids[active] = pid;
            c2p[active][0] = pipefd[0];//parent reads from here
            c2p[active][1] = pipefd[1];
            active++;
            close(pipefd[1]);//parent does not write 
        }

        //parent collects from all children the partial results
        for (int w=0; w<active; ++w) {
            int i0 = w*chunk;
            int i1 = i0 + chunk;
            if (i0 >= n) break;
            if (i1 > n) i1 = n;
            if (read_all(c2p[w][0], &y[i0], (size_t)(i1-i0)*sizeof(double)) != (ssize_t)((size_t)(i1-i0)*sizeof(double))) { perror("read y"); free(x); free(y); free(xn); return -1; }
            close(c2p[w][0]);//close after reading
        }
        //wait all child processes to finish
        for (int w=0; w<active; ++w) {
            int status; (void)waitpid(kids[w], &status, 0);
        }

        //calculate norm length of y
        double norm2 = 0.0;
        for (int i=0;i<n;i++) norm2 += y[i]*y[i];
        double norm = sqrt(norm2);
        //if norm is almost 0 then it means that the matrix have issues
        if (norm < 1e-20) { free(x); free(y); free(xn); return -1; }

        //normalize vector
        for (int i=0;i<n;i++) xn[i] = y[i] / norm;
       
        //estimate lambda using reyleigh quotient
        double rnum = 0.0;
        for (int i=0;i<n;i++) rnum += xn[i]*y[i];
        lambda = rnum;

        //check difference between x and xn
        double diff = 0.0;
        for (int i=0;i<n;i++) {
            double d = dabs(xn[i] - x[i]);
            if (d > diff) diff = d;
        }

        //update x for the next iteration
        memcpy(x, xn, (size_t)n*sizeof(double));
        //if difference is small enough we stop iterating
        if (diff < tol) break;
    }

    //store outputs
    *lambda_out = lambda;
    *vec_out = xn;
    
    free(x); free(y);
    return it;
}
