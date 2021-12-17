#include "dsmexec_utils.h"
#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common_impl.h"

#include <poll.h>
#include <unistd.h>

#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>


/* variables globales */

/* un tableau gerant les infos d'identification */
/* des processus dsm */
dsm_proc_t *proc_array = NULL; 

/* le nombre de processus effectivement crees */
volatile int num_procs_creat = 0;

void usage(void)
{
	fprintf(stdout,"Usage : dsmexec machine_file executable arg1 arg2 ...\n");
	fflush(stdout);
	exit(EXIT_FAILURE);
}

void sigchld_handler(int sig)
{
	/* on traite les fils qui se terminent */
	/* pour eviter les zombies */
	wait(NULL);
}

/*******************************************************/
/*********** ATTENTION : BIEN LIRE LA STRUCTURE DU *****/
/*********** MAIN AFIN DE NE PAS AVOIR A REFAIRE *******/
/*********** PLUS TARD LE MEME TRAVAIL DEUX FOIS *******/
/*******************************************************/

int main(int argc, char *argv[])
{

	if (argc < 3) {
		usage();
		exit(EXIT_SUCCESS);
	}

	pid_t pid; // pour les forks
	
	int i; // pour les boucles for

	char buff[MAX_STR];
    unsigned short port;

	int proc_index; // dans proc_array
	int proc_fd;
	unsigned short proc_port;
	pid_t proc_pid; // pid dans la machine locale
	
	/* Mise en place d'un traitant pour recuperer les fils zombies*/
	/*
	struct sigaction sigchild_action;
	memset(&sigchild_action,0,sizeof(struct sigaction));
	sigchild_action.sa_handler = sigchld_handler;
	*/


	//sigaction(SIGCHLD,&sigchild_action,NULL);

	/*
	struct sigaction act = {{0}};
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE,&act,NULL);
	*/


	/* lecture du fichier de machines */
	/* 1- on recupere le nombre de processus a lancer */
	/* 2- on recupere les noms des machines : le nom de */
	/* la machine est un des elements d'identification */
	
	num_procs_creat = read_machine_names(argv[1], &proc_array);
	
	/* creation de la socket d'ecoute */
	/* + ecoute effective */ 

	int listen_fd = -1;
	if (-1 == (listen_fd = socket_listen_and_bind(num_procs_creat, &port))) {
		printf("Could not create, bind and listen properly\n");
		return 1;
	}
	// dsmexec conn info
	char dsmexec_hostname[20];
	if ( -1 == (gethostname(dsmexec_hostname, 20))){
      perror("gethostname");
   	}

	printf("dsmexec_conn_info : %s:%hu\n", dsmexec_hostname, port);

	// pipe definition

	int tab_stdout_pipe[2];
	int tab_stderr_pipe[2];

	/* creation des fils */
	for(i = 0; i < num_procs_creat ; i++) {

		/* creation du tube pour rediriger stdout */
		pipe(tab_stdout_pipe);
		proc_array[i].stdout_fd = tab_stdout_pipe[0];
		
		/* creation du tube pour rediriger stderr */
		pipe(tab_stderr_pipe);
		proc_array[i].stderr_fd = tab_stderr_pipe[0];

		// rank
		proc_array[i].connect_info.rank = i;

		pid = fork();
		if(pid == -1) ERROR_EXIT("fork");
		
		if (pid == 0) { /* fils */	
			
			/* redirection stdout */
			dup2(tab_stdout_pipe[1], STDOUT_FILENO);
			
			/* redirection stderr */	      	      
			dup2(tab_stderr_pipe[1], STDERR_FILENO);


			/* Creation du tableau d'arguments pour le ssh */ 

			/*
				ssh -qt hostname "bash ..."
			*/


			//char *newargv[20] = {"ssh", proc_array[i].connect_info.machine, "~/DSM_bin/dsmwrap", dsmexec_hostname, dsmexec_port_str, pid_str, truc};
			int newargc = 4+argc;
			char **newargv = malloc((newargc+1) * sizeof(char *));

			newargv[0] = malloc(4 * sizeof(char));
			strcpy(newargv[0], "ssh");

			newargv[1] = malloc(MAX_STR * sizeof(char));
			strcpy(newargv[1], proc_array[i].connect_info.machine);

			newargv[2] = malloc(MAX_STR * sizeof(char));
			sprintf(newargv[2], "dsmwrap");

			newargv[3] = malloc(MAX_STR * sizeof(char));
			strcpy(newargv[3], dsmexec_hostname);

			newargv[4] = malloc(MAX_STR * sizeof(char));
			sprintf(newargv[4], "%hu", port);

			newargv[5] = malloc(MAX_STR * sizeof(char));
			sprintf(newargv[5], "%d", getpid());

			for(int j = 2; j<argc; j++){
				newargv[j+4] = malloc(MAX_STR * sizeof(char));
				strcpy(newargv[j+4], argv[j]);
			}

			newargv[newargc] = NULL;

			/* jump to new prog : */
			execvp("ssh", newargv);

		} else  if(pid > 0) { /* pere */

			proc_array[i].pid = pid;
			proc_array[i].connect_info.rank = i;

			close(tab_stdout_pipe[1]);
			close(tab_stderr_pipe[1]);

		}
	}

	printf("---------\nAccepting\n---------\n");

	for(i = 0; i < num_procs_creat ; i++){

		/* on accepte les connexions des processus dsm */
		struct sockaddr_in csin;
		socklen_t size = sizeof(csin);
		if (-1 == (proc_fd = accept(listen_fd, (struct sockaddr *)&csin, &size))) {
			perror("Accept");
		}

		/*  On recupere le nom de la machine distante */	
		/* 1- puis la chaine elle-meme */
    	memset(buff,0,MAX_STR);
		if (recv(proc_fd, buff, MAX_STR, MSG_NOSIGNAL) <= 0) {
			ERROR_EXIT("revc");
		}


		/* On recupere le pid du processus distant  (optionnel)*/
        if (recv(proc_fd, &proc_pid, sizeof(pid_t), MSG_NOSIGNAL) <= 0) {
			ERROR_EXIT("revc");
		}

		/* On recupere le numero de port de la socket */
		/* d'ecoute des processus distants */
        /* cf code de dsmwrap.c */
        if (recv(proc_fd, &proc_port, sizeof(unsigned short), MSG_NOSIGNAL) <= 0) {
			ERROR_EXIT("revc");
		}

		printf("%s:%hu(pid=%d) was accepted\n", buff, proc_port, proc_pid);

		proc_index = procs_array_get_index_by_machine_and_pid(proc_array, num_procs_creat, buff, proc_pid);

		proc_array[proc_index].connect_info.fd = proc_fd;
		proc_array[proc_index].connect_info.port_num = proc_port;

    }
	
	
	/***********************************************************/ 
	/********** ATTENTION : LE PROTOCOLE D'ECHANGE *************/
	/********** DECRIT CI-DESSOUS NE DOIT PAS ETRE *************/
	/********** MODIFIE, NI DEPLACE DANS LE CODE   *************/
	/***********************************************************/
	
	for(i = 0; i < num_procs_creat ; i++){

	/* 1- envoi du nombre de processus aux processus dsm*/
	/* On envoie cette information sous la forme d'un ENTIER */
	/* (IE PAS UNE CHAINE DE CARACTERES */
		int nb_proc = num_procs_creat;
		if (send(proc_array[i].connect_info.fd, &nb_proc, sizeof(int), MSG_NOSIGNAL) <= 0) {
		  if (errno != EPIPE)  
		    ERROR_EXIT("send");
		}		
	


	
	/*2 On envoie cette information sous la forme d'un ENTIER */
	/* (IE PAS UNE CHAINE DE CARACTERES */
		int nb_rank = proc_array[i].connect_info.rank;
		if (send(proc_array[i].connect_info.fd, &nb_rank, sizeof(int), MSG_NOSIGNAL) <= 0) {
		  if (errno != EPIPE)  
		    ERROR_EXIT("send");
		}
	
	/* 3- envoi des infos de connexion aux processus */
	/* Chaque processus distant doit recevoir un nombre de */
	/* structures de type dsm_proc_conn_t égal au nombre TOTAL de */
	/* processus distants, ce qui signifie qu'un processus */
	/* distant recevra ses propres infos de connexion */
	/* (qu'il n'utilisera pas, nous sommes bien d'accords). */
		dsm_proc_conn_t tab_conn[num_procs_creat];
		for (int j =0;j < num_procs_creat;j++){
			memcpy(&(tab_conn[j]),&(proc_array[j].connect_info),sizeof(dsm_proc_conn_t)); 
		}
		if (send(proc_array[i].connect_info.fd, tab_conn, num_procs_creat*sizeof(dsm_proc_conn_t), MSG_NOSIGNAL) <= 0) {
		  if (errno != EPIPE)  		    
		    ERROR_EXIT("send");
		}
	}

	/***********************************************************/
	/********** FIN DU PROTOCOLE D'ECHANGE DES DONNEES *********/
	/********** ENTRE DSMEXEC ET LES PROCESSUS DISTANTS ********/
	/***********************************************************/

	
	/* gestion des E/S : on recupere les caracteres */
	/* sur les tubes de redirection de stdout/stderr */

	printf("---------\npipes I/O\n---------\n");

	struct pollfd pollfds[2*num_procs_creat];
	char deconnected[2*num_procs_creat];
	int nbr_running_procs = 2*num_procs_creat;
	
    for(int i = 0; i < num_procs_creat; i++){

		//stdout
        pollfds[2*i].fd = proc_array[i].stdout_fd;
        pollfds[2*i].events = POLLIN;
        pollfds[2*i].revents = 0;
		deconnected[2*i] = 0;

		// stderr
		pollfds[2*i+1].fd = proc_array[i].stderr_fd;
        pollfds[2*i+1].events = POLLIN;
        pollfds[2*i+1].revents = 0;
		deconnected[2*i+1] = 0;

    }

	char buffer[PRINTF_MAX_BUFFER];

    while(nbr_running_procs>0){
        poll(pollfds, 2*num_procs_creat, 1000);
        for(int i = 0; i < num_procs_creat; i++){
			//stdout
            if(pollfds[2*i].revents & POLLIN){
                read_from_pipe(pollfds[2*i].fd, buffer);
				printf(
					"[Proc %d\t: %s\t: stdout] %s\n",
					proc_array[i].connect_info.rank,
					proc_array[i].connect_info.machine,
					buffer
				);
                pollfds[2*i].revents = 0;
            }

			if(!deconnected[2*i] && pollfds[2*i].revents & POLLHUP){
				printf(
					"[Proc %d\t: %s\t: stdout] Deconnected\n",
					proc_array[i].connect_info.rank,
					proc_array[i].connect_info.machine
				);
                pollfds[2*i].revents = 0;
				deconnected[2*i] = 1;
				nbr_running_procs--;
            }

			//stderr
            if(pollfds[2*i+1].revents & POLLIN){
                read_from_pipe(pollfds[2*i+1].fd, buffer);
				printf(
					"[Proc %d\t: %s\t: stderr] %s\n",
					proc_array[i].connect_info.rank,
					proc_array[i].connect_info.machine,
					buffer
				);
                pollfds[2*i+1].revents = 0;
            }

			if(!deconnected[2*i+1] && pollfds[2*i+1].revents & POLLHUP){
				printf(
					"[Proc %d\t: %s\t: stderr] Deconnected\n",
					proc_array[i].connect_info.rank,
					proc_array[i].connect_info.machine
				);
                pollfds[2*i+1].revents = 0;
				deconnected[2*i+1] = 1;
				nbr_running_procs--;
            }
        }
    }

	/* on attend les processus fils */
	printf("---------\nWaiting children\n---------\n");
	for(int i = 0; i < num_procs_creat; i++){
		wait(NULL);
	}
	printf("OK\n");
	
	printf("---------\nCleaning Memory\n---------\n");
	
	/* on ferme les descripteurs proprement */
	for(i = 0; i < num_procs_creat ; i++){
		close(proc_array[i].connect_info.fd);
		close(proc_array[i].stdout_fd);
		close(proc_array[i].stderr_fd);
	}


	/* on ferme la socket d'ecoute */
	close(listen_fd);

	free(proc_array);

	printf("OK\n");

	printf("---------\nEND\n---------\n");

	exit(EXIT_SUCCESS);
}
