#ifndef __DSMEXEC_UTILS_H__
#define __DSMEXEC_UTILS_H__

#include "common_impl.h"

/*
Param:
    char **buffer : allocated in this function free *buffer after use.

returns : number of files.
*/
int read_machine_names(char *path, dsm_proc_t **dsm_procs);

int procs_array_get_index_by_machine_and_pid(dsm_proc_t *proc_array, int proc_nbr, char *machine_name, int pid);

void read_from_pipe(int pipe_fd, char *buffer);


#endif