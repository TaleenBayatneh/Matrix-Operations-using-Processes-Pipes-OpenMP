#include "common.h"   
#include "ops.h"      
#include "timer.h"    

// Function: multiply two matrices using single processes (or openmp if enabled)
Matrix *op_mul_single(const Matrix *A, const Matrix *B, const char *name) {
    // Check if matrix dimensions are compatible for multiplication (A.cols must equal B.rows)
    if (A->cols != B->rows) {
        fprintf(stderr, "The dimensions are invalid \n"); 
        return NULL;  // Return NULL if dimensions are invalid
    }

    // Allocate result matrix C with dimensions (A.rows x B.cols)
    Matrix *C = matrix_create(name, A->rows, B->cols);

    
    // OpenMP: parallelize loops if enabled and matrix not too small, collapse(2) merges nested loops for better load balancing
    #pragma omp parallel for if(g_omp_enabled && (A->rows * B->cols > 8)) collapse(2)
    for (int i = 0; i < A->rows; i++) { // rows if the first matrix
        for (int j = 0; j < B->cols; j++) { // cols of the second matrix 
            double s = 0.0;  // store the dot product of row i of A and column j of B
            for (int k = 0; k < A->cols; k++)
                s += matrix_get(A, i, k) * matrix_get(B, k, j);  // Multiply corresponding elements and sum
            matrix_set(C, i, j, s);  // Store computed value in result matrix
        }
    }

    return C;  // Return the result matrix
}

// Matrix multiplication using a pool of processes (multiprocessing)
Matrix *op_mul_processes(Pool *p, const Matrix *A, const Matrix *B, const char *name) {
    // Check matrix dimension compatibility
    if (A->cols != B->rows) {
        fprintf(stderr,"The dimensions are invalid\n"); 
        return NULL;
    }

    // Allocate result matrix C
    Matrix *C = matrix_create(name, A->rows, B->cols);

    int R = A->rows;   // Number of rows in A
    int K = A->cols;   // Number of columns in A (also rows in B)
    int Cc = B->cols;  // Number of columns in B

    // Allocate temporary array to store a column of B for fast access
    double *Bcol = (double*)xmalloc((size_t)K * sizeof(double));

    int total = R * Cc;  // Total number of elements in result matrix
    int next = 0;         // Next element to assign as a job to the process
    int done = 0;         // Number of elements already computed by the processes
    int job_id = 1;       // Unique ID for each multiplication job

    //  assign tasks to processes
    while (next < total) {
        int wi = pool_find_idle(p);  // Find a worker
        if (wi < 0) 
            break;           // Exit if no available workers

        // Compute row and column indices for the current element
        int i = next / Cc;
        int j = next % Cc;

        // Allocate the data for this job: row from A + column from B
        double *payload = (double*)xmalloc((size_t)K * 2 * sizeof(double));

        // Copy row i of A into payload
        memcpy(payload, &A->data[(size_t)i * A->cols], (size_t)K * sizeof(double));

        // Extract column j of B and copy into payload
        for (int r = 0; r < K; r++)
            Bcol[r] = matrix_get(B, r, j);
        memcpy(payload + K, Bcol, (size_t)K * sizeof(double));

        // Create job header with information for the worker
            JobHeader h = { 
                .cmd = CMD_MUL_CELL,        // Choose the operation type: multiplication of a single cell
                .job_id = job_id++,          // Unique identifier for this job
                .i = i,                      // Row index of the element to compute
                .j = j,                      // Column index of the element to compute
                .rows = R,                   // Total number of rows in the result matrix
                .cols = Cc,                  // Total number of columns in the result matrix
                .n = K,                      // Number of elements in the row/column (length of dot product)
                .payload_bytes = (int)(2 * K * sizeof(double)) // Size of data being sent (row + column)
            };

        // Send job and data to worker process
        pool_send(wi, p, &h, payload);

        free(payload);  // Free temporary payload memory
        next++;         // Move to next element
    }

    double val;  // To store received result

    // Receive results from workers and continue sending new jobs
    while (done < total) {
        int wi;              // Worker index
        pool_wait_any(p, &wi);  // Wait for any worker to finish
        ResultHeader rh;         // Result header

        // Receive result from worker
        if (pool_recv(wi, p, &rh, &val, sizeof(val)) < 0)
             break;//break loop if failed

        // Store computed value in result matrix
        matrix_set(C, rh.i, rh.j, val);
        done++;  // Increment completed elements

        // If there are remaining jobs, send the next one to this worker
        if (next < total) {
            int i = next / Cc;
            int j = next % Cc;

            double *payload = (double*)xmalloc((size_t)K * 2 * sizeof(double));
            memcpy(payload, &A->data[(size_t)i * A->cols], (size_t)K * sizeof(double));
            for (int r = 0; r < K; r++)
                Bcol[r] = matrix_get(B, r, j);
            memcpy(payload + K, Bcol, (size_t)K * sizeof(double));

            // Create job header with information for the worker
            JobHeader h = { 
                .cmd = CMD_MUL_CELL,        // Choose the operation type: multiplication of a single cell
                .job_id = job_id++,          // Unique identifier for this job
                .i = i,                      // Row index of the element to compute
                .j = j,                      // Column index of the element to compute
                .rows = R,                   // Total number of rows in the result matrix
                .cols = Cc,                  // Total number of columns in the result matrix
                .n = K,                      // Number of elements in the row/column (length of dot product)
                .payload_bytes = (int)(2 * K * sizeof(double)) // Size of data being sent (row + column)
            };

            pool_send(wi, p, &h, payload);  // Send new job to worker
            free(payload);
            next++;
        }
    }

    free(Bcol);  // Free temporary column array
    return C;    // Return the final result matrix
}

