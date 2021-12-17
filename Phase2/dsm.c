#include "dsm.h"
#include "common_impl.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>




/* indique l'adresse de debut de la page de numero numpage */
static char *num2address( int numpage )
{
   char *pointer = (char *)(BASE_ADDR+(numpage*(PAGE_SIZE)));

   if( pointer >= (char *)TOP_ADDR ){
      fprintf(stderr,"[%i] Invalid address !\n", DSM_NODE_ID);
      return NULL;
   }
   else return pointer;
}

/* cette fonction permet de recuperer un numero de page */
/* a partir  d'une adresse  quelconque */
static int address2num( char *addr )
{
  return (((intptr_t)(addr - BASE_ADDR))/(PAGE_SIZE));
}

/* cette fonction permet de recuperer l'adresse d'une page */
/* a partir d'une adresse quelconque (dans la page)        */
/*
static char *address2pgaddr( char *addr )
{
  return  (char *)(((intptr_t) addr) & ~(PAGE_SIZE-1));
}
*/

/* fonctions pouvant etre utiles */
static void dsm_change_info( int numpage, dsm_page_state_t state, dsm_page_owner_t owner)
{
   if ((numpage >= 0) && (numpage < PAGE_NUMBER)) {
	if (state != NO_CHANGE )
	table_page[numpage].status = state;
      if (owner >= 0 )
	table_page[numpage].owner = owner;
      return;
   }
   else {
	fprintf(stderr,"[%i] Invalid page number !\n", DSM_NODE_ID);
      return;
   }
}

static dsm_page_owner_t get_owner( int numpage)
{
   return table_page[numpage].owner;
}

/*
static dsm_page_state_t get_status( int numpage)
{
   return table_page[numpage].status;
}
*/

/* Allocation d'une nouvelle page */
static void dsm_alloc_page( int numpage )
{

   char *page_addr = num2address( numpage );

   int prot;

   if (table_page[numpage].status == READ_ONLY){
      prot = PROT_READ;
   }
   else if(table_page[numpage].status == WRITE){
      prot = PROT_READ | PROT_WRITE;
   }
   else{
      prot = PROT_NONE;
   }

   if ( MAP_FAILED == mmap(page_addr, PAGE_SIZE, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) ){
      ERROR_EXIT("mmap");
   }

   return ;
}

void dsm_send(int sckt, char *buff, size_t size,char*err_msg)
{
   /* a completer */
   int ret = 0, offset = 0;
   int max_attempts = 10000;
    int attempts=0;

	while (offset != size) {
		if (-1 == (ret = write(sckt, buff + offset, size - offset))) {
			perror(err_msg);
			exit(EXIT_FAILURE);
		}
      if(ret == 0){
         attempts++;
         if(attempts == max_attempts){
            fprintf(stderr, "dsm_send : Max attempts reached.\n");
            exit(EXIT_FAILURE);
         }
      }else{
         attempts = 0;
      }
		offset += ret;
	}
}

int dsm_recv(int sckt, char * buffer, size_t size_p,char*err_msg){

   //Receiving data
   int treated_size = 0;
   char *data_cursor = buffer;
   int err;
   int max_attempts = 10000;
   int attempts=0;

   while(treated_size < size_p){
      err = read(sckt, data_cursor, size_p-treated_size);
      if(err == -1){
         perror(err_msg);
         exit(EXIT_FAILURE);
      }
      treated_size+=err;
      data_cursor+=err;
      if(err == 0){
         attempts ++;
         if(attempts == max_attempts){
               buffer[treated_size] = '\0';
               //fprintf(stderr, "dsm_recv : Max attempts reached for %s. data(%d octes):%s\n",err_msg, treated_size, buffer);
               //exit(EXIT_FAILURE);
               return 0;

         }
      }else{
         attempts = 0;
      }
   }

   return 1;

}

/* Changement de la protection d'une page */
/*
static void dsm_protect_page( int numpage , int prot)
{
   char *page_addr = num2address( numpage );
   mprotect(page_addr, PAGE_SIZE, prot);
   return;
}
*/

