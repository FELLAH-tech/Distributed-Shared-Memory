#ifndef __DSM_H__
#define __DSM_H__

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include "common_impl.h"



/* fin des includes */

#define TOP_ADDR    (0x40000000)
#define PAGE_NUMBER (100)
#define PAGE_SIZE   (sysconf(_SC_PAGE_SIZE))
#define BASE_ADDR   (TOP_ADDR - (PAGE_NUMBER * PAGE_SIZE))

/* Not used */
typedef enum
{
   NO_ACCESS, 
   READ_ACCESS,
   WRITE_ACCESS, 
   UNKNOWN_ACCESS 
} dsm_page_access_t;

typedef enum
{   
   INVALID,
   READ_ONLY,
   WRITE,
   NO_CHANGE  
} dsm_page_state_t;

typedef int dsm_page_owner_t;

typedef struct
{ 
   dsm_page_state_t status;
   dsm_page_owner_t owner;
} dsm_page_info_t;

dsm_page_info_t table_page[PAGE_NUMBER];

typedef enum
{
   DSM_NO_TYPE = -1,
   DSM_REQ,
   DSM_PAGE,
   DSM_UPDATE,
   DSM_NREQ,
   DSM_FINALIZE
} dsm_req_type_t;

typedef struct
{
   int source;
   int page_num;
} dsm_req_t;

typedef struct
{
   size_t taille; // dépend de req_type
   dsm_req_type_t req_type;
   int page_num;
   
}dsm_msg_header_t;



pthread_t comm_daemon;

dsm_proc_conn_t *proc_conn_info;

int DSM_NODE_NUM; /* nombre de processus dsm */
int DSM_NODE_ID; /* rang (= numero) du processus */ 

int dsmexec_fd; /* socket entre dsmexec et ce processus distant */
int master_fd; /* socket d'écoute avec les autres processus distants */

pthread_mutex_t available_page;  // Can be locked when the page is received by comm deamon.

pthread_mutex_t finalize_mutex;
char finalize;

pthread_mutex_t requested_page_mutex;
int requested_page;


char *dsm_init( int argc, char *argv[]);
void  dsm_finalize( void );

#endif