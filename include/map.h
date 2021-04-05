#ifndef SRC_map_H_
#define SRC_map_H_

#include <stdbool.h>

#include <simulator.h> // spaceship_t

char team_symbols[N_TEAMS] ={'A','B','C'};

// Pone una square del map a vacío
int map_clean_square(map_t *map, int posy, int posx);

// Obtiene información de una square del map
square_t map_get_square(map_t *map, int posy, int posx);

//Obtiene la distancia entre dos posiciones del map
int map_get_distancia(map_t *map, int oriy,int orix,int targety,int targetx);

//Obtiene información sobre una nave a partir del team y el número de nave
spaceship_t map_get_nave(map_t *map, int team, int id_spaceship);

// Obtiene el número de naves vivas en un team
int map_get_num_naves(map_t *map, int team);

// Obtiene el símbolo de la posición y,x en el map
char map_get_symbol(map_t *map, int posy, int posx);

// Chequea si la square y,x está vacía en el map
bool map_is_square_empty(map_t *map, int posy, int posx);

// Chequea si el contenido de square corresponde a una square empty
bool square_is_empty(map_t *map, square_t square);

// Restaura los símbolos del map dejando sólo las naves vivas
void map_restore(map_t *map);

// Genera la animación de un misil en el map
void map_send_misil(map_t *map, int origeny, int origenx, int targety, int targetx);

// Fija el contenido de "nave" en el map, en la posición nave.posy, nave.posx
int map_set_nave(map_t *map, spaceship_t nave);

// Fija el número de naves 'numNaves' para el team
void map_set_num_naves(map_t *map, int team, int numNaves);

// Fija el símbolo 'symbol' en la posición posy, posx del map
void map_set_symbol(map_t *map, int posy, int posx, char symbol);


#endif /* SRC_map_H_ */
