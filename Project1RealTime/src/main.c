#include "common.h"
#include "menu.h"

// Global flag used to detect Ctrl+C interruption
static volatile sig_atomic_t g_interrupted = 0;

// SIGINT handler (triggered on Ctrl+C)
static void on_sigint(int sig) {
    (void)sig;     // Unused parameter
    g_interrupted = 1; // Mark that interruption happened
}

// SIGCHLD handler to reap zombie processes
static void on_sigchld(int sig) {
    (void)sig;  // Unused parameter
    // Reap all exited child processes without blocking
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}
//the make fie run this first
int main(int argc, char **argv) {

    AppConfig cfg; // Holds configuration values
    const char *config_path = "config/default.conf";  // Default config path

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {

        // config <path>
        if (strcmp(argv[i], "--config") == 0 && (i + 1) < argc) {
            config_path = argv[i + 1];      // Use the provided config file
            i++;   // Skip next argument
        }
        // help
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--config path]\n", argv[0]);
            return 0;  // Exit after showing help
        }
    }

    // Load configuration file, this function in menu
    load_config(config_path, &cfg);

    // Register signal handlers
    signal(SIGINT, on_sigint);   // Handle Ctrl+C
    signal(SIGCHLD, on_sigchld); // Reap child processes

    (void)g_interrupted; // Silence unused-warning for now

    // Run the main interactive menu, given the config content 
    run_menu(&cfg);
    return 0;
}
