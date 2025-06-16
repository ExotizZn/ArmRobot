#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>

#include "../include/toast.h"
#include "../include/config.h"

int toast_actif(const Toast *toast) {
	return toast->texte[0] && (SDL_GetTicks() - toast->debut_ticks < toast->duree_ms);
}

// Toast functions
void afficher_toast(Toast *toast, const char *msg, SDL_Color couleur) {
	strncpy(toast->texte, msg, 255);
	toast->texte[255] = 0;
	toast->debut_ticks = SDL_GetTicks();
	toast->duree_ms = 2700;
	toast->couleur = couleur;
	toast->rebondi_fait = 0;
}