static void dsm_free_page( int numpage )
{
   char *page_addr = num2address( numpage );
   munmap(page_addr, PAGE_SIZE);
   return;
}

static void *dsm_comm_daemon( void *arg)
{

   const int poll_timeout = 500;       // micro secondes
   const int receiving_timeout = 4;   // multiple of poll_timeout
   int receiving_time_counter = 0;     // multiple of poll_timeout

   int active_procs_but_me = DSM_NODE_NUM-1;

   struct pollfd pollfds[DSM_NODE_NUM-1];
   int i = 0;
  for (int j=0; j < DSM_NODE_NUM; j++){
      
      if(j != DSM_NODE_ID){
         
         pollfds[i].fd = proc_conn_info[j].fd;
         pollfds[i].events = POLLIN;
         pollfds[i].revents = 0;
         i++;
      }
   }

   dsm_msg_header_t header_msg_recv;
   dsm_msg_header_t header_msg_send;
   dsm_req_type_t page_info;

   while(1){

      if (-1 == poll(pollfds, DSM_NODE_NUM-1, poll_timeout)) {
			perror("Poll");
		}

      // Verifying receiving_timeout
      pthread_mutex_lock(&requested_page_mutex);
      receiving_time_counter++;
      if(requested_page >= 0 && receiving_time_counter == receiving_timeout){

         /*Try again*/
         receiving_time_counter = 0;
         pthread_mutex_unlock(&available_page);
      }
      pthread_mutex_unlock(&requested_page_mutex);

      // Daemon exit condition
      pthread_mutex_lock(&finalize_mutex);
      if(finalize && active_procs_but_me <= 0){
         return NULL;
      }
      pthread_mutex_unlock(&finalize_mutex);
      
      for (int j = 0; j < DSM_NODE_NUM-1; j++){


         if (pollfds[j].fd != -1 && pollfds[j].revents & POLLIN){

            /* réception du header */
            if(! dsm_recv(pollfds[j].fd,(char*) &header_msg_recv,sizeof(dsm_msg_header_t),"header_recv")){
               pollfds[j].revents = 0;
               continue;
            }

            switch (header_msg_recv.req_type){

               case DSM_REQ:

                  /*Verify if I am the owner of the page*/
                  if(table_page[header_msg_send.page_num].owner == DSM_NODE_ID){

                     /* envoi du header */
                     header_msg_send.page_num = header_msg_recv.page_num; // pour l'envoyer au deamon
                     header_msg_send.req_type = DSM_PAGE;
                     header_msg_send.taille = PAGE_SIZE;
                     dsm_send(pollfds[j].fd,(char*) &header_msg_send,sizeof(dsm_msg_header_t),"header_send");

                     /* envoi des informations de la page*/
                     dsm_send(pollfds[j].fd,(char*) &table_page[header_msg_recv.page_num],sizeof(dsm_page_info_t),"page_info");

                     /* envoi de la page*/
                     dsm_send(pollfds[j].fd,(char*)num2address(header_msg_send.page_num), PAGE_SIZE, "page_send");

                     /* Update table_page for me */
                     table_page[header_msg_send.page_num].owner = proc_conn_info[conn_info_get_index_by_fd(pollfds[j].fd)].rank;
                     
                     /* libérer la page */
                     dsm_free_page(header_msg_send.page_num);

                     /* Sending page info to all procs but me & the receiver */

                     header_msg_send.req_type = DSM_UPDATE;
                     header_msg_send.taille = 0;

                     for(int i = 0; i<DSM_NODE_NUM; i++){
                        if(proc_conn_info[i].rank != DSM_NODE_ID && proc_conn_info[i].rank != pollfds[j].fd){
                           dsm_send(proc_conn_info[i].fd, (char*) &header_msg_send, sizeof(dsm_msg_header_t), "update_page_status_header_send");
                           dsm_send(proc_conn_info[i].fd, (char*) &table_page[header_msg_send.page_num], sizeof(dsm_page_info_t), "update_page_status_send");
                        }
                     }

                  }

                  break;

               case DSM_PAGE:

                  /* réception des informations de la page*/
                  dsm_recv(pollfds[j].fd, (char*) &table_page[header_msg_recv.page_num], sizeof(dsm_page_info_t),"info_page");
                  
                  /* mis à jour des informations de la page*/
                  table_page[header_msg_recv.page_num].owner = DSM_NODE_ID;

                  /* Allocation de la page */
                  dsm_alloc_page(header_msg_recv.page_num);

                  /* réception de la page */
                  dsm_recv(pollfds[j].fd,(char*)num2address(header_msg_recv.page_num),PAGE_SIZE,"page_recv");

                  pthread_mutex_lock(&requested_page_mutex);

                  if(requested_page == header_msg_recv.page_num){
                     /*signaler la disponibilité de la page*/
                     receiving_time_counter = 0;
                     pthread_mutex_unlock(&available_page);
                  }else{
                     //fprintf(stderr, "I am receiving an other page instead of the requested one.\n");
                     break;
                     //exit(EXIT_FAILURE);
                  }

                  pthread_mutex_unlock(&requested_page_mutex);
                  

                  break;

               case DSM_UPDATE:

                  /* I'am obliged to read the message otherwise the sender will be blocked*/
                  dsm_recv(pollfds[j].fd, (char*) & page_info, sizeof(dsm_page_info_t), "update_info_page_receive");

                  /*
                     When it comes to my pages, I'm always right.
                     So I ignore the update message.
                  */
                  if(table_page[header_msg_recv.page_num].owner != DSM_NODE_ID){

                     memcpy(&table_page[header_msg_recv.page_num], &page_info, sizeof(dsm_page_info_t));

                     /*
                        Give an other chance to the main thread (re-request the page after a new seg fault)
                        if it hasn't requested the needed page from its right owner.
                     */
                     pthread_mutex_lock(&requested_page_mutex);

                     if(requested_page == header_msg_recv.page_num){
                        receiving_time_counter = 0;
                        pthread_mutex_unlock(&available_page);
                     }

                     pthread_mutex_unlock(&requested_page_mutex);
                  }

                  break;
               
               case DSM_FINALIZE:
                  active_procs_but_me--;
                  break;

               default:
                  break;
            }

         }else if(pollfds[j].revents !=0){
            pollfds[j].fd = -1;
            pollfds[j].events = 0;
            pollfds[j].revents = 0;
         }

         pollfds[j].revents = 0;

      }

   }

   return NULL;
}


