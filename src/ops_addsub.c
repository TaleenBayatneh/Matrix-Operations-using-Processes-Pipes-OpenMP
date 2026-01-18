#include "common.h"
#include "ops.h"
#include "timer.h"

// Allocate a new matrix with the same dimensions as A and a given name
static Matrix *alloc_like(const Matrix *A, const char *name) {
    Matrix *C = matrix_create(name, A->rows, A->cols);  // Create a new matrix C with same rows and cols as A
    return C; 
}

//single process with openmp if enabled

// Function: ADD two matrices using single processes (or openmp if enabled)
Matrix *op_add_single(const Matrix *A, const Matrix *B, const char *name) {
    // Check if the matrices have the same dimensions
    if (A->rows != B->rows || A->cols != B->cols) {
        fprintf(stderr, "The Two matrix have not the same dimensions\n"); 
        return NULL;  // Return NULL if dimensions mismatch
    }
    
    Matrix *C = alloc_like(A, name);  // Allocate a result matrix C with same dimensions as A
    int N = A->rows * A->cols;  // Total number of elements
    
    // Parallelize the addition if OpenMP is enabled and size > 256
    #pragma omp parallel for if(g_omp_enabled && N > 256)
    //start the loop to do the add operation
    for (int i = 0; i < N; ++i)
        C->data[i] = A->data[i] + B->data[i];  // Add corresponding elements from A and B and the save the result in C
    
    return C;  // Return the resulting matrix
}

// Function: subtract two matrices using single processes (or openmp if enabled)
Matrix *op_sub_single(const Matrix *A, const Matrix *B, const char *name) {
    // Check if the matrices have the same dimensions
    if (A->rows != B->rows || A->cols != B->cols) {
        fprintf(stderr, "The Two matrix have not the same dimensions\n"); 
        return NULL;  // Return NULL if dimensions mismatch
    }
    
    Matrix *C = alloc_like(A, name);  // Allocate a result matrix C with same dimensions as A
    int N = A->rows * A->cols;  // Total number of elements
    
    // Parallelize the subtraction if OpenMP is enabled and size > 256
    #pragma omp parallel for if(g_omp_enabled && N > 256)

    //start the loop to do the subtract operation 
    for (int i = 0; i < N; ++i)
        C->data[i] = A->data[i] - B->data[i];  // Subtract corresponding elements from A and B and save the result in C
    
    return C;  // Return the resulting matrix
}


// add and subtract with multiprocessing (IPC Pool) 
//These functions perform matrix operations using multiple worker processes 

// Function: ADD two matrices using multiple worker processes
Matrix *op_add_processes(Pool *p, const Matrix *A, const Matrix *B, const char *name) {
    // Check if the two matrix have the same dimensions
    if (A->rows != B->rows || A->cols != B->cols) {
        fprintf(stderr, "The Two matrix have not the same dimensions\n");
        return NULL;
    }

    // Allocate result matrix C with same dimensions as A
    Matrix *C = alloc_like(A, name);

    // Initialize variables
    int R = A->rows;     // Number of rows
    int Cc = A->cols;    // Number of columns
    int total = R * Cc;  // Total number of elements
    int next = 0;        // Index of next element to send to a worker(processe)
    int done = 0;        // Number of elements completed
    int job_id = 1;      // Unique id for each worker (processe)

    // Dispatch jobs to workers until all elements are assigned
    while (next < total) {
        int wi = pool_find_idle(p); // Find an available worker
        if (wi < 0) 
            break;          // if there is no worker No available, stop dispatching

        // Compute the coordinates of the element 
        int i = next / Cc;          // Row index
        int j = next % Cc;          // Column index

        // store the two elements in payload ==> element from first mattrix and element from second matrix
        double payload[2] = { matrix_get(A,i,j), matrix_get(B,i,j) };

        // Create JobHeader structure with all necessary information for worker
        JobHeader h = { 
            .cmd = CMD_ADD_ELEM,             // choose the operation: add element
            .job_id = job_id++,              // Unique job ID
            .i = i, .j = j,                  // Element coordinates
            .rows = R, .cols = Cc, .n = 1,  // Matrix dimensions & number of elements
            .payload_bytes = (int)sizeof(payload) // Size of data
        };

        // Send the job and data (payload) to the worker process
        pool_send(wi, p, &h, payload);

        next++; // Move to the next element to dispatch
    }

    // Collect results from workers
    while (done < total) {
        int wi;                // Worker index that finished a job
        pool_wait_any(p, &wi); // Wait for any worker to finish

        double val;            // Variable to store the value
        ResultHeader rh;       // Result header have element coordinates

        // Receive result from worker via IPC
        if (pool_recv(wi, p, &rh, &val, sizeof(val)) < 0) 
            break; // if it fail , break from loop

        // Set received value in the result matrix C
        matrix_set(C, rh.i, rh.j, val);

        done++; // Increment completed counter

        // Dispatch a new job to this worker if elements remain
        if (next < total) {
            int i = next / Cc;          // Compute row index
            int j = next % Cc;          // Compute column index
            double payload[2] = { matrix_get(A,i,j), matrix_get(B,i,j) }; // get the data
           
            // set the header for the worker process
            JobHeader h = { 
                .cmd = CMD_ADD_ELEM, 
                .job_id = job_id++, 
                .i = i, .j = j,
                .rows = R, .cols = Cc, .n = 1,
                .payload_bytes = (int)sizeof(payload)
            };

            pool_send(wi, p, &h, payload); // Send new job
            next++; // Move to next element
        }
    }

    return C; // return the final result matrix
}

