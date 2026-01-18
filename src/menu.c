#include "common.h"
#include "matrix.h"
#include "menu.h"
#include "file_io.h"
#include "ops.h"
#include "pool.h"
#include "timer.h"
#include <sys/stat.h>
 
// Global flag to indicate whether OpenMP is enabled
#ifdef HAVE_OMP
int g_omp_enabled = 1;
#else
int g_omp_enabled = 0;
#endif

static MatrixRegistry g_reg;  // Global registry for matrices
static Pool *g_pool = NULL;   // Global process pool pointer
static int g_next_id = 1;     // Global ID counter for assigning unique matrix IDs

// Returns a string for OpenMP state ("ON" or "OFF")
static const char* omp_state_str(void) {
    if (g_omp_enabled) {
        return "ON";
    } else {
        return "OFF";
    }
}

// Renames the matrix to the given integer ID (as string)
static void rename_to_id(Matrix *m, int id) {

    char nbuf[MAX_NAME];   // Temporary buffer for integer -> string
    snprintf(nbuf, sizeof(nbuf), "%d", id);       // Convert id to string
    snprintf(m->name, sizeof(m->name), "%s", nbuf);  // Copy string into matrix name
}

// Assigns a new unique ID to the matrix and renames it
static int assign_new_id(Matrix *m) {

    int id = g_next_id;   // Take the next available ID
    g_next_id++; // Increment the global counter
    rename_to_id(m, id);  // Rename the matrix to this ID
    return id; // Return the assigned ID
}

// Prints the matrix values without any header
static void print_matrix_raw(const Matrix *m) {

    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            printf("%g", matrix_get(m, i, j));  // Print each element
            if (j < m->cols - 1) {
                printf(" ");  // Print space between elements except last column
            }
        }
        printf("\n");  // Newline at the end of each row
    }
}

// Prints the matrix with its ID and dimensions as a header
static void print_matrix_with_header(const Matrix *m) {
    printf("ID : %s, dimension : %d*%d\n", m->name, m->rows, m->cols);  // Header
    print_matrix_raw(m); //print the actual matrix value
}

// Menu options related to matrices: enter, modify, delete, display
// Create a new matrix, dimensions entered by the user
static void enter_matrix() {

    int r, c;
    printf("Rows Cols: ");
    if (scanf("%d %d", &r, &c) != 2) {     
        fprintf(stderr, "Invalid input for dimensions.\n");
        return;
    }

    char name[MAX_NAME];
    
    // Convert the next available numeric ID to a string to use as a temporary
    // name for the new matrix. The registry stores matrix names as strings,
    // so we need to represent the numeric ID as a string for adding it later.
    // ensuring we don't exceed the buffer size.
    // Now 'name' contains something like "1", "2", "3", etc.
    snprintf(name, sizeof(name), "%d", g_next_id);  // Temporary name using next ID

    // Create matrix with given name, rows, and columns
    Matrix *m = matrix_create(name, r, c);

    printf("Enter %d values row-major:\n", r * c);

    // Read matrix values from user
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            double v;
        if (scanf("%lf", &v) != 1) {        
            fprintf(stderr, "Invalid numeric input.\n");
            return;
        }
            matrix_set(m, i, j, v);
        }
    }

    // Assign a unique ID and add the matrix to the registry
    int id = assign_new_id(m);
    registry_add(&g_reg, m);

    // Print matrix info and values
    printf("Created matrix (OMP=%s):\n", omp_state_str());
    print_matrix_with_header(m);
    printf("Saved with ID: %d\n", id);
}

// Display matrix info and content
static void display_matrix() {

    int id;
    if (scanf("%d", &id) != 1) {             
        fprintf(stderr, "Invalid ID.\n");
        return;
    }
    char key[MAX_NAME];
    snprintf(key, sizeof(key), "%d", id);
    Matrix *m = registry_get(&g_reg, key);

    if (m == NULL) {
        printf("Matrix not found\n");
        return;
    }
    print_matrix_with_header(m);
    printf("(OMP=%s)\n", omp_state_str());
}

