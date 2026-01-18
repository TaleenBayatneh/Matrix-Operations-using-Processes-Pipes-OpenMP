#ifndef COMMON_H
#define COMMON_H        // common.h is the group of the the laprary we used in each file code rather that we write all the #include in eaach file this file will be the group of them 
#include <stdio.h>      // it will make our project more readable and more error handuler        
#include <stdlib.h>     // stander library for malloc, free , die , xmalloc 
#include <stdint.h>     // give us a fixed size for the integer we use it when we need to read and write a binary data from pipe
#include <string.h>
#include <unistd.h>     // posix unix , give us fork, pipe , write , read , the main library to conect the parent with the chiled
#include <errno.h>      // it have the varible errno that save the last error in the system, it used after the read or write or any faild system
#include <time.h>       // get timer.c
#include <math.h>
#include <fcntl.h>      // used to file control, or change the control of the pipe
#include <signal.h>     // for signal
#include <sys/types.h>  // give us some varible like the id of the process  
#include <sys/wait.h>   // for ending the procses like wait and waitpid after the child finsh the parent will call it to end the procsess
#include <dirent.h>     // for reviewing files
#include <poll.h>       // for monitor each pipe we use it in pool when the system wait for the child to get any information, here the child will know that he finsh without drop the system
#define MAX_NAME 32


static inline void die( const char *msg)
                            // samll helper function for ending the program if there is a problem that can not continue like , problem with open file , so good end with the resion
{                           // or problem with the pipe or fork, or any case that need to end the program and know what is the problem , get the massege and print it with the errno that save the last error happen in the system , and perror, change this number of error to a text
    perror(msg);   

    exit(EXIT_FAILURE);
}


static inline void *xmalloc(size_t n)
 {                         // a function that like malloc but it is extened of it ,  will also cheack the problem 
    void *p = malloc(n);   // size_t is the size of the byte we need store it in the memory , it will give you a pointer to the place of store of your data , if p == null falid if not it will return the place that store the data

    if (!p) die("malloc");

    return p;
}


static inline ssize_t write_all(int fd, const void *buf, size_t count) 

{ 
    size_t off = 0;                    // function that will write all data from the buffer to the pipe , but it write thim all and it can wait of there is a signal come and interapt somthing
                                       // the fd is the number of the pipe that we need to write on it file desciptor 
    const char *p = (const char *)buf; // the off is the counter to how many byte we write for now then we change the buffer to a char since we will count p + off 

    while (off < count)                // we need to write the hole byte so the counter will start as off is less that the acuall size
    {
        ssize_t w = write(fd, p + off, count - off);
        if (w < 0)                     // using write , write information of he place we are on it p+ off, the size , is the rest of the size we take from it count - off
         {
            if (errno == EINTR) continue;  // if the write give us a -1 we need to check if the problem from the signal (EINTR) we will return to try , if not 
            return -1;
        }
        if (w == 0) break;             // if there is nothing been writen in the write , so the problem so just break

        off += (size_t)w;              // put the number of byte we got it in the off so that we know who byte are not there and just return it
    }
    return (ssize_t)off;
}



static inline ssize_t read_all(int fd, void *buf, size_t count)
 {                                     // the read all function that do the same as the write but in read way and it is the same alogthim
    size_t off = 0;           

    char *p = (char *)buf;
    while (off < count) 
    {
        ssize_t r = read(fd, p + off, count - off);
                                    // as we said the fd is the number of the id of the pipe that open in the system in the file descriptor 
        if (r < 0)                  // so if we make an pipe between perant and chile assume int p2c[2] pipe(p2c) the 0 for the read and the 1 for the write , so the chiled will read from his pipe and the fd is 0
         {                          // if we will write the buffer already have the inforamtion , but in the read the buffer will will have , and the p+off here if we read 40 byte , then signal hit us we will continuo from this byte 41 , count- off , the rest of the all size
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;

        off += (size_t)r;
    }
    return (ssize_t)off;
}

extern int g_omp_enabled;          // extern mean that this varible are in another place bu i need to use it < so the difinition are in anther place 
                                   // so the g_opm_enable is a golbal varible that will make the openMP on or off , and they are in the common so we can use it in any file then 
#endif                    