// Function: Subtract two matrices using multiple worker processes
Matrix *op_sub_processes(Pool *p, const Matrix *A, const Matrix *B, const char *name) {
    // Check if the two matrix have the same dimensions
    if (A->rows != B->rows || A->cols != B->cols) {
        fprintf(stderr, "The Two matrix have not the same dimensions \n");
        return NULL;
    }

    // Allocate matrix for the final result 
    Matrix *C = alloc_like(A, name);

    // Initialize variables
    int R = A->rows;     // Number of rows
    int Cc = A->cols;    // Number of columns
    int total = R * Cc;  // Total number of elements
    int next = 0;        // Index of next element to send to a worker(processe)
    int done = 0;        // Number of elements completed
    int job_id = 1;      // Unique id for each worker (processe)

    // Dispatch subtraction jobs to workers
    while (next < total) {
        int wi = pool_find_idle(p); // Find a worker
        if (wi < 0) break;          // No available worker, stop dispatching

        int i = next / Cc;          // Row index
        int j = next % Cc;          // Column index
        double payload[2] = { matrix_get(A,i,j), matrix_get(B,i,j) }; // get the data from the first and second matrices

        JobHeader h = { 
            .cmd = CMD_SUB_ELEM,     // choose the operation: subtract element
            .job_id = job_id++,      // Job ID
            .i = i, .j = j,          // Coordinates
            .rows = R, .cols = Cc, .n = 1,
            .payload_bytes = (int)sizeof(payload)
        };

        pool_send(wi, p, &h, payload); // Send job to worker
        next++; // Move to next element
    }

    // Collect subtraction results from workers
    while (done < total) {
        int wi;
        pool_wait_any(p, &wi); // Wait for any worker to finish

        double val;       // store the sub value from worker
        ResultHeader rh;  // Result header containing coordinates of the value 

        // Receive result
        if (pool_recv(wi, p, &rh, &val, sizeof(val)) < 0) 
            break; // if fail so break 

        // Store the value to the result matrix
        matrix_set(C, rh.i, rh.j, val); 
        done++;

        // Send next job if more elements remain
        if (next < total) {

            // compute the coordinates
            int i = next / Cc; 
            int j = next % Cc;

            double payload[2] = { matrix_get(A,i,j), matrix_get(B,i,j) };

            JobHeader h = { 
                .cmd = CMD_SUB_ELEM, 
                .job_id = job_id++, 
                .i = i, .j = j,
                .rows = R, .cols = Cc, .n = 1,
                .payload_bytes = (int)sizeof(payload)
            };

            pool_send(wi, p, &h, payload);
            next++;
        }
    }

    return C; // return final subtraction result matrix
}