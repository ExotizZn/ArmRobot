#ifndef ETOILES_H
#define ETOILES_H

#include <SDL2/SDL.h>

#include "config.h"

typedef struct Etoile {
	float x, y, speed, rayon, twinkle;
	float phase;
} Etoile;

void init_etoiles(Etoile *tab, int n, int W, int H);
void update_etoiles(Etoile *tab, int n, int W, int H, float dt);
void dessiner_etoiles(SDL_Renderer *rendu, Etoile *tab, int n, int W, int H, float temps);

#endif