#include "dsmexec_utils.h"
#include "common_impl.h"

#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int read_machine_names(char *path, dsm_proc_t **dsm_procs){

    int err;
    int nbr_lines = 0;
    int fd = open(path, O_RDONLY | O_EXCL);
    if(fd == -1){
        ERROR_EXIT("read_machine_names open");
    }

    // Recuperer nbr_lines
    char cp;
    char c = '\n';
    do{
        cp = c;
        err = read(fd, (void *)&c, sizeof(char));
        if(err == 0){
            break;
        }
        if(c == '\n'&& cp != '\n')
            nbr_lines++;
    }while(1);

    *dsm_procs = malloc(nbr_lines * sizeof(dsm_proc_t));
    if(*dsm_procs == NULL){
        ERROR_EXIT("read_machine_names malloc");
    }

    lseek(fd, 0, SEEK_SET);

    int machine = 0;
    int index_in_name=0;
    c = '\0';
    do{
        cp = c;
        err = read(fd, (void *)&c, sizeof(char));
        if(err == 0){
            break;
        }
        
        if(c == '\n' && cp != '\n'){
            (*dsm_procs)[machine].connect_info.machine[index_in_name] = '\0';
            machine++;
            index_in_name = 0;
        }else if(c != '\n'){
            (*dsm_procs)[machine].connect_info.machine[index_in_name] = c;
            index_in_name++;
        }

    }while(1);

    close(fd);

    return nbr_lines;

}

int procs_array_get_index_by_machine_and_pid(dsm_proc_t *proc_array, int proc_nbr, char *machine_name, int pid){
    
    for(int i = 0; i<proc_nbr; i++){
        if(strcmp(proc_array[i].connect_info.machine, machine_name) == 0 && proc_array[i].pid == pid){
            return i;
        }
    }

    fprintf(stderr, "procs_array_get_index_by_machine_and_pid : proc not found.\n");
    exit(EXIT_FAILURE);

}

void read_from_pipe(int pipe_fd, char *buffer){
    char c;
    int err;
    int buffer_index = 0;

    err = read(pipe_fd, &c, 1);
    while(buffer_index < PRINTF_MAX_BUFFER-1 && err == 1 && c != '\n'){

        buffer[buffer_index++] = c;

        err = read(pipe_fd, &c, 1);
    }

    buffer[buffer_index] = '\0';
}