#ifndef MENU_H
#define MENU_H
#include "matrix.h"
#include "pool.h"
// menu need to work, dir: to load files from,menu order: from config to customize the order as user want, menu count to to use it between what user use and what acully it code for , workers number from config to send it to pool
typedef struct {
    char matrix_dir[256];
    int  menu_order[32];
    int  menu_count;
    int  workers;
} AppConfig;

int load_config(const char *path, AppConfig *cfg);
void run_menu(AppConfig *cfg);

#endif
