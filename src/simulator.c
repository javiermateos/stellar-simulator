#include <errno.h>     // errno
#include <fcntl.h>     // O_* constants
#include <mqueue.h>    // mq_open
#include <semaphore.h> // sem_open
#include <stdio.h>     // fprintf
#include <stdlib.h>    // exit
#include <sys/mman.h>  // shm_open
#include <unistd.h>    // fork, alarm, execl, write

#include "simulator.h"

#define MAX_CHAR_ID_LEADER 12 // Max chars for an integer

int fd_pipe_leader[N_TEAMS][2]; // Used to communicate with leader processes
int fd_shm_map;                 // Shared memory with the map
map_t* pmap;                    // pointer to the map
mqd_t queue;                    // message queue with spaceships
sem_t* sem_count_w = NULL;      // counting writers
sem_t* sem_count_r = NULL;      // counting readers
sem_t* sem_mutex_r = NULL;      // mutex readers
sem_t* sem_w = NULL;            // semaphore for writers
sem_t* sem_r = NULL;            // semapthore for readers
sem_t* sem_ready = NULL;        // semapore for monitor process

static status init_shared_resources();
static status init_map();

int main(int argc, char* argv[])
{
    int i;
    pid_t pid;
    char id_leader[MAX_CHAR_ID_LEADER];
    int sval;

    // init resources
    fprintf(stdout, "[SIMULATOR] Initializing shared resources...\n");
    if (!init_shared_resources()) {
        exit(EXIT_FAILURE);
    }

    // init map
    printf("[SIMULATOR] Initializing the map...\n");
    if (!init_map()) {
        exit(EXIT_FAILURE);
    }

    // leader spawns
    for (i = 0; i < N_TEAMS; i++) {
        pid = fork();
        if (pid < 0) {
            perror("[SIMULATOR]: fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            close(fd_pipe_leader[i][WRITE]);
            dup2(fd_pipe_leader[i][READ], STDIN_FILENO);
            close(fd_pipe_leader[i][READ]);
            sprintf(id_leader, "%d", i);
            execl("./leader", "leader", id_leader, NULL);
            perror("[SIMULATOR]: execl");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            close(fd_pipe_leader[i][READ]);
        }
    }

    // Simulation start
    if (sem_getvalue(sem_ready, &sval) == 0) {
        fprintf(stdout, "[SIMULATOR] Waiting for the monitor process...\n");
    }
    sem_wait(sem_ready);
    fprintf(stdout, "[SIMULATOR] Start of battle...\n");
    while (1) {
        fprintf(stdout, "[SIMULATOR] New turn...\n");
        alarm(TURN_DURATION);
        // Send the command to leaders processes
        /* for (i = 0; i < N_TEAMS; i++) { */
        /*     write(leader_pipe, TURN, sizeof(TURN)); */
        /* } */

        // Read messages sent by the spaceships from the queue

        // Upload the map
    }

    // wait for the leaders processes finish

    exit(EXIT_SUCCESS);
}

static status init_shared_resources()
{
    int i;
    int status;
    struct sigaction act;
    struct mq_attr attributes;

    // Shared memory
    fprintf(stdout, "[SIMULATOR] Managing shared memory...\n");
    fd_shm_map =
      shm_open(SHM_MAP_NAME, O_CREAT | O_EXCL | O_RDWR, S_IWUSR | S_IRUSR);
    if (fd_shm_map == -1) {
        perror("[SIMULATOR] Error creating the shared memory...\n");
        return ERROR;
    }

    if (ftruncate(fd_shm_map, sizeof(map_t)) == -1) {
        perror("[SIMULATOR] Error sizing the shared memory...\n");
        return ERROR;
    }

    pmap = (map_t*)mmap(
      NULL, sizeof(map_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm_map, 0);
    if (pmap == MAP_FAILED) {
        perror("[SIMULATOR] Error mapping the shared memory...\n");
        return ERROR;
    }

    // Pipes
    printf("[SIMULATOR] Managing pipes...\n");
    for (i = 0; i < N_TEAMS; i++) {
        status = pipe(fd_pipe_leader[i]);
        if (status == -1) {
            perror("[SIMULATOR] Error creating the pipes...\n");
            return ERROR;
        }
    }

    // Queue
    attributes.mq_flags = 0;
    attributes.mq_maxmsg = 10;
    attributes.mq_curmsgs = 0;
    attributes.mq_msgsize = sizeof(move_t);

    printf("[SIMULADOR] Managing message queue...\n");
    queue = mq_open(MQ_ACTION_NAME,
                    O_CREAT | O_EXCL | O_RDONLY,
                    S_IWUSR | S_IRUSR,
                    &attributes);
    if (queue == (mqd_t)-1) {
        perror("[SIMULATOR] Error creating the queue...\n");
        return ERROR;
    }

    // Semaphores
    sem_ready =
      sem_open(SEM_READY_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (sem_ready == SEM_FAILED) {
        perror("[SIMULATOR] Error creating the semaphore sem_ready...\n");
        return ERROR;
    }
    
    sem_count_w = sem_open(SEM_COUNT_W_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (sem_count_w == SEM_FAILED) {
        perror("[SIMULATOR] Error creating the semaphore sem_count_w...\n");
        return ERROR;
    }

    if ((sem_contlect =
           sem_open(SEM_CONT_LECT, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) ==
        SEM_FAILED) {
        printf("Error abriendo el semaforo en el programa simulador...\n");
        return ERROR;
    }

    if ((sem_mutexlect =
           sem_open(SEM_MUTEX_LECT, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED) {
        printf("Error abriendo el semaforo en el programa simulador...\n");
        return ERROR;
    }

    if ((sem_esem = sem_open(
           SEM_ESEM, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
        printf("Error abriendo el semaforo en el programa simulador...\n");
        return ERROR;
    }

    if ((sem_lsem = sem_open(
           SEM_LSEM, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
        printf("Error abriendo el semaforo en el programa simulador...\n");
        return ERROR;
    }

    /*Señales*/
    printf("Simulador: gestionando seniales...\n");
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGALRM;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        printf("Error estableciendo el manejador de señal de SIGALRM...\n");
        return ERROR;
    }

    sigfillset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGINT;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        printf("Error estableciendo el manejador de señal de SIGALRM...\n");
        return ERROR;
    }

    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        printf("Error estableciendo el manejador de señal de SIGTERM..\n");
        return ERROR;
    }

    return OK;
}
