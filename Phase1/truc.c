#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common_impl.h"
#include <sys/socket.h>

int main(int argc, char *argv[])
{
   int fd;
   int i;
   char str[1024];
   char exec_path[2048];   
   char *wd_ptr = NULL;
   wd_ptr = getcwd(str,1024);

   if (wd_ptr == NULL){
     perror("getcwd");
   }
   fprintf(stdout,"Working dir is %s\n",str);
   
   fprintf(stdout,"Number of args : %i\n", argc);
   for(i= 0; i < argc ; i++)
     fprintf(stderr,"arg[%i] : %s\n",i,argv[i]);
    
   sprintf(exec_path,"%s/%s",str,"titi");	      
   fd = open(exec_path,O_RDONLY);
   if(fd == -1) perror("open");
   fprintf(stdout,"================ Valeur du descripteur : %i\n",fd);

   int dsmexec_fd = atoi(getenv("DSMEXEC_FD"));
   int master_fd = atoi(getenv("MASTER_FD"));

   fprintf(stdout,"==> dsmexec_fd : %i\n==> master_fd : %i\n",dsmexec_fd,master_fd);	

   /* 1- recevoir du nombre de processus */
	/* On reçoie cette information sous la forme d'un ENTIER */
	/* (IE PAS UNE CHAINE DE CARACTERES */
		int nb_proc;
		if (recv(dsmexec_fd, &nb_proc, sizeof(int), 0) <= 0) {
			ERROR_EXIT("send");
		}		

	/* 2- recevoir des rangs */

		int nb_rank;
		if (recv(dsmexec_fd, &nb_rank, sizeof(int), 0) <= 0) { // le sock_fd est le dernier argument
			ERROR_EXIT("send");
		}
	
  fprintf(stdout,"==> Nombre de processus : %i\n==> Nombre rang : %i\n",nb_proc,nb_rank);	
	/* 3- recevoir des infos de connexion  */
  
    dsm_proc_conn_t tab_struct[nb_proc];
    memset(tab_struct,0,nb_proc*sizeof(dsm_proc_conn_t));
    if (recv(dsmexec_fd, tab_struct, nb_proc*sizeof(dsm_proc_conn_t), 0) <= 0) {
			ERROR_EXIT("send");
    }

    for(int j = 0; j < nb_proc ; j++){
      fprintf(stdout,"==> fd : %i\n==> fd_exit : %i\n==> Nom machine : %s\n==> Nombre port : %i\n==> Nombre rang : %i\n",tab_struct[j].fd,tab_struct[j].fd_for_exit,tab_struct[j].machine,tab_struct[j].port_num,tab_struct[j].rank);
    }
  
   return 0;
}