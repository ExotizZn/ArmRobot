#include <stdlib.h>

#include "../include/config.h"

// Initialize random values for stars
void init_random_values() {
	for (int i = 0; i < NB_ETOILES * 4; i++) {
		random_values[i] = (rand() % 100) / 100.0f;
	}
}

float ease_out_bounce(float t) {
	if (t < 1/2.75) return 7.5625f * t * t;
	if (t < 2/2.75) { t -= 1.5/2.75; return 7.5625f * t * t + 0.75; }
	if (t < 2.5/2.75) { t -= 2.25/2.75; return 7.5625f * t * t + 0.9375; }
	t -= 2.625/2.75; return 7.5625f * t * t + 0.984375;
}