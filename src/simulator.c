#include <errno.h>     // errno
#include <fcntl.h>     // O_* constants
#include <mqueue.h>    // mq_open
#include <semaphore.h> // sem_open
#include <signal.h>    // sigaction
#include <stdio.h>     // fprintf
#include <stdlib.h>    // exit
#include <sys/mman.h>  // shm_open
#include <time.h>      // time
#include <unistd.h>    // fork, alarm, execl, write
#include <wait.h>      // wait

#include "map.h"
#include "simulator.h"


extern char team_symbols[N_TEAMS];

int flag_SIGALRM;
int fd_pipe_leader[N_TEAMS][2]; // Used to communicate with leader processes
int fd_shm_map;                 // Shared memory with the map
map_t* pmap;                    // pointer to the map
mqd_t queue;                    // message queue with spaceships
sem_t* sem_w = NULL;            // semaphore for writers
sem_t* sem_ready = NULL;        // semapore for monitor process

static status init_shared_resources();
static void init_map();
static void handler_SIGALRM(int signal);
static void handler_SIGINT(int signal);
static void free_resources();
static void process_move(move_t move);

int main(int argc, char* argv[])
{
    int i;
    pid_t pid;
    char id_leader[MAX_CHAR_ID];
    int sval;
    command_t cmd; // Commands sent to leaders
    move_t move;   // Moves sent by spaceships
    int teams_alive;
    int winner;

    // init resources
    fprintf(stdout, "[SIMULATOR] Initializing shared resources...\n");
    if (!init_shared_resources()) {
        free_resources();
        exit(EXIT_FAILURE);
    }

    // init map
    printf("[SIMULATOR] Initializing the map...\n");
    init_map();

    // leader spawns
    for (i = 0; i < N_TEAMS; i++) {
        pid = fork();
        if (pid < 0) {
            perror("[SIMULATOR]: fork");
            free_resources();
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
        cmd.type = TURN;
        for (i = 0; i < N_TEAMS; i++) {
            write(fd_pipe_leader[i][WRITE], &cmd, sizeof(command_t));
        }
        flag_SIGALRM = 0;
        while (!flag_SIGALRM) {
            // Read messages sent by the spaceships from the queue
            fprintf(stdout, "[SIMULATOR] Listening to the message queue...\n");
            if (mq_receive(queue, (char*)&move, sizeof(move), NULL) == -1) {
                if (errno == EINTR) {
                    // We should restart the loop to check flag_SIGALRM
                    continue;
                } else {
                    perror("[SIMULATOR] mq_receive");
                }
            }
            fprintf(stdout, "[SIMULATOR] Message received in the queue...\n");

            // Process the move sent by the spaceship
            sem_wait(sem_w);

            process_move(move);

            sem_post(sem_w);
        }
        // Upload the map
        sem_wait(sem_w);

        map_restore(pmap);

        sem_post(sem_w);

        // Check if there is a winner
        for (i = 0, teams_alive = 0; i < N_TEAMS; i++) {
            if (map_get_num_spaceships(pmap, i)) {
                teams_alive++;
                winner = i;
            }
        }

        if (teams_alive == 1) {
            fprintf(
              stdout, " [SIMULATOR] Winner: %c...\n", team_symbols[winner]);
            cmd.type = END;
            for (i = 0; i < N_TEAMS; i++) {
                write(fd_pipe_leader[i][WRITE], &cmd, sizeof(command_t));
            }
            break;
        }
    }

    free_resources();

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
    fprintf(stdout, "[SIMULATOR] Managing pipes...\n");
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

    fprintf(stdout, "[SIMULADOR] Managing message queue...\n");
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

    sem_w = sem_open(SEM_W_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    if (sem_w == SEM_FAILED) {
        perror("[SIMULATOR] Error creating the semaphore sem_w...\n");
        return ERROR;
    }

    // Signals
    fprintf(stdout, "[SIMULADOR] Managing signals...\n");
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = handler_SIGALRM;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("[SIMULATOR] Error establishing the handler for SIGALRM...\n");
        return ERROR;
    }

    sigfillset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = handler_SIGINT;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("[SIMULATOR] Error establishing the handler for SIGINT...\n");
        return ERROR;
    }

    // Ignore SIGTERM because we used it for childs
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("[SIMULATOR] Error establishing the handler for SIGTERM...\n");
        return ERROR;
    }

    return OK;
}

static void handler_SIGALRM(int signal)
{
    flag_SIGALRM = 1;
}

static void handler_SIGINT(int signal)
{
    free_resources();
    exit(EXIT_SUCCESS);
}

