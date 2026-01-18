#include "common.h"
#include "pool.h"
#include "timer.h"
#include <omp.h>
static void worker_loop(int read_fd, int write_fd);
static ssize_t read_exact(int fd, void *buf, size_t n) { return read_all(fd, buf, n); }
static ssize_t write_exact(int fd, const void *buf, size_t n) { return write_all(fd, buf, n); }

Pool *pool_create(int n)      // function that will create the pool for the child
{                             // and all the pipe between the parent , so it will create all the worker and each child will have his own procses indepnded and create tow pipe child>parent , parent >child
    if (n <= 0) n = 1;        // check that the worker at least 1
                              
    Pool *p = (Pool*)xmalloc(sizeof(Pool));
                              // get for the child a safe please (xmalloc are in the common.h the safe verstion of the malloc)
    p->n = n;                 // point the n that the worker is pointer on it , its an intger number comming ftom the struct that in the pool.h so it the number of the child in the pool

    p->w = (Worker*)xmalloc((size_t)n * sizeof(Worker));
                              // if we have 4 child we will make 4 struct from varaible worker
    memset(p->w, 0, (size_t)n * sizeof(Worker));
                              // for a safty we will use memset to make all struct we made it for each  worker have inial value of 0

    for (int i=0;i<n;i++) 
                              // make the tow channal pipe p2c, c2p , each pipe will give me an 2 struct from the file descriptores
    {
        int p2c[2], c2p[2];   // so the pipe [0] is for reading side and pipe[1] is for writing side
        if (pipe(p2c) < 0) die("pipe p2c");   
                                             // if there is an problem move it to the function of the die that in the common.h yjat will till us from were exactly the problem comming from
        if (pipe(c2p) < 0) die("pipe c2p");   
       
        pid_t pid = fork();                  // it will create the process using fork and if there is an problem with the createion we will see it in the die function

        if (pid < 0) die("fork");            // after creating the fork the pid 0 for the child and 1 for the parent

        if (pid == 0) 
        {
                                             // this is for the child after we create it and we see that the pid of it is 0 we need to closed the p2c[1]  parent -> child pipe so that the child did not write on the pipe of the parent only read and the c2p[0] child -> parent pipe and the child did not read from the child pipe he will write on it since the
            close(p2c[1]); 
            close(c2p[0]);
            
            worker_loop(p2c[0], c2p[1]);    // send the child to the worker loop with all the information and pipe open ,after that we will finsh and get out we used _exit since we are on a another procsess that not related to the fork
            _exit(0);                       
        } else
         {
                                            // if the pid>0 so that it will make the same think in the prevoues put know we are in the parent
            close(p2c[0]); 
            close(c2p[1]);

            p->w[i].pid = pid;              // we needto store the pid of the child with the parent so that we can call him as a parent ,so we will point in the worker array in the struct and put his pid and store it we store it in the worker struct
            p->w[i].to_child = p2c[1];      // we need to know were we will write to the child so p2c[1] is the write end from the pipe of the parent ->child we store it in the to_child since this is the channle that the parent can write the order to the child
            p->w[i].from_child = c2p[0];    // we to read when the child finsh the process
            p->w[i].busy = 0;               // the child is busy or he is free to called from the parent , so when the pid is > 0 the parent will save the child inforamtion in the pool
        }
    }
    return p;
}

void pool_destroy(Pool *p)
 {
    if (!p) return;     // if the pointer p eaqal null do not do anyting

    for (int i=0;i<p->n;i++)  // order to quit everything , we loop though all the child in the pool we give him a jop header that contain a order to quit 99 in the pool.h the struct of the enum the order for the CMD_Quit = 99 , 0 everything
     {  
        JobHeader h = { 
            .cmd = CMD_QUIT, .job_id = 0, .payload_bytes = 0 
        };                   // we use the write exact to write this header with the pipe of the to_child for each child
        write_exact(p->w[i].to_child, &h, sizeof(h));
    }

    for (int i=0;i<p->n;i++)
     {                       // we will wait the child with the pid to finsh , and aslo closed the all the pipe from the child and from the parent
        int status=0;        // so that after it finsh everything we will free all the process we make it 
        waitpid(p->w[i].pid, &status, 0);
        close(p->w[i].to_child);
        close(p->w[i].from_child);
    }
    free(p->w);
    free(p);
}

int pool_find_idle(Pool *p)
    {                           // find a free child in the pool 
    for (int i=0;i<p->n;i++)    // it go to the child by child and see his state if he is busy or not, if he is busy it will return his number
        if (!p->w[i].busy) return i;

    return -1;                  // if all of thim are busy return -1 all of them are in the process
}

