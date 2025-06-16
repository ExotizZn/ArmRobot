#ifndef TOAST_H
#define TOAST_H

#include <SDL2/SDL.h>

#include "config.h"

typedef struct Toast {
	char texte[256];
	Uint32 debut_ticks;
	Uint32 duree_ms;
	SDL_Color couleur;
	int rebondi_fait;
} Toast;

int toast_actif(const Toast *toast);
void afficher_toast(Toast *toast, const char *msg, SDL_Color couleur);

#endif