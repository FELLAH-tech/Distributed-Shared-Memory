#include "dsm.h"
#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Vous pouvez ecrire ici toutes les fonctions */
/* qui pourraient etre utilisees par le lanceur */
/* et le processus intermediaire. N'oubliez pas */
/* de declarer le prototype de ces nouvelles */
/* fonctions dans common_impl.h */

int socket_listen_and_bind(int Nb_proc, ushort* port) {
	int listen_fd = -1;
	if (-1 == (listen_fd = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("Socket");
		exit(EXIT_FAILURE);
	}
	printf("Listen socket descriptor %d\n", listen_fd);

	int yes = 1;
	if (-1 == setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	struct addrinfo indices;
	memset(&indices, 0, sizeof(struct addrinfo));
	indices.ai_family = AF_INET;
	indices.ai_socktype = SOCK_STREAM; //TCP
	indices.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	struct addrinfo *res, *tmp;


	int err = 0;
	if (0 != (err = getaddrinfo("0.0.0.0",NULL, &indices, &res))) {
		errx(1, "%s", gai_strerror(err));
	}

	tmp = res;
	while (tmp != NULL) {
		if (tmp->ai_family == AF_INET) {
			
			if (-1 == bind(listen_fd, tmp->ai_addr, tmp->ai_addrlen)) {
				perror("Binding");
			}
			if (-1 == listen(listen_fd, Nb_proc)) {
				perror("Listen");
			}
            struct sockaddr_in sin;
            socklen_t len = sizeof(sin);
            if (getsockname(listen_fd, (struct sockaddr *)&sin, &len) == -1)
                perror("getsockname");

			*port = ntohs(sin.sin_port);
			freeaddrinfo(res);
			return listen_fd;
		}
		tmp = tmp->ai_next;
		
	}
	freeaddrinfo(res);
	return listen_fd;
}

int socket_and_connect(char *hostname, char *port) {
	int sock_fd = -1;
	// Création de la socket
	if (-1 == (sock_fd = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("Socket");
		exit(EXIT_FAILURE);
	}
	struct addrinfo hints, *res, *tmp;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;
	int error;
	error = getaddrinfo(hostname, port, &hints, &res);
	if (error) {
		errx(1, "%s", gai_strerror(error));
		exit(EXIT_FAILURE);
	}
	tmp = res;
	while (tmp != NULL) {
		if (tmp->ai_addr->sa_family == AF_INET) {

			char try_again = 1;
			while (try_again){
				try_again = 0;
				if (-1 == connect(sock_fd, tmp->ai_addr, tmp->ai_addrlen)) {
					try_again = 1;
				}
			}
			freeaddrinfo(res);
			return sock_fd;
		}
		tmp = tmp->ai_next;
	}
	freeaddrinfo(res);
	return -1;
}

/* obtenir le nom de la machine par son rang*/
void rank2hostname(dsm_proc_conn_t tab_struct[],int rank,int numb_proc,char *hostname){
	
	for (int j=0; j<numb_proc; j++){
		if(rank == tab_struct[j].rank){
			strcpy(hostname,tab_struct[j].machine);
			return;
		}
	}

	fprintf(stderr, "rank2hostname : No such rank = %d.\n", rank);
	exit(EXIT_FAILURE);

}

/* obtenir le port par son rang*/
void rank2port(dsm_proc_conn_t tab_struct[],int rank,int numb_proc,char* port_str){

	for (int j=0; j<numb_proc; j++){
		if(rank == tab_struct[j].rank){
			sprintf(port_str,"%hu",tab_struct[j].port_num);
			return;
		}
	}

	fprintf(stderr, "rank2port : No such rank = %d.\n", rank);
	exit(EXIT_FAILURE);

}

void display_connect_info(dsm_proc_conn_t *tab, int tab_size){

	for (int j=0; j<tab_size; j++){

		printf("address : %s:%hu, rank : %d, fd : %d, fd_for_exit : %d\n", tab[j].machine, tab[j].port_num, tab[j].rank, tab[j].fd, tab[j].fd_for_exit);
		 
	}

}

/* obtenir l'indice par son rang*/
int conn_info_get_index_by_rank(int rank){

	for (int j=0; j<DSM_NODE_NUM; j++){
		if(rank == proc_conn_info[j].rank){
			return j;
		}
	}
	fprintf(stderr, "conn_info_get_index_by_rank: No such rank = %d.\n", rank);
	exit(EXIT_FAILURE);
}

/* obtenir l'indice par son rang*/
int conn_info_get_index_by_fd(int fd){

	for (int j=0; j<DSM_NODE_NUM; j++){
		if(fd == proc_conn_info[j].fd){
			return j;
		}
	}
	fprintf(stderr, "conn_info_get_index_by_fd : No such fd = %d.\n", fd);
	exit(EXIT_FAILURE);
}