// Delete matrix, after showing content for confirmation
static void delete_matrix() {

    int id;
    printf("ID to delete: ");
    if (scanf("%d", &id) != 1) {        
        fprintf(stderr, "Invalid ID.\n");
        return;
    }

    char key[MAX_NAME];
    snprintf(key, sizeof(key), "%d", id);
    Matrix *m = registry_get(&g_reg, key);

    if (m == NULL) {
        printf("Matrix not found\n");
        return;
    }

    // Show the matrix to the user
    printf("About to delete matrix:\n");
    print_matrix_with_header(m);

    // Confirm deletion
    printf("Are you sure? (y/n): ");
    char ans = 'n';
    if ( scanf(" %c", &ans)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    if (ans == 'y' || ans == 'Y') {
        registry_remove(&g_reg, key);
        printf("Deleted ID %d\n", id);
    } else {
        printf("Canceled.\n");
    }
}

// Edit matrix values
static void modify_matrix() {

    int id;
    printf("ID: ");
    if (scanf("%d", &id)!= 1) {        
            fprintf(stderr, "Invalid entry.\n");
            return;
        }
    char key[MAX_NAME];
    snprintf(key, sizeof(key), "%d", id);

    Matrix *m = registry_get(&g_reg, key);

    if (m == NULL) {
        printf("Matrix not found\n");
        return;
    }

    int choice;
    printf("1) set value  2) set full row  3) set full col : ");
    if (scanf("%d", &choice)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    if (choice == 1) { // if the change on a specific value, get i:rows index, j:col index "both start from 0 idex " v:new value

        int i, j;
        double v;
        printf("i j v: ");
        if (scanf("%d %d %lf", &i, &j, &v)!= 3) {        
                fprintf(stderr, "Invalid entry.\n");
                return;
            }

        if (i < 0 || i >= m->rows || j < 0 || j >= m->cols) {
            printf("Index out of range\n");
            return;
        }
        matrix_set(m, i, j, v);// function that take the matrix id and apply new data to the matrix 

    } else if (choice == 2) { // change a full row, get i:row index, v: set of new valuse as num as col 

        int i;
        printf("Row index: ");
        if ( scanf(" %d", &i)!= 1) {        
                fprintf(stderr, "Invalid entry.\n");
                return;
            }
        if (i < 0 || i >= m->rows) {
            printf("Index out of range\n");
            return;
        }

        for (int j = 0; j < m->cols; j++) {
            double v;
            if ( scanf(" %lf", &v)!= 1) {        
            fprintf(stderr, "Invalid entry.\n");
            return;
              }
            matrix_set(m, i, j, v);
        }

    } else if (choice == 3) {// change a full col, get j:col index, v: set of new valuse as num as row 

        int j;
        printf("Column index: ");
        if ( scanf(" %d", &j)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

        if (j < 0 || j >= m->cols) {
            printf("Index out of range\n");
            return;
        }

        for (int i = 0; i < m->rows; i++) {
            double v;
            if ( scanf(" %lf", &v)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }
            matrix_set(m, i, j, v);
        }

    } else {
        printf("Invalid choice\n");
        return;
    }
    // Show updated matrix
    printf("Updated matrix (ID=%d):\n", id);
    print_matrix_with_header(m);
}

// options in menu I/O FUNCTIONS

// Read a single matrix from a file path
static void read_matrix() {

    char path[256];
    printf("File path: ");
    // Read file path safely (max 255 chars)
      if ( scanf(" %255s", path)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    char name[MAX_NAME];
    snprintf(name, sizeof(name), "%d", g_next_id);

    Matrix *m = NULL;

    // Try to load the matrix from the file.
    // read_matrix_file fills 'm' on success.
    if (read_matrix_file(path, name, &m) == 0 && m != NULL) {

        int id = assign_new_id(m);     // Assign a unique ID to the matrix
        registry_add(&g_reg, m);       // Store matrix inside the registry

        printf("Loaded matrix (OMP=%s):\n", omp_state_str());
        print_matrix_with_header(m);
        printf("Saved with ID: %d\n", id);
    }
    else {
        printf("Failed to load\n");
    }
}

// Read all matrices from a folder
static void read_dir() {

    char dir[256];
    printf("Folder path: ");
      if ( scanf("%255s", dir)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }
    int prev = g_reg.count;  // Remember how many matrices existed before loading
    int rc = load_directory(dir, &g_reg);
    if (rc < 0) {
        printf("Failed to load directory\n");
        return;
    }
    // Process only newly-loaded matrices
    for (int i = prev; i < g_reg.count; ++i) {

        Matrix *m = g_reg.items[i];
        if (m == NULL)
            continue;
        int id = assign_new_id(m);   // Assign a proper ID
        printf("Loaded matrix from folder (OMP=%s):\n", omp_state_str());
        print_matrix_with_header(m);
        printf("Saved with ID: %d\n", id);
    }
}
// Save a single matrix to a user-chosen file path
static void save_matrix() {

    int id;
    char path[256];

    printf("Matrix ID: ");
      if ( scanf(" %d", &id)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }
    printf("Save path: ");
      if ( scanf(" %255s", path)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    char key[MAX_NAME];

    // Convert the numeric ID to a string to use it as the lookup key.
    // The registry uses string names, not integers.
    snprintf(key, sizeof(key), "%d", id);

    Matrix *m = registry_get(&g_reg, key);

    if (m == NULL) {
        printf("Not found\n");
        return;
    }

    // Save matrix to file
    if (write_matrix_file(path, m) < 0)
        printf("Failed to write\n");
    else
        printf("Saved ID %d to %s\n", id, path);
}

// Save all matrices to a folder
static void save_all() {

    char dir[256];
    printf("Folder path: ");
    if ( scanf(" %255s", dir)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    mkdir(dir, 0777);  // Ensure folder exists (ignore failure)

    if (save_all_to_dir(dir, &g_reg) < 0)
        printf("Failed to save all\n");
    else
        printf("Saved all matrices to %s\n", dir);
}

//Menu option of Display ALL

// List all matrices currently in memory
static void list_all() {

    if (g_reg.count == 0) {
        printf("No matrices in memory.\n");
        return;
    }
    for (int i = 0; i < g_reg.count; i++) {
        Matrix *m = g_reg.items[i];
        if (m == NULL)
            continue;
        print_matrix_with_header(m);
    }
}

//Reads two matrix IDs from the user.
//Retrieves matrices A and B from the registry.
//Measures execution time for both paths.
//Stores the single-process result in memory (registry).
//Prints both results.
//Multi-process result is NOT stored (only printed then freed).
//Single-process is stored because it has a unique ID.
static void add_two() {

    int idA, idB;        // variables to store user-selected matrix IDs
    printf("A ID: ");      // ask user for first matrix ID
    // read first ID
      if ( scanf(" %d", &idA)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    printf("B ID: ");// ask for second matrix ID
   // read second ID
      if ( scanf(" %d", &idB)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    char a[MAX_NAME], b[MAX_NAME];// buffers to store string keys for registry

    snprintf(a, sizeof(a), "%d", idA);// convert idA (int) → string key
    snprintf(b, sizeof(b), "%d", idB);// convert idB (int) → string key

    Matrix *A = registry_get(&g_reg, a);// retrieve matrix A from registry
    Matrix *B = registry_get(&g_reg, b);// retrieve matrix B from registry

    if (!A || !B) {// full IF BLOCK: ensure both matrices exist
        printf("missing matrices\n");                              
        return;                                                 
    }

    char out_single[MAX_NAME];// buffer for temporary output matrix name (string ID)
    snprintf(out_single, sizeof(out_single), "%d", g_next_id);// next available ID before assignment

    uint64_t t0 = now_millis();// start time for single-process timing

    Matrix *C_single = op_add_single(A, B, out_single);// perform single-process matrix addition

    uint64_t t1 = now_millis();// end time measurement for single-process mode

    if (!C_single) { // full IF BLOCK
        printf("add failed\n");                                   
        return;                                                   
    }

    int id_single = assign_new_id(C_single);// assign unique ID and rename matrix accordingly
    registry_add(&g_reg, C_single);// store result matrix in registry

    printf("\n(ID=%d, %dx%d) \n[SINGLE-PROCESS result] (OMP=%s)  time=%llums\n",
           id_single, // print assigned ID
           C_single->rows, C_single->cols,// dimensions
           omp_state_str(),// OMP status (ON/OFF)
           (unsigned long long)(t1 - t0));// execution time in ms

    print_matrix_raw(C_single);// display final matrix values (no header)

    t0 = now_millis();// start timing: multi-process version

    Matrix *C_mp = op_add_processes(g_pool, A, B, "_tmp");

    uint64_t t2 = now_millis();// end timing for multi-process addition

    if (C_mp) {// full IF BLOCK: ensure multi-process succeeded
        printf("\n[MULTI-PROCESS result] (already saved)   time=%llums\n",
               (unsigned long long)(t2 - t0));// print execution time
        print_matrix_raw(C_mp);// display multi-process final matrix
        matrix_free(C_mp);// free temporary result (not saved to registry)
    }
    else {// fallback if it failed
        printf("\n[MULTI-PROCESS] failed\n");// error message
    }
}
// as add
static void sub_two() {

    int idA,idB;                                            

    printf("A ID: ");                                       
    if ( scanf(" %d", &idA)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }                                   
    printf("B ID: ");                                      
     if ( scanf(" %d", &idB)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }                                    

    char a[MAX_NAME], b[MAX_NAME];                         

    snprintf(a, sizeof(a), "%d", idA);// convert idA to string key
    snprintf(b, sizeof(b), "%d", idB);// convert idB to string key


    Matrix *A = registry_get(&g_reg, a);// retrieve matrix A by key
    Matrix *B = registry_get(&g_reg, b);// retrieve matrix B by key

    //CHECK: both matrices must exist -
    if (!A || !B) {                                          
        printf("missing matrices\n");                      
        return;                                           
    }
    char out_single[MAX_NAME];       // name buffer for output matrix
    snprintf(out_single, sizeof(out_single), "%d", g_next_id); // temporary name matching next ID
    uint64_t t0 = now_millis();// timestamp: start single-process
    Matrix *C_single = op_sub_single(A, B, out_single);      // do subtraction (single process)
    uint64_t t1 = now_millis();// timestamp: end single-process


    //CHECK: ensure subtraction succeeded
    if (!C_single) {// check returned matrix pointer
        printf("sub failed\n");                          
        return;                                             
    }

    int id_single = assign_new_id(C_single);       
    registry_add(&g_reg, C_single);                  

    printf("\n(ID=%d, %dx%d) \n[SINGLE-PROCESS result] (OMP=%s)  time=%llums\n",
           id_single,                                
           C_single->rows, C_single->cols,                   
           omp_state_str(),                                 
           (unsigned long long)(t1 - t0));                 

    print_matrix_raw(C_single);                
    t0 = now_millis();    // timestamp: start multi-process
    Matrix *C_mp = op_sub_processes(g_pool, A, B, "_tmp");   // perform subtraction via processes

    uint64_t t2 = now_millis(); // timestamp: end multi-process

    //CHECK: multi-process success
    if (C_mp) {                                             
        printf("\n[MULTI-PROCESS result] (already saved)    time=%llums\n",
               (unsigned long long)(t2 - t0));   

        print_matrix_raw(C_mp);               
        matrix_free(C_mp);                     
    }
    else {                                     
        printf("\n[MULTI-PROCESS] failed\n");     
    }
}
// (1) single-process (optionally with OMP)
//(2) multi-process using child processes + pipes
//Measures execution time for both.
static void mul_two() {

    int idA, idB;              

    printf("A ID: ");                                   
    if ( scanf(" %d", &idA)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }               

    printf("B ID: ");                              
     if ( scanf(" %d", &idB)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }                      
    char a[MAX_NAME], b[MAX_NAME];                           // buffers for string keys
    snprintf(a, sizeof(a), "%d", idA);                       // convert numeric idA → string key
    snprintf(b, sizeof(b), "%d", idB);                       // convert numeric idB → string key
    Matrix *A = registry_get(&g_reg, a);                     // fetch matrix A from registry
    Matrix *B = registry_get(&g_reg, b);                     // fetch matrix B from registry

    if (!A || !B) {                                          // if either pointer is NULL
        printf("missing matrices\n");                        // print error
        return;                                              // abort operation
    }
    char out_single[MAX_NAME];                           
    snprintf(out_single, sizeof(out_single), "%d", g_next_id); // temp name matching next ID
    uint64_t t0 = now_millis();                            
    Matrix *C_single = op_mul_single(A, B, out_single);      
    uint64_t t1 = now_millis();             
    
    //CHECK: multiplication succeeded 
    if (!C_single) {                                     
        printf("mul failed\n"); 
        return;
    }
    int id_single = assign_new_id(C_single);                
    registry_add(&g_reg, C_single);                        
    printf("\n(ID=%d, %dx%d)\n[SINGLE-PROCESS result] (OMP=%s)  time=%llums\n",
           id_single,                                        
           C_single->rows, C_single->cols,                  
           omp_state_str(),                              
           (unsigned long long)(t1 - t0));              

    print_matrix_raw(C_single);                            
    t0 = now_millis();                                     
    Matrix *C_mp = op_mul_processes(g_pool, A, B, "_tmp");   
    uint64_t t2 = now_millis();                          


    //CHECK: multi-process success
    if (C_mp) {                                           
        printf("\n[MULTI-PROCESS result] (already saved) (OMP=%s) time=%llums\n",
               omp_state_str(),                        
               (unsigned long long)(t2 - t0));      
        print_matrix_raw(C_mp);                             
        matrix_free(C_mp);                                 
    }
    else {                                                  
        printf("\n[MULTI-PROCESS] failed\n");                
    }
}

static void determinant() {

    int id;// variable to store user-entered ID
    printf("Matrix ID: ");// ask for matrix ID
    // read ID into 'id'
     if ( scanf(" %d", &id)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }

    char key[MAX_NAME];// buffer to hold the string key
    snprintf(key, sizeof(key), "%d", id);// convert numeric ID → string key


    Matrix *A = registry_get(&g_reg, key);// fetch matrix from registry

    //CHECK: matrix must exist
    if (!A) {// if lookup returned NULL
        printf("not found\n");// inform user
        return;                                            
    }

    //CHECK: matrix must be square 
    if (A->rows != A->cols) {// determinant requires square matrix
        printf("error: determinant requires a square matrix\n");
        return;                                       
    }

    uint64_t t0 = now_millis();// timestamp: start single-process
    double d_single = op_det_single(A);// compute determinant single-process
    uint64_t t1 = now_millis();// timestamp: end single-process

    printf("\n(ID=%d, %dx%d)\n[SINGLE-PROCESS det] (OMP=%s) = %.6f  time=%llums\n",
           id,                                               
           A->rows, A->cols,                                
           omp_state_str(),                               
           d_single,                                   
           (unsigned long long)(t1 - t0));         
    t0 = now_millis();                                    
    double d_mp = op_det_processes(g_pool, A);         
    uint64_t t2 = now_millis();                 
    printf("[MULTI-PROCESS det] (OMP=%s) = %.6f  time=%llums\n",
#ifdef HAVE_OMP
           g_omp_enabled ? "ON" : "OFF",                      // OMP state if available
#else
           "OFF",                                             // always OFF if no OMP support
#endif
           d_mp,                                              // returned multiprocess det
           (unsigned long long)(t2 - t0));                    // elapsed multi-process time
}

static void eigen() {

    int id;// user-entered matrix ID
    printf("Matrix ID (square): ");// ask user for ID
    // read ID into 'id'
 if ( scanf(" %d", &id)!= 1) {        
        fprintf(stderr, "Invalid entry.\n");
        return;
    }
    char key[MAX_NAME];// string buffer for ID→key
    snprintf(key, sizeof(key), "%d", id);// convert numeric ID → text key
    Matrix *A = registry_get(&g_reg, key);// fetch matrix from registry

    //CHECK: matrix must exist
    if (!A) {// if NULL returned
        printf("not found\n");// notify user
        return;// stop execution
    }

    //CHECK: matrix must be square 
    if (A->rows != A->cols) {// eigenvalues require square matrix
        printf("error: eigen requires a square matrix (got %dx%d)\n",
               A->rows, A->cols);
        return;
    }
    double lambda1, lambda2;// eigenvalues for both modes
    double *vec1 = NULL, *vec2 = NULL;// eigenvectors (heap allocated)


    //SINGLE-PROCESS EIGEN
    uint64_t t0 = now_millis();// timestamp start
    int it1 = op_eigen_power(// dominant eigen via power method
                    A,
                    1e-6,// tolerance
                    1000,// max iterations
                    &lambda1,// output eigenvalue
                    &vec1);// output eigenvector
    uint64_t t1 = now_millis();// timestamp end

    if (it1 < 0) {// negative = failure
        printf("single-process eigen failed\n");
        return;
    }

    printf("\n(ID=%d, %dx%d) \n[SINGLE-PROCESS eigen] (OMP=%s)  lambda ~ %.8f (iters=%d)  time=%llums\n",
           id, A->rows, A->cols, omp_state_str(),
           lambda1, it1, (unsigned long long)(t1 - t0));

    printf("eigenvector:\n");// print eigenvector
    for (int i = 0; i < A->rows; i++)
        printf("%g\n", vec1[i]);// each component
    free(vec1);// free allocated vector

    //MULTI-PROCESS EIGEN
    t0 = now_millis();// timestamp start
    int it2 = op_eigen_processes(// multiprocess eigen solver
                    g_pool,// worker pool
                    A,
                    1e-6,// tolerance
                    1000,// max iterations
                    &lambda2,// output eigenvalue
                    &vec2);// output eigenvector
    uint64_t t2 = now_millis();// timestamp end

    if (it2 < 0) {// if failed
        printf("[MULTI-PROCESS eigen] failed\n");
    }
    else {
        printf("\n[MULTI-PROCESS eigen] ( OMP=%s)  lambda ~ %.8f (iters=%d)  time=%llums\n",
#ifdef HAVE_OMP
               g_omp_enabled ? "ON" : "OFF",// print OMP state if available
#else
               "OFF",// otherwise OFF
#endif
               lambda2, it2, (unsigned long long)(t2 - t0));

        printf("eigenvector:\n");// print vector
        for (int i = 0; i < A->rows; i++)
            printf("%g\n", vec2[i]);
        free(vec2);// free allocated vector
    }
}
//Toggle the global flag controlling whether OpenMP-based computations are used in single-process operations.
//If OpenMP is not compiled in, enabling it shows a message.

//Enable OpenMP if supported 
static void enable_omp() {
#ifdef HAVE_OMP// compiler defines HAVE_OMP only if OpenMP enabled
    g_omp_enabled = 1;// set global flag → ON
    printf("OpenMP: ENABLED\n");// notify user
#else
    g_omp_enabled = 0;// force OFF (not supported)
    printf("OpenMP: NOT AVAILABLE (compile environment lacks OpenMP)\n");
#endif
}

//Disable OpenMP explicitly
static void disable_omp() {
    g_omp_enabled = 0;// set global flag → OFF
    printf("OpenMP: DISABLED\n");// notify user
}

static const char *op_label_by_code(int code) {
    switch (code) {
        case 1:  return "Enter a matrix";
        case 2:  return "Display a matrix";
        case 3:  return "Delete a matrix";
        case 4:  return "Modify a matrix";
        case 5:  return "Read a matrix from a file";
        case 6:  return "Read a set of matrices from a folder";
        case 7:  return "Save a matrix to a file";
        case 8:  return "Save all matrices in memory to a folder";
        case 9:  return "Display all matrices in memory";
        case 10: return "Add 2 matrices";
        case 11: return "Subtract 2 matrices";
        case 12: return "Multiply 2 matrices";
        case 13: return "Find the determinant of a matrix";
        case 14: return "Find eigenvalues & eigenvectors (dominant only)";
        case 15: return "Exit";
        case 16: return "Enable OpenMP";
        case 17: return "Disable OpenMP";
        default: return "Unknown";
    }
}

//Display the dynamic menu for the matrix IPC tool.
//Shows OpenMP status and lists all menu items in order.
//Menu entries are retrieved from AppConfig structure.
static void print_menu_dynamic(const AppConfig *cfg) {
    printf("\n    Matrix operation    \n");// title
    printf("[OMP %s]\n",                                         
#ifdef HAVE_OMP
           g_omp_enabled ? "available: ENABLED" : "available: disabled" // show OMP status if compiled
#else
           "unavailable (compile environment lacks OpenMP)"   // otherwise show not available
#endif
    );

    //Loop through menu entries and print them 
    for (int i = 0; i < cfg->menu_count; i++) {
        int code = cfg->menu_order[i];// get operation code
        printf("%d) %s\n", i + 1, op_label_by_code(code));    // display menu item
    }
    printf("> ");// prompt symbol
}

//Load application configuration from a text file.
//Set default values if file is missing or entries absent.
//config includes: matrix_dir, workers count, menu_order.
int load_config(const char *path, AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));// clear structure
    strcpy(cfg->matrix_dir, "data/mat");// default matrix directory
    cfg->workers = 4;// default worker threads/processes
    for (int i = 0; i < 17; i++) {                             
        cfg->menu_order[i] = i + 1;// default menu order
        cfg->menu_count = 17;// default menu count
    }

    FILE *f = fopen(path, "r");// open config file
    if (!f) return -1;// if missing, return failure

    char line[512];// buffer for each line
    while (fgets(line, sizeof(line), f)) {// read line by line
        if (line[0] == '#' || strlen(line) < 3) continue;      // skip comments or short lines

        char key[64], val[448];// buffers for key/value
        if (sscanf(line, "%63[^=]=%447[^\n]", key, val) == 2) { // parse key=value
            if (strcmp(key, "matrix_dir") == 0) {// custom matrix directory
                printf(cfg->matrix_dir, sizeof(cfg->matrix_dir), "%s", val);
            } 
            else if (strcmp(key, "menu_order") == 0) {// custom menu order
                cfg->menu_count = 0;// reset count
                char *p = val;
                while (*p) {// parse comma/space separated numbers
                    int x = (int)strtol(p, &p, 10);           // convert to integer
                    if (x >= 1 && x <= 17)                     // valid menu code
                        cfg->menu_order[cfg->menu_count++] = x;
                    if (*p == ',' || *p == ' ') p++;           // skip separators
                    else if (*p) p++;// skip other chars
                }
            } 
            else if (strcmp(key, "workers") == 0) {// custom worker count
                cfg->workers = atoi(val);// convert to int
            }
        }
    }

    fclose(f);// close file
    return 0;// success
}
// Initialize matrix registry and worker pool.
// Load matrices from default/configured folder.
// Display interactive menu for the user to perform matrix operations.
// Handle single- and multi-process arithmetic, determinant, eigen computations.
//Manage OpenMP enable/disable dynamically.
void run_menu(AppConfig *cfg) {

    registry_init(&g_reg);// initialize global matrix registry
    load_directory(cfg->matrix_dir, &g_reg);// load matrices from directory
    if (g_reg.count > 0) {// if matrices were loaded
        for (int i = 0; i < g_reg.count; i++) {// iterate through loaded matrices
            Matrix *m = g_reg.items[i];                
            if (!m) continue;// skip null entries
            rename_to_id(m, i + 1);// assign sequential ID to each matrix
        }
        g_next_id = g_reg.count + 1;// set next available ID
    }
    g_pool = pool_create(cfg->workers);// create worker pool with configured number of processes
    int running = 1;// menu loop control flag
    while (running) {// main menu loop
        print_menu_dynamic(cfg);// display menu dynamically

        int choice = 0;                                
        if (scanf("%d", &choice) != 1) {// read user input and check for invalid input
            printf("bad input\n"); 
            break;// exit menu on invalid input
        }

        if (choice < 1 || choice > cfg->menu_count) {  // check if choice is out of menu bounds
            printf("invalid choice\n"); 
            continue;// repeat menu loop
        }

        int op_code = cfg->menu_order[choice - 1];// map user choice to actual operation code

        //execute operation according to op_code using switch-case 
        switch (op_code) {
            case 1:  enter_matrix(); break;            // create a new matrix
            case 2:  display_matrix(); break;          // display a matrix
            case 3:  delete_matrix(); break;           // delete a matrix
            case 4:  modify_matrix(); break;           // modify matrix values
            case 5:  read_matrix(); break;             // load matrix from file
            case 6:  read_dir(); break;                // load all matrices from folder
            case 7:  save_matrix(); break;             // save a matrix to file
            case 8:  save_all(); break;                // save all matrices to folder
            case 9:  list_all(); break;                // list all matrices in memory
            case 10: add_two(); break;                  // add two matrices
            case 11: sub_two(); break;                  // subtract two matrices
            case 12: mul_two(); break;                  // multiply two matrices
            case 13: determinant(); break;             // compute determinant
            case 14: eigen(); break;                   // compute dominant eigenvalue/vector
            case 15: running = 0; break;               // exit menu
            case 16: enable_omp(); break;              // enable OpenMP
            case 17: disable_omp(); break;             // disable OpenMP
            default: printf("unknown op\n");           // fallback for unexpected code
        }
    }
    pool_destroy(g_pool);// destroy worker pool on exit
    registry_free(&g_reg);// free all matrices and clear registry
}

