#include <signal.h> // sigaction
#include <stdio.h>  // fprintf
#include <stdlib.h> // exit
#include <unistd.h> // pipes
#include <wait.h>   // wait
#include <time.h> // time

#include "simulator.h"

int fd_pipe_spaceships[N_SPACESHIPS][2]; // Communicate with spaceships
                                         // processes

static status init_shared_resources(int team);
static void free_resources();
static void handler_SIGTERM(int signal);
static unsigned int rand_interval(unsigned int min, unsigned int max);

int main(int argc, char* argv[])
{
    int i, j;
    int team; // Team of this leader
    pid_t pid;
    char id_spaceship[MAX_CHAR_ID];
    command_t cmd;
    unsigned int r; // Rand number to choose between attack and move

    if (argc != 2) {
        fprintf(stderr, "[LEADER] Wrong number of arguments...\n");
        exit(EXIT_FAILURE);
    }

    team = atoi(argv[1]); // identifier of the leader team

    // Init resources
    if (!init_shared_resources(team)) {
        free_resources();
        exit(EXIT_FAILURE);
    }

    // Spacships spawns
    for (i = 0; i < N_SPACESHIPS; i++) {
        pid = fork();
        if (pid < 0) {
            perror("[LEADER]: fork");
            free_resources();
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            // Child process
            close(fd_pipe_spaceships[i][WRITE]);
            dup2(fd_pipe_spaceships[i][READ], STDIN_FILENO);
            close(fd_pipe_spaceships[i][READ]);
            sprintf(id_spaceship, "%d", i);
            execl("./spaceship", "spaceship", argv[1], id_spaceship, NULL);
            perror("[LEADER]: execl");
            exit(EXIT_FAILURE);
        }
    }

    srandom(time(NULL));
    while (1) {
        fprintf(stdout,
                "[LEADER %d] Reading the next command from the simulator...\n",
                team);
        read(STDIN_FILENO, &cmd, sizeof(command_t));
        switch (cmd.type) {
            case TURN:
                for (i = 0; i < N_ACTIONS_LEADER; i++) {
                    // Calculate a random action
                    r = rand_interval(0, 1);
                    switch (r) {
                        case ATTACK:
                            cmd.type = ATTACK;
                            for (j = 0; j < N_SPACESHIPS; j++) {
                                fprintf(stdout,
                                        "[LEADER %d] Sending ATTACK command to "
                                        "spaceship with id %d...\n",
                                        team,
                                        j);
                                write(fd_pipe_spaceships[j][WRITE],
                                      &cmd,
                                      sizeof(command_t));
                            }
                            break;
                        case MOVE:
                            cmd.type = MOVE;
                            for (j = 0; j < N_SPACESHIPS; j++) {
                                fprintf(stdout,
                                        "[LEADER %d] Sending MOVE command to "
                                        "spaceship with id %d...\n",
                                        team,
                                        j);
                                write(fd_pipe_spaceships[j][WRITE],
                                      &cmd,
                                      sizeof(command_t));
                            }
                    }
                }
                break;
            case DESTROY:
                write(fd_pipe_spaceships[cmd.id_spaceship][WRITE],
                      &cmd,
                      sizeof(command_t));
                break;
        }
        // Check if is the end
        if (cmd.type == END) {
            break;
        }
    }

    free_resources();

    exit(EXIT_SUCCESS);
}

static status init_shared_resources(int team)
{
    int i;
    int status;
    struct sigaction act;

    // Pipes
    fprintf(stdout, "[LEADER] Managing pipes...\n");
    for (i = 0; i < N_SPACESHIPS; i++) {
        status = pipe(fd_pipe_spaceships[i]);
        if (status == -1) {
            perror("[LEADER] Error creating the pipes...\n");
            return ERROR;
        }
    }

    // Signals
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = handler_SIGTERM;
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("[LEADER] Error establishing the handler for SIGTERM...\n");
        return ERROR;
    }

    // Ignore SIGINT because we used it for the main process
    act.sa_handler = SIG_IGN;
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("[LEADER] Error establishing the handler for SIGINT...\n");
        return ERROR;
    }

    // Ignore SIGALRM because we used it for the turn
    act.sa_handler = SIG_IGN;
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("[LEADER] Error establishing the handler for SIGALRM...\n");
        return ERROR;
    }

    return OK;
}

static void free_resources()
{
    int i;

    kill(0, SIGTERM);

    for (i = 0; i < N_SPACESHIPS; i++) {
        wait(NULL);
        close(fd_pipe_spaceships[i][WRITE]);
        close(fd_pipe_spaceships[i][READ]);
    }
}

static void handler_SIGTERM(int signal)
{
    free_resources();
    exit(EXIT_SUCCESS);
}

static unsigned int rand_interval(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    do {
        r = random();
    } while (r >= limit);

    return min + (r / buckets);
}
