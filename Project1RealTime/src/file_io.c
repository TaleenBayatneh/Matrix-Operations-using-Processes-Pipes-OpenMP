#include "common.h" 
#include "file_io.h"
#include <sys/stat.h>
#include <sys/types.h>

// Checks whether the given string `s` ends with the suffix `suf`, which suf is .txt |.mtx.
// Returns 1 if the suffix matches the end of the string, otherwise 0.
// This is a simple utility helper that avoids scanning the whole string to chick the ext of a file
// and only compares the last `m` characters.
static int has_suffix(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    if (m>n) 
    return 0;
    return strcmp(s + (n-m), suf) == 0;
}

// Loads a matrix from a text file and returns it through `out`.
// The file format should start with: <rows> <cols>, then rows*cols numbers.
// If `name_override` is provided, it becomes the matrix name; otherwise
// we derive the name from the filename. Returns 0 on success, -1 on error.
int read_matrix_file(const char *path, const char *name_override, Matrix **out) {

    FILE *f = fopen(path, "r"); // Try to open the matrix file
    if (!f) { 
      perror("fopen");   // print error if file couldn't be opened
      return -1; 
      }     
    int rows = 0, cols = 0;
    // Read matrix dimensions (rows and columns)
    if (fscanf(f, "%d %d", &rows, &cols) != 2) {  // Ensure both values were read
        fprintf(stderr, "Invalid matrix header in %s\n", path);
        fclose(f);
        return -1;
    }
    char name[MAX_NAME] = {0};
    // Use provided name if available, otherwise derive from the filename
    if (name_override && *name_override) {
        snprintf(name, sizeof(name), "%s", name_override);   // Copy override name
    } else {
        const char *base = strrchr(path, '/');  // Extract basename
        if (base) {
            base = base + 1;
        } else {
            base = path;
        }
        
        // snprintf is used to safely write the integer into the char array,
        snprintf(name, sizeof(name), "%.*s", MAX_NAME - 1, base); // Copy basename safely

        char *dot = strrchr(name, '.'); // Remove file extension
        if (dot) {
            *dot = 0;
        }
    }
    Matrix *m = matrix_create(name, rows, cols);  // Allocate matrix with metadata
    // Read matrix values one by one
    for (int i = 0; i < rows; i++) {    // Loop through rows
        for (int j = 0; j < cols; j++) {    // Loop through columns
            double v;

            if (fscanf(f, "%lf", &v) != 1) {// Ensure valid numeric value
                fprintf(stderr, "Invalid data in %s at (%d,%d)\n", path, i, j);
                matrix_free(m);  // Free allocated matrix on error
                fclose(f);
                return -1;
            }

            matrix_set(m, i, j, v); // Store the value in matrix cell
        }
    }

    fclose(f);  // Close file
    *out = m;  // Output the loaded matrix
    return 0; // Success
}
    
// Writes a matrix to a text file at the given path or in the same dir in a new file
// The file format is: <rows> <cols> on the first line,
// followed by the matrix values row by row.
// Returns 0 on success, or -1 if the file can't be opened.
int write_matrix_file(const char *path, const Matrix *m) {

    FILE *f = fopen(path, "w");   // Try to open file for writing
    if (!f) {    // Check if file open failed
        perror("fopen");
        return -1;
    }
    // Write matrix dimensions to the first line
    fprintf(f, "%d %d\n", m->rows, m->cols);

    // Write matrix contents row by row
    for (int i = 0; i < m->rows; i++) {       
        for (int j = 0; j < m->cols; j++) {
            // Print matrix cell value
            fprintf(f, "%.10g", matrix_get(m, i, j));
            // Add a space between values, but not after the last column
            if (j + 1 < m->cols) {
                fprintf(f, " ");
            }
        }
        fprintf(f, "\n");   // Newline after each row
    }
    fclose(f);              // Close the file
    return 0;               // Success
}
// Loads all matrix files (.txt or .mtx) from a directory into the registry.
// Returns the number of successfully loaded matrices.
int load_directory(const char *dir, MatrixRegistry *reg) {

    DIR *d = opendir(dir);  // Try to open directory
    if (!d) {      // Directory might not exist — not fatal
        return 0;
    }
    struct dirent *ent;
    int loaded = 0;
    // Read files inside directory one by one
    while ((ent = readdir(d)) != NULL) {
        // Skip hidden entries like "." and ".."
        if (ent->d_name[0] == '.') {
            continue;
        }
        // Skip files that are not .txt or .mtx
        if (!has_suffix(ent->d_name, ".txt") &&
            !has_suffix(ent->d_name, ".mtx")) 
        {
            continue;
        }
        // Build full file path
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        // Extract matrix name (filename without extension)
        char name[MAX_NAME];
        snprintf(name, sizeof(name), "%.*s", MAX_NAME - 1, ent->d_name);
        // Remove file extension
        char *dot = strrchr(name, '.');
        if (dot) {
            *dot = 0;
        }
        Matrix *m = NULL;
        // Try to load the matrix file
        if (read_matrix_file(path, name, &m) == 0) {
            registry_add(reg, m);    // Add to registry
            loaded++;    // Count successful load
        }
    }
    closedir(d);      // Close directory
    return loaded;    // Return number of loaded matrices
}

// Saves all matrices in the registry to the given directory.
// Creates the directory if it does not exist.
// Returns 0 on success, or -1 on error.
int save_all_to_dir(const char *dir, MatrixRegistry *reg) {
    struct stat st;
    // Check if directory exists; create it if not
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0777) != 0) {   // Failed to create directory
            perror("mkdir");
            return -1;
        }
    }
    // Save each matrix as a .txt file
    for (int i = 0; i < reg->count; i++) {
        Matrix *m = reg->items[i];
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.txt", dir, m->name);
        // If writing fails, stop and return error
        if (write_matrix_file(path, m) != 0) {
            return -1;
        }
    }
    return 0;           // Success
}
