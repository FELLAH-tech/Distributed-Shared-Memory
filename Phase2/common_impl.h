#ifndef __COMMON_IMPL_H
#define __COMMON_IMPL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

/* autres includes (eventuellement) */

#define ERROR_EXIT(str) {perror(str);exit(EXIT_FAILURE);}

/**************************************************************/
/****************** DEBUT DE PARTIE NON MODIFIABLE ************/
/**************************************************************/

#define MAX_STR  (1024)
typedef char maxstr_t[MAX_STR];

#define PRINTF_MAX_BUFFER 10*1024

/* definition du type des infos */
/* de connexion des processus dsm */
struct dsm_proc_conn  {
   int      rank;
   maxstr_t machine;
   int      port_num;
   int      fd;
   int      fd_for_exit; /* special */  
};

typedef struct dsm_proc_conn dsm_proc_conn_t; 

/**************************************************************/
/******************* FIN DE PARTIE NON MODIFIABLE *************/
/**************************************************************/


/* definition du type des infos */
/* d'identification des processus dsm */
struct dsm_proc {
  pid_t pid;
  dsm_proc_conn_t connect_info;
  int stdout_fd; // to read from
  int stderr_fd; // to read from
};
typedef struct dsm_proc dsm_proc_t;

/*
Param:
    Nombre de processus.
returns : Create listening socket
*/
int socket_listen_and_bind(int Nb_proc, ushort* port);

int socket_and_connect(char *hostname, char *port);

/* obtenir le nom de la machine par son rang*/
void rank2hostname(dsm_proc_conn_t tab_struct[],int rank,int numb_proc,char *hostname);

/* obtenir le port par son rang*/
void rank2port(dsm_proc_conn_t tab_struct[],int rank,int numb_proc,char* port_str);

void display_connect_info(dsm_proc_conn_t *tab, int tab_size);

int conn_info_get_index_by_rank(int rank);

int conn_info_get_index_by_fd(int fd);

#endif