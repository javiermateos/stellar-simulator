#ifndef SRC_map_H_
#define SRC_map_H_

#include <stdbool.h>

#include <simulator.h> // spaceship_t

square_t map_get_square(map_t* map, int posy, int posx);

int map_get_distance(map_t* map, int oriy, int orix, int targety, int targetx);

spaceship_t map_get_spaceship(map_t* map, int team, int id_spaceship);

int map_get_num_spaceships(map_t* map, int team);

char map_get_symbol(map_t* map, int posy, int posx);

bool map_is_square_empty(map_t* map, int posy, int posx);

bool square_is_empty(map_t* map, square_t square);

void map_clean_square(map_t* map, int posy, int posx);

void map_restore(map_t* map);

void map_send_missil(map_t* map,
                     int origeny,
                     int origenx,
                     int targety,
                     int targetx);

int map_set_spaceship(map_t* map, spaceship_t spaceship);

void map_set_num_spaceships(map_t* map, int team, int numspaceships);

void map_set_symbol(map_t* map, int posy, int posx, char symbol);

#endif /* SRC_map_H_ */
