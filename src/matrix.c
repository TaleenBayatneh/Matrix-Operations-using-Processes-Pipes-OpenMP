#include "matrix.h"

// Initializes an empty matrix registry.
void registry_init(MatrixRegistry *r) {
    r->items = NULL;       // No items yet
    r->count = 0;          // Count of matrices
    r->cap = 0;            // Capacity of items array
}

// Frees all matrices and the registry itself.
void registry_free(MatrixRegistry *r) {
    // Free each matrix in the registry
    for (int i = 0; i < r->count; i++) {
        if (r->items[i] != NULL) {
            matrix_free(r->items[i]);
        }
    }
    free(r->items);       // Free the items array
    r->items = NULL;
    r->count = 0;
    r->cap = 0;
}

// Compares two matrix names (up to MAX_NAME characters)
static int name_eq(const char *a, const char *b) {
    return strncmp(a, b, MAX_NAME) == 0;
}

// Retrieves a matrix by name from the registry.
// Returns NULL if not found.
Matrix *registry_get(MatrixRegistry *r, const char *name) {
    for (int i = 0; i < r->count; i++) {
        if (r->items[i] != NULL) {
            if (name_eq(r->items[i]->name, name)) {
                return r->items[i];   // Found the matrix
            }
        }
    }
    return NULL;   // Not found
}

// Adds a matrix to the registry.
// Returns 0 on success, -1 if matrix is NULL or already exists.
int registry_add(MatrixRegistry *r, Matrix *m) {
    if (m == NULL) {
        return -1;  // Cannot add NULL
    }
    // Check for duplicate by name
    if (registry_get(r, m->name) != NULL) {
        fprintf(stderr, "Matrix '%s' already exists.\n", m->name);
        return -1;
    }
    // Grow the items array if needed
    if (r->count == r->cap) {
        if (r->cap == 0) {
            r->cap = 8;
        } else {
            r->cap = r->cap * 2;
        }
        r->items = realloc(r->items, (size_t)r->cap * sizeof(Matrix*));
        if (r->items == NULL) {
            die("realloc registry");
        }
    }
    // Add the matrix to the array
    r->items[r->count] = m;
    r->count++;

    return 0;
}

// Removes a matrix by name from the registry.
// Returns 0 on success, -1 if not found.
int registry_remove(MatrixRegistry *r, const char *name) {
    for (int i = 0; i < r->count; i++) {
        if (r->items[i] != NULL) {
            if (name_eq(r->items[i]->name, name)) {
                matrix_free(r->items[i]);     // Free the matrix
                // Shift remaining items left
                for (int k = i + 1; k < r->count; k++) {
                    r->items[k - 1] = r->items[k];
                }
                r->count--;
                return 0;  // Success
            }
        }
    }
    fprintf(stderr, "Matrix '%s' not found.\n", name);
    return -1;
}

// Creates a new matrix with the specified id, number of rows and columns.
// Returns A pointer to the newly created Matrix. The caller is responsible for freeing it
Matrix *matrix_create(const char *name, int rows, int cols) {
    // Allocate memory for the Matrix structure
    Matrix *m = (Matrix*)xmalloc(sizeof(Matrix));
    // Clear all fields to zero to avoid uninitialized values
    memset(m, 0, sizeof(*m));
    // Copy the name into the matrix,the name is an id "incremented seq "provided by the calller
    if (name != NULL) {
        strncpy(m->name, name, MAX_NAME - 1);
        m->name[MAX_NAME - 1] = '\0';
    } else {
        m->name[0] = '\0';  // Empty string if no name provided
    }
    // Set matrix dimensions
    m->rows = rows;
    m->cols = cols;

    // Allocate memory for the matrix data (rows * cols doubles)
    m->data = (double*)xmalloc((size_t)rows * cols * sizeof(double));

    // Initialize all elements of the data array to zero
    memset(m->data, 0, (size_t)rows * cols * sizeof(double));
    return m; // Return the pointer to the newly created matrix
}

// Frees a matrix and its data
void matrix_free(Matrix *m) {

    if (m == NULL) {
        return;  // Nothing to free
    }
    free(m->data);
    free(m);
}
