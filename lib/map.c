#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include "map.h"

void map_clean_square(map_t *map, int posy, int posx)
{
	map->squares[posy][posx].team=-1;
	map->squares[posy][posx].id_spaceship=-1;
	map->squares[posy][posx].symbol=SYMB_EMPTY;
}

square_t map_get_square(map_t *map, int posy, int posx)
{
	return map->squares[posy][posx];
}

int map_get_distance(map_t *map, int oriy,int orix,int targety,int targetx)
{
	int distx,disty;

	distx=abs(targetx - orix);
	disty=abs(targety - oriy);
	return (distx > disty)? distx:disty;
}

spaceship_t map_get_spaceship(map_t *map, int team, int id_spaceship)
{
	return map->spaceships[team][id_spaceship];
}

int map_get_num_spaceships(map_t *map, int team)
{
	return map->n_spaceships_alive[team];
}

char map_get_symbol(map_t *map, int posy, int posx)
{
	return map->squares[posy][posx].symbol;
}

bool map_is_square_empty(map_t *map, int posy, int posx)
{
	return (map->squares[posy][posx].team < 0);
}

void map_restore(map_t *map)
{
	int i,j;

	for(j=0;j<MAP_MAX_X;j++) {
		for(i=0;i<MAP_MAX_Y;i++) {
			square_t cas = map_get_square(map,j, i);
			if (cas.team < 0) {
				map_set_symbol(map,j, i, SYMB_EMPTY);
			}
			else {
				map_set_symbol(map, j, i,team_symbols[cas.team]);
			}
		}
	}
}

void map_set_symbol(map_t *map, int posy, int posx, char symbol)
{
	map->squares[posy][posx].symbol=symbol;
}


int map_set_spaceship(map_t *map, spaceship_t spaceship)
{
	if (spaceship.team >= N_TEAMS) return -1;
	if (spaceship.id >= N_SPACESHIPS) return -1;
	map->spaceships[spaceship.team][spaceship.id]=spaceship;
	if (spaceship.alive) {
		map->squares[spaceship.posy][spaceship.posx].team=spaceship.team;
		map->squares[spaceship.posy][spaceship.posx].id_spaceship=spaceship.id;
		map->squares[spaceship.posy][spaceship.posx].symbol=team_symbols[spaceship.team];
	}
	else {
		map_clean_square(map,spaceship.posy, spaceship.posx);
	}
	return 0;
}

void map_set_num_spaceships(map_t *map, int team, int num_spaceships)
{
	map->n_spaceships_alive[team]=num_spaceships;
}

void map_send_missil(map_t *map, int originy, int originx, int targety, int targetx)
{
	int px=originx;
	int py=originy;
	int tx=targetx;
	int ty=targety;
	char ps = map_get_symbol(map,py, px);
	int nextx, nexty;
	char nexts;

	int run = tx - originx;
	int rise = ty - originy;
	float m = ((float) rise) / ((float) run);
	float b = originy - (m * originx);
	int inc = (originx < tx)? 1:-1;

	for (nextx = originx; (( originx < tx) && (nextx <= tx)) || (( originx > tx) && (nextx >= tx)); nextx+=inc)
	{
		// solve for y
		float y = (m * nextx) + b;

		// round to nearest int
		nexty = (y > 0.0) ? floor(y + 0.5) : ceil(y - 0.5);

		if ((nexty < 0) || (nexty >= MAP_MAX_Y)) { 
			continue;
		}
		nexts = map_get_symbol(map,nexty, nextx);
		map_set_symbol(map,nexty,nextx,'*');
		map_set_symbol(map,py,px,ps);
		usleep(50000);
		px = nextx;
		py= nexty;
		ps = nexts;
	}

	map_set_symbol(map,py, px,ps);
}
