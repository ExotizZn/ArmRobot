#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>

#include "../include/etoiles.h"
#include "../include/config.h"

// Star field functions
void init_etoiles(Etoile *tab, int n, int W, int H) {
	for (int i = 0; i < n; ++i) {
		tab[i].x = random_values[i * 4] * W;
		tab[i].y = random_values[i * 4 + 1] * H;
		tab[i].speed = 0.09f + random_values[i * 4 + 2] / 2.6f;
		tab[i].rayon = 0.8f + random_values[i * 4 + 3] / 2.0f;
		tab[i].twinkle = 0.14f + random_values[i * 4 + 2] / 3.2f;
		tab[i].phase = (random_values[i * 4 + 3] * 628) / 100.0f;
	}
}

void update_etoiles(Etoile *tab, int n, int W, int H, float dt) {
	for (int i = 0; i < n; ++i) {
		Etoile *e = &tab[i];
		e->y += e->speed * dt * 500.0f;
		if (e->y > H) {
			e->y = 0;
			e->x = random_values[i * 4] * W;
			e->speed = 0.09f + random_values[i * 4 + 2] / 2.6f;
			e->rayon = 0.8f + random_values[i * 4 + 3] / 2.0f;
			e->twinkle = 0.14f + random_values[i * 4 + 2] / 3.2f;
			e->phase = (random_values[i * 4 + 3] * 628) / 100.0f;
		}
	}
}

void dessiner_etoiles(SDL_Renderer *rendu, Etoile *tab, int n, int W, int H, float temps) {
	for (int i = 0; i < n; ++i) {
		const Etoile *e = &tab[i];
		float scint = 0.8f + e->twinkle * sin_table[(int)((temps * 0.58f + e->phase) * 180 / M_PI) % ANGLE_STEPS];
		float r = e->rayon * scint;
		int alpha = 140 + (int)(100 * sin_table[(int)((temps * 0.37f + e->phase) * 180 / M_PI) % ANGLE_STEPS]);
		float xx = e->x + sin_table[(int)((temps * 0.3f + e->phase * 1.3f) * 180 / M_PI) % ANGLE_STEPS] * 3.3f;
		filledCircleRGBA(rendu, (int)xx, (int)e->y, r, 240, 245, 255, alpha);
	}
}