static void free_resources()
{
    int i;

    kill(0, SIGTERM);

    for (i = 0; i < N_TEAMS; i++) {
        wait(NULL);
        close(fd_pipe_leader[i][WRITE]);
        close(fd_pipe_leader[i][READ]);
    }

    mq_close(queue);
    mq_unlink(MQ_ACTION_NAME);
    sem_unlink(SEM_COUNT_W_NAME);
    sem_unlink(SEM_COUNT_R_NAME);
    sem_unlink(SEM_MUTEX_R_NAME);
    sem_unlink(SEM_W_NAME);
    sem_unlink(SEM_R_NAME);
    sem_unlink(SEM_READY_NAME);
    munmap(pmap, sizeof(map_t));
    shm_unlink(SHM_MAP_NAME);
}

static void init_map()
{
    int i, j;
    spaceship_t spaceship;
    int posx, posy;

    srand(time(NULL));

    // Clean the map
    for (i = 0; i < MAP_MAX_X; i++) {
        for (j = 0; j < MAP_MAX_Y; j++) {
            map_clean_square(pmap, j, i);
        }
    }

    // Set up the spaceships in the map
    for (i = 0; i < N_TEAMS; i++) {
        map_set_num_spaceships(pmap, i, N_SPACESHIPS);
        for (j = 0; j < N_SPACESHIPS; j++) {
            spaceship.health = MAX_LIFE_SPACESHIPS;
            spaceship.team = i;
            spaceship.id = j;
            spaceship.alive = true;
            // Search for an empty square
            while (1) {
                posx = rand() % (MAP_MAX_X);
                posy = rand() % (MAP_MAX_Y);
                if (map_is_square_empty(pmap, posx, posy)) {
                    break;
                }
            }
            spaceship.posx = posx;
            spaceship.posy = posy;
            map_set_spaceship(pmap, spaceship);
        }
    }
}

static void process_move(move_t move)
{
    square_t src_square, dst_square;
    spaceship_t attacked_spaceship, spaceship;
    command_t cmd;

    src_square = map_get_square(pmap, move.originY, move.originX);
    dst_square = map_get_square(pmap, move.objetiveY, move.objetiveX);

    /*Mover*/
    switch (move.type) {
        case MOVE:
            // TODO: We need to check if the square is still empty
            fprintf(stdout,
                    "[SIMULATOR] ACTION MOVE [%c%d] %d,%d -> %d,%d...\n",
                    src_square.symbol,
                    src_square.id_spaceship,
                    move.originX,
                    move.originY,
                    move.objetiveX,
                    move.objetiveY);
            spaceship = map_get_spaceship(pmap, move.team, move.id_spaceship);
            spaceship.posx = move.objetiveX;
            spaceship.posy = move.objetiveY;
            map_set_spaceship(pmap, spaceship);
            break;
        case ATTACK:
            map_send_missil(pmap, move.originY, move.originX, move.objetiveY, move.objetiveX);
            if (map_is_square_empty(pmap, move.objetiveY, move.objetiveX)) {
                printf("[SIMULATOR] ACTION ATTACK [%c%d] %d,%d -> %d,%d: "
                       "FAILED: Objective square empty...\n",
                       src_square.symbol,
                       src_square.id_spaceship,
                       move.originX,
                       move.originY,
                       move.objetiveX,
                       move.objetiveY);
                map_set_symbol(
                  pmap, move.objetiveY, move.objetiveX, SYMB_WATER);
            } else {
                attacked_spaceship = map_get_spaceship(
                  pmap, dst_square.team, dst_square.id_spaceship);
                attacked_spaceship.health =
                  attacked_spaceship.health - ATACK_DAMAGE;
                if (attacked_spaceship.health <= 0) {
                    printf(
                      "[SIMULATOR] ACTION ATTACK [%c%d] %d,%d -> %d,%d: target "
                      "destroyed...\n",
                      src_square.symbol,
                      src_square.id_spaceship,
                      move.originX,
                      move.originY,
                      move.objetiveX,
                      move.objetiveY);
                    map_set_symbol(
                      pmap, move.objetiveY, move.objetiveY, SYMB_DESTROYED);
                    cmd.type = DESTROY;
                    cmd.id_spaceship = attacked_spaceship.id;
                    write(fd_pipe_leader[attacked_spaceship.team][WRITE],
                          &cmd,
                          sizeof(command_t));
                    map_set_num_spaceships(
                      pmap,
                      attacked_spaceship.team,
                      map_get_num_spaceships(pmap, attacked_spaceship.team) -
                        1);
                    attacked_spaceship.alive = false;
                    attacked_spaceship.health = 0;
                } else {
                    printf(
                      "[SIMULATOR] ACTION ATTACK [%c%d] %d,%d -> %d,%d: target "
                      "damaged with %d remaining health...\n",
                      src_square.symbol,
                      src_square.id_spaceship,
                      move.originX,
                      move.originY,
                      move.objetiveX,
                      move.objetiveY,
                      attacked_spaceship.health);
                    map_set_symbol(
                      pmap, move.objetiveY, move.objetiveX, SYMB_DAMAGED);
                }
                map_set_spaceship(pmap, attacked_spaceship);
            }
            break;
        default:
            // TODO: Think about the default case
            break;
    }
}
