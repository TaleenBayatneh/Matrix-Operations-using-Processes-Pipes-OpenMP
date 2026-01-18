#ifndef POOL_H
#define POOL_H
                      
#include "matrix.h"

typedef enum
 {                      // the enum is the option of the parent that will send it to the worker
    CMD_ADD_ELEM=1,     // so the add will get 1 sub 2 mul 3 and so on 
    CMD_SUB_ELEM=2,
    CMD_MUL_CELL=3,
    CMD_DET_ROW_ELIM=4,
    CMD_EIG_ROW_DOT=5,
    CMD_QUIT=99         // is an oeder to get the child out of the worker loop
} Command;              // so its an command from the parent to the chiled

typedef struct 
{                       // the massege struct that will send to the child , that include the jop header that will send via parent to the child by the pipe
       int cmd, job_id, i, j, n, rows, cols, payload_bytes;
} JobHeader;            // so that he will send for him the the enum of the the type of the mission, the jop id , i , j for the matrix , payload (is the size of the information after the header) and so on 
                        // so that when the parnet send the jop to the child he wil send for him the jop header and then the payload that will have the real number

typedef struct          // the same but forom the child to the parent and the row and the cols is the result
{
    int cmd, job_id, i, j, rows, cols, payload_bytes;
} ResultHeader;        


typedef struct {
    pid_t pid;           // the struct if the worker or the child 

    int to_child;        // present one child in the pool , pid the number of the process that assghind to the child , to_child is the the pipe from the parent to the child , from_child is the pipe that the parent read the information from the given child 

    int from_child;      

    int busy;            // and does the child is busy or not , 0 free , 1 is busy
} Worker;

typedef struct
 {
    int n;              // the struct of the worker pool n is the number of the child , and the w is an matrix of the child
    Worker *w;

} Pool;


Pool *pool_create(int n);         // create a pool of n of child
void  pool_destroy(Pool *p);      // create cmd_quit for each child and destroy them 
int pool_find_idle(Pool *p);      // search for a worker that its not busy as we see the struct of the child and he will give you his index also 
int pool_send(int wi, Pool *p, const JobHeader *h, const void *payload);    // he will send the jopheader and a playload for a spicific child
int pool_wait_any(Pool *p, int *wi);  // wait for a child to finsh and return his ponter
int pool_recv(int wi, Pool *p, ResultHeader *rh, void *payload_buf, size_t bufcap);  // read the result from a child that finsh (resultheader+patload)

#endif
