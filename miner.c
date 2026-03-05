#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "miner.h"

typedef struct {
    long int ini_sol;
    long int fin_sol;
    Bool completed;
} Info;

typedef struct {
    int n_round;
    pid_t pid;
    Status stat;
    long int target_ini;
    long int sol;
    Bool exit;
} Bloq;

int main(int argc, char *argv[]) {
    int pipe_info[2], pipe_res[2];
    int n_rounds, n_threads;
    long int target_ini;
    pid_t pid;

    if(argc != 4) {
        printf("Usage: ./miner <TARGET_INI> <ROUNDS> <N_THREADS>\n");
        return 1;
    }

    target_ini = (long int)atol(argv[1]);
    n_rounds = atol(argv[2]);
    n_threads = atol(argv[3]);

    if(pipe(pipe_info) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if(pipe(pipe_res) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if(pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if(pid == 0) {

    } else {
        exec_miner(n_rounds, n_threads, target_ini, &pipe_info, &pipe_res);
    }
}

void exec_miner(int n_rounds, int n_threads, long int sol_ini, int *pipe_info, int *pipe_res) {
    Info *data = NULL;
    Bloq data_bloq;
    pthread_t *threads = NULL;
    long int incr, j, k;
    Bool cont;
    int i, l, error;

    if(n_rounds <= 0 || n_rounds <= 0 || sol_ini < 0) {
        perror("input");
        exit(EXIT_FAILURE);
    }

    incr = POW_LIMIT / n_threads;

    if(!(data = (Info *)malloc(sizeof(Info)))) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    data->completed = FALSE;
    data->ini_sol = sol_ini;
    data->fin_sol = -1;

    if(!(threads = (pthread_t *)malloc(sizeof(pthread_t) * n_threads))) {
        perror("malloc");
        free(data);
        exit(EXIT_FAILURE);
    }

    close(pipe_info[0]);
    close(pipe_res[1]);

    for(l = 0; l < n_rounds; l++) {
        for(i = 0, j = 0, k = incr; i < n_threads; i++, j += incr, k += incr) {
            error = pthread_Create(&threads[i], NULL, &solucionar, j, k, data);
            if(error != 0) {
                fprintf(stderr, "pthread_Create: %s\n", strerror(error));
                exit(EXIT_FAILURE);
            }
        }

        for(i = 0; i < n_threads; i++) {
            error = pthread_join(threads[i], NULL);
            if(error != 0) {
                fprintf(stderr, "pthread_join: %s\n", strerror(error));
                exit(EXIT_FAILURE);
            }
        }

        data_bloq.exit = FALSE;
        data_bloq.n_round = l;
        data_bloq.pid = getpid();
        data_bloq.sol = data->fin_sol;
        data_bloq.target_ini = data->ini_sol;
        
        if(write(pipe_info[1], &data_bloq, sizeof(data_bloq)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        data->completed = FALSE;
        data->ini_sol = data->fin_sol;
        data->fin_sol = -1;
        
        if(read(pipe_res[0], cont, sizeof(cont)) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if(cont == FALSE) {
            break;
        }
    }

    data_bloq.exit = TRUE;
    data_bloq.n_round = 0;
    data_bloq.pid = getpid();
    data_bloq.sol = 0;
    data_bloq.target_ini = 0;

    if(write(pipe_info[1], &data_bloq, sizeof(data_bloq) + 1) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    free(data);
    free(threads);
    close(pipe_info[1]);
    close(pipe_res[0]);
    exit(EXIT_SUCCESS);
}

void solucionar(long int min, long int max, Info *data) {
    long int i;

    for(i = min; i <= max; i++) {
        if(data->completed == TRUE) {
            return;
        } else if(pow_hash(i) == data->ini_sol) {
            data->completed = TRUE;
            data->fin_sol = i;
            return;
        }
    }
}

void exec_register(int *pipe_info, int *pipe_res) {
    char *tok = NULL;
    Bloq data_bloq;
    int fichero, nbytes;
    Bool cont;

    if((fichero = open("log.txt", O_CREAT | O_TRUNC | O_WRONLY, 
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    close(pipe_info[1]);
    close(pipe_res[0]);

    while((nbytes = read(pipe_info[0], data_bloq, sizeof(data_bloq))) == sizeof(data_bloq)) {
        dprintf("Id:       %d\n"
                "Winner:   %d\n"
                "Target:   %ld\n"
                "Solution: %ld\n"
                "Votes:    %d/%d\n"
                "Wallets:  %d:%d",
                data_bloq->n_round,
                (int)data_bloq->pid,
                data_bloq->target_ini,
                data_bloq->sol,
                data_bloq->n_round,
                data_bloq->n_round,
                (int)data_bloq->pid,
                data_bloq->n_round);

        cont = TRUE;
        if(write(pipe_res[1], cont, sizeof(cont)) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
    }

    if(nbytes == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }


}