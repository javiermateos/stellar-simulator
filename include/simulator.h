#ifndef SRC_SIMULADOR_H_
#define SRC_SIMULADOR_H_

#include <stdbool.h> // bool

#define N_TEAMS 3
#define N_SPACESHIPS 3

/*** SIMULATION ***/
#define MAX_LIFE_SPACESHIPS 50
#define MAX_ATACK_SCOPE 20
#define ATACK_DAMAGE 10
#define MOVE_RANGE 1
#define TURN_DURATION 5

/*** COMMANDS ***/
#define TURN 2
#define DESTROY 3
#define END 4
#define NO_CMD -1

/*** MOVES ***/
#define MOVE 0
#define ATTACK 1

/*** MAP ***/
#define MAP_MAX_X 20 // Number of columns of the map
#define MAP_MAX_Y 20 // Number of rows of the map
#define SYMB_EMPTY '.'
#define SYMB_DAMAGED '%'
#define SYMB_DESTROYED 'X'
#define SYMB_WATER 'w'

/*** NAMES OF SHARED RESOURCES ***/
#define SHM_MAP_NAME "/shm_map"
#define MQ_ACTION_NAME "/mq_actions"
#define SEM_COUNT_W_NAME "/sem_count_w"
#define SEM_COUNT_R_NAME "/sem_count_r"
#define SEM_MUTEX_R_NAME "/sem_mutex_r"
#define SEM_W_NAME "/sem_w"
#define SEM_R_NAME "/sem_r"
#define SEM_READY_NAME "/sem_ready"

#define N_ACTIONS_LEADER 2 // Number of leader actions in each turn
#define MAX_CHAR_ID 12 // Max chars for an identification integer

typedef struct {
    int health; // Remaining health
    int posx;   // map column
    int posy;   // map row
    int team;   // spaceship team
    int id;     // Ship number in the team
    bool alive; // Alive or dead
} spaceship_t;

typedef struct {
    char symbol;      // Symbol shown in the screen
    int team;         // Team that is in the square. If there is no team is -1.
    int id_spaceship; // Ship that is in the square.
} square_t;

typedef struct {
    spaceship_t spaceships[N_TEAMS]
                          [N_SPACESHIPS]; // Info about spaceships in the map
    square_t squares[MAP_MAX_Y][MAP_MAX_X];
    int n_spaceships_alive[N_TEAMS]; // Number of spaceships alive in each team
} map_t;

typedef struct {
    int type;
    int originX;
    int originY;
    int objetiveX;
    int objetiveY;
    int id_spaceship;
    int team;
} move_t;

typedef struct {
    int type;
    int id_spaceship;
} command_t;

enum { READ = 0, WRITE = 1 };

typedef enum { OK = 1, ERROR = 0} status;

#endif /* SRC_SIMULADOR_H_ */