int pool_send(int wi, Pool *p, const JobHeader *h, const void *payload)
 {
    Worker *w = &p->w[wi];      // function to send a jop for a child in the pool

    if (w->busy) {              // if this child if busy we will return the error or -1 and make the errno ponit on the busy
         errno = EBUSY; return -1;
         }
    if (write_exact(w->to_child, h, sizeof(*h)) != sizeof(*h)) return -1;
                                // if the worker is not busy so we need to send him a jop by the pipe, so we set the jop header and point it using the to child pipe        
    if (h->payload_bytes > 0)
     {
        if (write_exact(w->to_child, payload, (size_t)h->payload_bytes) != h->payload_bytes) return -1;
    }                           // if there is an payload on the child like an information of the matrix or some number need for prosses we will send it via the pipe also 
    w->busy = 1;                // this child is busy know so we will set the flag of the busy to 1
    return 0;
}

int pool_wait_any(Pool *p, int *wi) 
{                               // impotant function to let the perant or the system wait intel the all the child is finsh wihout freez the hile system
    struct pollfd fds[128];     // we will use polldf asa buffer to store all the channle that are open from the currant childs, if the number of child more than 128 then there is an error
                            
    if (p->n > 128) die("increase poll array");

    int nfds = 0;             

    for (int i=0;i<p->n;i++)
     {
        if (p->w[i].busy) {                    // so we need to prepare all the channle of the worker that we need to monitor them so first all we need only the worker who are busy =1
            fds[nfds].fd = p->w[i].from_child; // then we will see the pipe that we are reading from him the result , so we will monter this fd may be the child will send us a information
            fds[nfds].events = POLLIN;         // monitor this fd and see if there is an evernt coming from this channle only if there is data , 3 type of event 5 type of event or flag for the kirnal 
            fds[nfds].revents = 0;
            nfds++;
        }                
    }
    if (nfds == 0) return 0;                   // nothing to wait for since there are no worker busy all of them are free


    int r = poll(fds, nfds, -1);               // varaible type pollfd have all the pipe we are monitor, nfds the number of the varivle that are busy , -1 for wating for infinite
    if (r < 0)                                 // if there is an error while wating
    {
        if (errno == EINTR) return -1;
        die("poll");
    }
                                               // we need to find the worker that finsh so if the channle pipe have an event of pollin then we have on this channle a information and then store the number of this worker and save it in *w
    int idx=0;
    for (int i=0;i<p->n;i++) 
    {
        if (!p->w[i].busy) continue;

        if (fds[idx].revents & POLLIN) { *wi = i; return 1; }
        idx++;
    }
    return -1;
}

int pool_recv(int wi, Pool *p, ResultHeader *rh, void *payload_buf, size_t bufcap) {
    Worker *w = &p->w[wi];            // function to read the information after the child is finsh and we know from the poll funstion
    if (!w->busy) {                   // we get the worker first from his number and check for his state then 
         errno = EINVAL; return -1; } // then read the result header , first we read the head of the result from the pipe child , this head will have the cmd i,j,paylod,row,clom as we mintion of the struct of the worker in the pool.h

    if (read_exact(w->from_child, rh, sizeof(*rh)) != sizeof(*rh)) return -1;

    if (rh->payload_bytes > 0)        // then we need to chek the payload size and if it bigger than the buffer so there is an problem , then we read the inforamtion
    {
        if ((size_t)rh->payload_bytes > bufcap) {

            fprintf(stderr, "Result payload too big (%d bytes)\n", rh->payload_bytes);

            return -1;
        }
        if (read_exact(w->from_child, payload_buf, (size_t)rh->payload_bytes) != rh->payload_bytes) return -1;
    }
    w->busy = 0;
    return rh->payload_bytes;          // return the size of the byte and the inforamtn we get
}



static void handle_add(const JobHeader *h, int rfd, int wfd) {
    (void)rfd;                            // function for add we will get the jop inforamtion type, location ,size and the number of it in the read and write
    double x[2];                          // set a array of tow varible 
    read_all(rfd, x, sizeof(x));          // read the inforamion fully with the read all function that we make it in the common.h
    double c = x[0] + x[1];               // so the result of this matrix know having tow number from the parent send
    ResultHeader rh = { .cmd = h->cmd, .job_id = h->job_id, .i=h->i, .j=h->j, .rows=h->rows, .cols=h->cols, .payload_bytes=(int)sizeof(double) };
    write_all(wfd, &rh, sizeof(rh));      // we will set the head of the result that the proccses will handle it then with all the information of the jop 
    write_all(wfd, &c, sizeof(double));   // then send the result to the perant by the pipe
}                                         // so we will recive an information , then we will make the procses, then we will send the resullt back


static void handle_sub(const JobHeader *h, int rfd, int wfd) {
    double x[2];                          // function for a sub the same as the add function
    read_all(rfd, x, sizeof(x));
    double c = x[0] - x[1];
    ResultHeader rh = { .cmd = h->cmd, .job_id = h->job_id, .i=h->i, .j=h->j, .rows=h->rows, .cols=h->cols, .payload_bytes=(int)sizeof(double) };
    write_all(wfd, &rh, sizeof(rh));

    write_all(wfd, &c, sizeof(double));
}


