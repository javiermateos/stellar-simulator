#include <errno.h>     // errno
#include <fcntl.h>     // O_* constants
#include <semaphore.h> // sem_open
#include <stdio.h>     // fprintf, perror
#include <stdlib.h>    // exit
#include <sys/mman.h>  // shm_open
#include <unistd.h> // usleep
#include <signal.h>    // sigaction

#include "gamescreen.h"
#include "simulator.h"
#include "map.h"

#define SCREEN_REFRESH 10000

int fd_shm_map;          // Shared memory with the map
map_t* pmap = NULL;      // pointer to the map
sem_t* sem_ready = NULL; // semapore for monitor process

static status init_shared_resources();
static void free_resources();
static void print_map(map_t* ptipo_mapa);
static void handler_SIGINT(int signal);

int main(int argc, char* argv[])
{
    // init resources
    if (!init_shared_resources()) {
        free_resources();
        exit(EXIT_FAILURE);
    }

    // Send the signal to the main process that monitor is ready
    sem_post(sem_ready);

    screen_init();

    // Main loop
    while (1) {
        // We dont need to control the access to map because in case that
        // the monitor shows wrong information about the map it will be 
        // refreshed in 0.01 secs.
        print_map(pmap);
        usleep(SCREEN_REFRESH);
    }

    exit(EXIT_SUCCESS);
}

static status init_shared_resources()
{
    struct sigaction act;

    // Semaphores
    sem_ready = sem_open(SEM_READY_NAME, O_RDWR);
    if (sem_ready == SEM_FAILED) {
        perror("[MONITOR] Error opening the semaphore sem_ready...\n");
        return ERROR;
    }

    // Shared memory
    fd_shm_map = shm_open(SHM_MAP_NAME, O_RDONLY, 0);
    if (fd_shm_map == -1) {
        perror("[MONITOR] Error opening the shared memory...\n");
        return ERROR;
    }

    pmap =
      (map_t*)mmap(NULL, sizeof(map_t), PROT_READ, MAP_SHARED, fd_shm_map, 0);
    if (pmap == MAP_FAILED) {
        perror("[MONITOR] Error mapping the shared memory...\n");
        return ERROR;
    }

    // Signals
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = handler_SIGINT;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("[SIMULATOR] Error establishing the handler for SIGINT...\n");
        return ERROR;
    }

    return OK;
}

static void free_resources()
{
    sem_close(sem_ready);
    munmap(pmap, sizeof(map_t));
}

static void print_map(map_t* ptipo_mapa)
{
    int i, j;

    for (j = 0; j < MAP_MAX_Y; j++) {
        for (i = 0; i < MAP_MAX_X; i++) {
            square_t square = map_get_square(ptipo_mapa, j, i);
            screen_addch(i, j, square.symbol);
        }
    }
    screen_refresh();
}

static void handler_SIGINT(int signal)
{
    free_resources();
    screen_end();
    exit(EXIT_SUCCESS);
}
