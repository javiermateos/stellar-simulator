#include <fcntl.h>     // O_* constants
#include <mqueue.h>    // mq_open
#include <semaphore.h> // sem_open
#include <signal.h>    // sigaction
#include <stdio.h>     // fprintf
#include <stdlib.h>    // exit
#include <sys/mman.h>  // shm_open
#include <time.h>      // time
#include <unistd.h>

#include "map.h"
#include "simulator.h"

mqd_t queue;         // message queue with simulator
int fd_shm_map;      // Shared memory with the map
map_t* pmap = NULL;  // pointer to the map
sem_t* sem_w = NULL; // semaphore for writers
sem_t* sem_r = NULL; // semaphore for readers

static status init_shared_resources();
static void free_resources();
static void handler_SIGTERM();
static spaceship_t locate_enemy_spaceship(map_t* pmap, spaceship_t spaceship);
static unsigned int rand_interval(unsigned int min, unsigned int max);

int main(int argc, char* argv[])
{
    int team, id_spaceship;
    command_t cmd;
    spaceship_t spaceship, attacked_spaceship;
    move_t move;
    int posx, posy, x, y;

    if (argc != 3) {
        fprintf(stderr, "[SPACESHIP] Wrong number of arguments...\n");
        exit(EXIT_FAILURE);
    }

    team = atoi(argv[1]);
    id_spaceship = atoi(argv[2]);

    // Init resources
    if (!init_shared_resources()) {
        free_resources();
        exit(EXIT_FAILURE);
    }

    // Main loop
    srandom(time(NULL));
    while (1) {
        fprintf(stdout,
                "[SPACESHIP %d/%d] Reading message from PIPE...\n",
                team,
                id_spaceship);
        read(STDIN_FILENO, &cmd, sizeof(command_t));
        if (cmd.type == DESTROY) {
            break;
        }

        // Process the command receive from the team leader
        // TODO: Control the concurrent access to the map
        spaceship = map_get_spaceship(pmap, team, id_spaceship);
        switch (cmd.type) {
            case ATTACK:
                fprintf(stdout,
                        "[SPACESHIP %d/%d] Attacking...\n",
                        team,
                        id_spaceship);
                attacked_spaceship = locate_enemy_spaceship(pmap, spaceship);
                fprintf(
                        stdout,
                        "[SPACESHIP %d/%d] Sending ATTACK move to %d %d...\n",
                        team,
                        id_spaceship,
                        attacked_spaceship.team,
                        attacked_spaceship.id);
                move.objetiveX = attacked_spaceship.posx;
                move.objetiveY = attacked_spaceship.posy;
                break;
            case MOVE:
                fprintf(stdout,
                        "[SPACESHIP %d/%d] Looking for a position to move...\n",
                        team,
                        id_spaceship);
                // Calculate a random position to move
                while (1) {
                    posx = spaceship.posx;
                    posy = spaceship.posy;
                    x = rand() % 3;
                    y = rand() % 3;
                    if (x == 0) {
                        posx--;
                    } else if (x == 1) {
                        posx++;
                    }
                    if (y == 0) {
                        posy--;
                    } else if (y == 1) {
                        posy++;
                    }
                    if (x == 2 && y == 2) {
                        posx++;
                        posy++;
                    }
                    if (map_is_square_empty(pmap, posx, posy)) {
                        break;
                    }
                }
                fprintf(stdout,
                        "[SPACESHIP %d/%d] Sending MOVE move to %d %d...\n",
                        team,
                        id_spaceship,
                        posx,
                        posy);
                move.objetiveX = posx;
                move.objetiveY = posy;
                break;
        }
        // TODO: Control the concurrent access to shared resources

        // Send the move to the simulator process
        move.type = cmd.type;
        move.originX = spaceship.posx;
        move.originY = spaceship.posy;
        move.id_spaceship = id_spaceship;
        move.team = team;
        if (mq_send(queue, (char*)&move, sizeof(move), 1) == -1) {
            perror("[SPACESHIP] Error sending message through the queue...\n");
        }
    }

    free_resources();

    exit(EXIT_SUCCESS);
}

static status init_shared_resources()
{
    struct sigaction act;
    // Shared memory
    fd_shm_map = shm_open(SHM_MAP_NAME, O_RDONLY, 0);
    if (fd_shm_map == -1) {
        perror("[SPACESHIP] Error opening the shared memory...\n");
        return ERROR;
    }

    pmap =
      (map_t*)mmap(NULL, sizeof(map_t), PROT_READ, MAP_SHARED, fd_shm_map, 0);
    if (pmap == MAP_FAILED) {
        perror("[SPACESHIP] Error mapping the shared memory...\n");
        return ERROR;
    }

    // Queue
    fprintf(stdout, "[SPACESHIP] Managing message queue...\n");
    queue = mq_open(MQ_ACTION_NAME, O_WRONLY);
    if (queue == (mqd_t)-1) {
        perror("[SPACESHIP] Error creating the queue...\n");
        return ERROR;
    }

    // Semapthores
    sem_w = sem_open(SEM_W_NAME, O_RDWR);
    if (sem_w == SEM_FAILED) {
        perror("[SPACESHIP] Error opening the semaphore sem_w..\n");
        return ERROR;
    }

    sem_r = sem_open(SEM_R_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    if (sem_r == SEM_FAILED) {
        perror("[SPACESHIP] Error creating the semaphore sem_r...\n");
        return ERROR;
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
    // Note: We dont need to unlink because the parent process will do it
    mq_close(queue);
    sem_close(sem_w);
    sem_close(sem_r);
    munmap(pmap, sizeof(map_t));

    exit(EXIT_SUCCESS);
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

    do
    {
        r = random();
    } while (r >= limit);

    return min + (r / buckets);
}

static spaceship_t locate_enemy_spaceship(map_t* pmap, spaceship_t spaceship)
{
    int i;
    int enemy_team;
    spaceship_t attacked_spaceship;

    while(1) {
        // Select a random team
        enemy_team = rand_interval(0, N_TEAMS -1);
        if (enemy_team == spaceship.team || !map_get_num_spaceships(pmap, enemy_team)) {
            continue;
        }
        // Search for any reachable enemy spaceship
        for (i = 0; i < N_SPACESHIPS; i++) {
            attacked_spaceship = map_get_spaceship(pmap, enemy_team, i);
            if (!attacked_spaceship.alive) {
                continue;
            }
            if (map_get_distance(pmap, spaceship.posy, spaceship.posx, attacked_spaceship.posy, attacked_spaceship.posx) <= MAX_ATACK_SCOPE) {
                break;
            }
        }
    }

    return attacked_spaceship;
}