static void handle_mul_cell(const JobHeader *h, int rfd, int wfd) {
    int n = h->n;                         // function for multiblation the same as before 
    double *row = (double*)malloc((size_t)n * sizeof(double));
    double *col = (double*)malloc((size_t)n * sizeof(double));

    read_all(rfd, row, (size_t)n * sizeof(double));
    read_all(rfd, col, (size_t)n * sizeof(double));
    double s = 0.0;
#pragma omp parallel for if(g_omp_enabled)  // here we have the openmp enable or disable depened on the varible we will set in the menu
    for (int k=0;k<n;k++) s += row[k] * col[k];
    ResultHeader rh = { .cmd = h->cmd, .job_id = h->job_id, .i=h->i, .j=h->j, .rows=1, .cols=1, .payload_bytes=(int)sizeof(double) };
    write_all(wfd, &rh, sizeof(rh));
    write_all(wfd, &s, sizeof(double));
    free(row); free(col);
}


static void handle_det_row_elim(const JobHeader *h, int rfd, int wfd) {
    int n = h->n;    
                                  // function for determinant row elimination , we used the gaussian elimination , each worker will edit one row only, depened on the pivot row
    int k = h->j;                 // so that the k is the pivot column index and the n is the colomn , we create a space for a memory for both of them, , then we will read the rows form the parent by using the read all function       
                                  // thw orker will read tow ros from the pipe rfd the prow and the row the currant one ,the parent will send him as a payload
    double *prow = (double*)malloc((size_t)n * sizeof(double));
    double *row  = (double*)malloc((size_t)n * sizeof(double));
    read_all(rfd, prow, (size_t)n * sizeof(double));
    
    read_all(rfd, row,  (size_t)n * sizeof(double));
    double pivot = prow[k];       // we will take the pivot row and calculate the the rate of the the currant row with the pivot row
                                  // then we will implementing class deletion currantly at k the k is the colomn to the last colomn , then we will subtraction each number with the currunat colomn 
    double factor = (pivot!=0.0) ? (row[k] / pivot) : 0.0;
    for (int c=k;c<n;c++) row[c] -= factor * prow[c];
                                  // after finsh we will prepare the the pipe and put all the inforamation we get with the result header and then we willsend it back by pipe to the parent
    row[k] = 0.0;                 // then we will write to the parent with the reasly header , then free the memory
    ResultHeader rh = { .cmd = h->cmd, .job_id=h->job_id, .i=h->i, .j=h->j, .rows=1, .cols=n, .payload_bytes=(int)(n*sizeof(double)) };
    write_all(wfd, &rh, sizeof(rh));
    write_all(wfd, row, (size_t)n * sizeof(double));
    free(prow); free(row);
}



static void handle_eig_row_dot(const JobHeader *h, int rfd, int wfd) {
    int n = h->n;                // function for calculate the eigenvalues , so the worker will calculate the dot product between a rows and the vector 
                                 // so we will get the size of the matrix then get for them a memory as we discripe before
    double *row = (double*)malloc((size_t)n * sizeof(double));
                                 // we will read a single row from the parent and a vector also from the parent they will should both of them have the same size 
    double *vec = (double*)malloc((size_t)n * sizeof(double));
    read_all(rfd, row, (size_t)n * sizeof(double));
                                 // then we will do a dot product between the row and the vector with the eqation (vec[k]*row[k])sum (from k =0 to n-1) = s  and the result is the one double number
    read_all(rfd, vec, (size_t)n * sizeof(double));
    double s = 0.0;
    
    for (int k=0;k<n;k++) s += row[k]*vec[k];
    ResultHeader rh = { .cmd = h->cmd, .job_id=h->job_id, .i=h->i, .j=0, .rows=1, .cols=1, .payload_bytes=(int)sizeof(double) };
                                  // put the resuld on the header to prepare it to send it by a pipe to the parent , then send it with a write all function , we will send the header and the result s to the parent , then clear the memory
    write_all(wfd, &rh, sizeof(rh));
    write_all(wfd, &s, sizeof(double));
    free(row); free(vec);
}





static void worker_loop(int read_fd, int write_fd) {
                                    // Ignore SIGINT in workers; parent handles shutdown
    signal(SIGINT, SIG_IGN);
    for (;;) {                      // worker loop is the a main function that all the child will work on it
        JobHeader h;                // it will get an pipe number that the child read and write the inforamtion though it from the parent
        ssize_t r = read_exact(read_fd, &h, sizeof(h));
        if (r == 0) break;          // we will get in the infint loop and before that we will we will set an signal and told him to ignore any intrupt from it
        if (r != sizeof(h)) break;  // so in the first we will get the diffine the header , then we will read from the pipe that the parend is send to us what is the type of the work CMD and read everything aslo
        switch (h.cmd) {            
            case CMD_ADD_ELEM:      handle_add(&h, read_fd, write_fd); break;
            case CMD_SUB_ELEM:      handle_sub(&h, read_fd, write_fd); break;
            case CMD_MUL_CELL:      handle_mul_cell(&h, read_fd, write_fd); break;
            case CMD_DET_ROW_ELIM:  handle_det_row_elim(&h, read_fd, write_fd); break;
            case CMD_EIG_ROW_DOT:   handle_eig_row_dot(&h, read_fd, write_fd); break;
            case CMD_QUIT:          return;
            default:                return;
        }
    }
}