static void dsm_handler(int page_num)
{

   /* A modifier */
   //printf("[%i] FAULTY  ACCESS !!! \n",DSM_NODE_ID);
   dsm_page_owner_t owner_rank = get_owner(page_num);

   if(owner_rank == DSM_NODE_ID){
      //Cannot request my own page
      return;
   }

   /*Set requested_page to the number of the page I am waiting for.*/
   pthread_mutex_lock(&requested_page_mutex);
   requested_page = page_num;
   pthread_mutex_unlock(&requested_page_mutex);

   /* send request type */
   dsm_msg_header_t header_msg;
   header_msg.page_num = page_num;
   header_msg.req_type = DSM_REQ;
   header_msg.taille = 0;

   int conn_info_i = conn_info_get_index_by_rank(owner_rank);
   
   dsm_send(proc_conn_info[conn_info_i].fd, (char *) &header_msg, sizeof(dsm_msg_header_t),"dsm_page_req");

   /*attente de la page*/
   pthread_mutex_lock(&available_page);

   /*I don't any page for the moment.*/
   pthread_mutex_lock(&requested_page_mutex);
   requested_page = -1;
   pthread_mutex_unlock(&requested_page_mutex);

}

/* traitant de signal adequat */
static void segv_handler(int sig, siginfo_t *info, void *context)
{
   /* A completer */
   /* adresse qui a provoque une erreur */
   void  *addr = info->si_addr;
  /* Si ceci ne fonctionne pas, utiliser a la place :*/
  /*
   #ifdef __x86_64__
   void *addr = (void *)(context->uc_mcontext.gregs[REG_CR2]);
   #elif __i386__
   void *addr = (void *)(context->uc_mcontext.cr2);
   #else
   void  addr = info->si_addr;
   #endif
   */
   /*
   pour plus tard (question ++):
   dsm_access_t access  = (((ucontext_t *)context)->uc_mcontext.gregs[REG_ERR] & 2) ? WRITE_ACCESS : READ_ACCESS;
  */
   /* adresse de la page dont fait partie l'adresse qui a provoque la faute */
   void  *page_addr  = (void *)(((unsigned long) addr) & ~(PAGE_SIZE-1));

   if ((addr >= (void *)BASE_ADDR) && (addr < (void *)TOP_ADDR))
     {
	dsm_handler(address2num(page_addr));
     }
   else
     {
	/* SIGSEGV normal : ne rien faire*/

  fprintf(stderr, "SEG FAULT at %p\n", addr);
  exit(EXIT_FAILURE);

     }
}

/* Seules ces deux dernieres fonctions sont visibles et utilisables */
/* dans les programmes utilisateurs de la DSM                       */
char *dsm_init(int argc, char *argv[])
{
   struct sigaction act;
   int index;

   /* Récupération de la valeur des variables d'environnement */
   /* DSMEXEC_FD et MASTER_FD                                 */
   int dsmexec_fd = atoi(getenv("DSMEXEC_FD"));
   int master_fd = atoi(getenv("MASTER_FD"));


   /* reception du nombre de processus dsm envoye */
   /* par le lanceur de programmes (DSM_NODE_NUM) */
	if (recv(dsmexec_fd, &DSM_NODE_NUM, sizeof(int), 0) <= 0) {
		ERROR_EXIT("send");
	}

   /* reception de mon numero de processus dsm envoye */
   /* par le lanceur de programmes (DSM_NODE_ID)      */
	if (recv(dsmexec_fd, &DSM_NODE_ID, sizeof(int), sizeof(int)) < 0) { // le sock_fd est le dernier argument
		ERROR_EXIT("DSM_NODE_ID. send");
	}

   /* reception des informations de connexion des autres */
   /* processus envoyees par le lanceur :                */
   /* nom de machine, numero de port, etc.               */
   proc_conn_info = malloc(DSM_NODE_NUM * sizeof(dsm_proc_conn_t));

   dsm_recv(dsmexec_fd, (char *) proc_conn_info, DSM_NODE_NUM*sizeof(dsm_proc_conn_t),"recv_conn_info");

   /*
   memset(proc_conn_info,0,DSM_NODE_NUM*sizeof(dsm_proc_conn_t));
   if (recv(dsmexec_fd, proc_conn_info, DSM_NODE_NUM*sizeof(dsm_proc_conn_t), 0) < DSM_NODE_NUM*sizeof(dsm_proc_conn_t)) {
		ERROR_EXIT("connect info. send");
   }
   */

   //display_connect_info(proc_conn_info, DSM_NODE_NUM);


   /* initialisation des connexions              */
   /* avec les autres processus : connect/accept */

   /* il faut éviter les doubles connect de la part de deux processus*/

   if (-1 == listen(master_fd, DSM_NODE_NUM)) {
      perror("Listen");
   }

   /* on accepte les connexions des autres processus dsm de rang inférieur */
   for(int j = 0; j < DSM_NODE_ID; j++){
      struct sockaddr_in csin;
      socklen_t size = sizeof(csin);
      int sock_fd = -1;
      if (-1 == (sock_fd = accept(master_fd, (struct sockaddr *)&csin, &size))) {
         ERROR_EXIT("Accept");
      }


      /* recevoir le rang */
      int rank;
      if (recv(sock_fd, &rank, sizeof(int), MSG_NOSIGNAL) <= 0) {
         ERROR_EXIT("recv");
      }

      proc_conn_info[conn_info_get_index_by_rank(rank)].fd = sock_fd;

   }

   /* on se connecte avec les autres processus dsm de rang supérieur*/
   for(int k = DSM_NODE_ID+1; k < DSM_NODE_NUM; k++){
      char hostname[MAX_STR];
      char port_str[MAX_STR];

      rank2hostname(proc_conn_info,k,DSM_NODE_NUM,hostname);
      rank2port(proc_conn_info,k,DSM_NODE_NUM,port_str);

      if (-1 == (proc_conn_info[k].fd = socket_and_connect(hostname,  port_str))){
         fprintf(stderr, "Could not create socket and connect properly\n");
         ERROR_EXIT("socket_and_connect");
      }
      /* Envoi du rang */
      if (send(proc_conn_info[k].fd, &DSM_NODE_ID, sizeof(int), MSG_NOSIGNAL) <= 0) {
         ERROR_EXIT("send");
      }

   }

   /*test*/

   // for (int j = 0; j < DSM_NODE_NUM;j++){
   //    fprintf(stdout,"RANK = %i\t SOCKET = %i\n",proc_conn_info[j].rank,proc_conn_info[j].fd);
   // }
   // fprintf(stdout,"\n");


   /* Allocation des pages en tourniquet */
   for(index = 0; index < PAGE_NUMBER; index ++){
     dsm_change_info( index, WRITE, index % DSM_NODE_NUM);
     if ((index % DSM_NODE_NUM) == DSM_NODE_ID)
       dsm_alloc_page(index);
     
   }

   /* initialisation des mutexes */

   pthread_mutex_init(&available_page, NULL);
   pthread_mutex_lock(&available_page);

   pthread_mutex_init(&finalize_mutex, NULL);
   finalize = 0;

   pthread_mutex_init(&requested_page_mutex, NULL);
   requested_page = -1;

   /* mise en place du traitant de SIGSEGV */
   act.sa_flags = SA_SIGINFO;
   act.sa_sigaction = segv_handler;
   sigaction(SIGSEGV, &act, NULL);

   /* creation du thread de communication           */
   /* ce thread va attendre et traiter les requetes */
   /* des autres processus                          */
   pthread_create(&comm_daemon, NULL, dsm_comm_daemon, NULL);

   /* Adresse de début de la zone de mémoire partagée */
   return ((char *)BASE_ADDR);
}

void dsm_finalize( void )
{

   fflush(stdout);
   fflush(stderr);

   /* fermer proprement les connexions avec les autres processus */

      /* Notifying other process that this process has finished*/
   dsm_msg_header_t header_msg;
   header_msg.page_num = -1;
   header_msg.req_type = DSM_FINALIZE;
   header_msg.taille = 0;

   for(int i = 0; i < DSM_NODE_NUM; i++){
      if(proc_conn_info[i].rank != DSM_NODE_ID){
         dsm_send(proc_conn_info[i].fd, (char *) &header_msg, sizeof(dsm_msg_header_t), "dsm_finalize");
      }
   }

      /* Notifying comme daemon that this process has finished*/
   pthread_mutex_lock(&finalize_mutex);
   finalize = 1;
   pthread_mutex_unlock(&finalize_mutex);

   /* terminer correctement le thread de communication */
   /* Waiting for other process */
   void *comm_daemon_return_val;
   pthread_join(comm_daemon, &comm_daemon_return_val);


   /*fermer les sockets entre les processus distants*/
   for(int j = 0; j < DSM_NODE_NUM; j++){

      /* Pour éviter de fermer n'importe quoi*/
      if (j != DSM_NODE_ID){
         close(proc_conn_info[j].fd);
      }
   }

   fflush(stdout);
   fflush(stderr);
   
   close(master_fd);
   close(dsmexec_fd);

   pthread_mutex_destroy(&available_page);
   pthread_mutex_destroy(&finalize_mutex);
   pthread_mutex_destroy(&requested_page_mutex);

   /* libérer les mémoires allouées */
   free(proc_conn_info);

  return;
}
