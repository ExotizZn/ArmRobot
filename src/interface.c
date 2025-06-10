#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "../include/config.h"

typedef struct {
	float x, y, speed, rayon, twinkle;
	float phase;
} Etoile;

typedef struct {
	SDL_Rect zone;
	float valeur;
	int glisser;
	SDL_Color couleur1;
	SDL_Color couleur2;
	char etiquette[32];
	int survole;
} Curseur;

typedef struct {
	SDL_Rect zone;
	const char *texte;
	int appuye;
	int survole;
	int valeur;
} Bouton;

typedef struct {
	char texte[256];
	Uint32 debut_ticks;
	Uint32 duree_ms;
	SDL_Color couleur;
	int rebondi_fait;
} Toast;

// --- FOND ETOILES FLUIDE ---
void init_etoiles(Etoile *tab, int n, int W, int H) {
	for(int i=0; i<n; ++i) {
		tab[i].x = rand() % W;
		tab[i].y = rand() % H;
		tab[i].speed = 0.09f + ((rand()%100)/260.0f);
		tab[i].rayon = 0.8f + (rand()%100)/50.0f;
		tab[i].twinkle = 0.14f + (rand()%100)/320.0f;
		tab[i].phase = (rand()%628)/100.0f;
	}
}

void update_etoiles(Etoile *tab, int n, int W, int H, float dt) {
	for(int i=0; i<n; ++i) {
		tab[i].y += tab[i].speed * dt * 500.0f;
		if(tab[i].y > H) {
			tab[i].y = 0;
			tab[i].x = rand() % W;
			tab[i].speed = 0.09f + ((rand()%100)/260.0f);
			tab[i].rayon = 0.8f + (rand()%100)/50.0f;
			tab[i].twinkle = 0.14f + (rand()%100)/320.0f;
			tab[i].phase = (rand()%628)/100.0f;
		}
	}
}

void dessiner_etoiles(SDL_Renderer *rendu, Etoile *tab, int n, int W, int H, float temps) {
	for(int i=0; i<n; ++i) {
		float y = tab[i].y;
		float scint = 0.8f + tab[i].twinkle * sinf(temps*0.58f + tab[i].phase);
		float r = tab[i].rayon * scint;
		int alpha = 140 + 100 * fabs(sinf(temps*0.37f + tab[i].phase));
		float xx = tab[i].x + sinf(temps*0.3f + tab[i].phase*1.3f) * 3.3f;
		filledCircleRGBA(rendu, (int)xx, (int)y, r, 240, 245, 255, alpha);
	}
}

// --- TOAST ---
void afficher_toast(Toast *toast, const char *msg, SDL_Color couleur) {
	strncpy(toast->texte, msg, 255);
	toast->texte[255] = 0;
	toast->debut_ticks = SDL_GetTicks();
	toast->duree_ms = 2700;
	toast->couleur = couleur;
	toast->rebondi_fait = 0;
}

int toast_actif(const Toast *toast) {
	return toast->texte[0] && (SDL_GetTicks() - toast->debut_ticks < toast->duree_ms);
}

// --- SAUVEGARDE/CHARGEMENT (texte/binaire) ---
void enregistrer_angles_texte(const char *fichier, Curseur *curseurs, int n) {
	FILE *f = fopen(fichier, "w");
	if(!f) return;
	for(int i=0; i<n; ++i)
		fprintf(f, "%.2f\n", curseurs[i].valeur);
	fclose(f);
}

void charger_angles_texte(const char *fichier, Curseur *curseurs, int n) {
	FILE *f = fopen(fichier, "r");
	if(!f) return;
	for(int i=0; i<n; ++i)
		fscanf(f, "%f", &curseurs[i].valeur);
	fclose(f);
}

void enregistrer_angles_binaire(const char *fichier, Curseur *curseurs, int n) {
	FILE *f = fopen(fichier, "wb");
	if(!f) return;
	for(int i=0; i<n; ++i)
		fwrite(&curseurs[i].valeur, sizeof(float), 1, f);
	fclose(f);
}

void charger_angles_binaire(const char *fichier, Curseur *curseurs, int n) {
	FILE *f = fopen(fichier, "rb");
	if(!f) return;
	for(int i=0; i<n; ++i)
		fread(&curseurs[i].valeur, sizeof(float), 1, f);
	fclose(f);
}

int demander_chemin_et_format(char *chemin, size_t max, int *binaire, int exporter) {
	FILE *fp = popen("zenity --list --radiolist --title='Format de fichier' " "--column='' --column='Format' TRUE 'Texte' FALSE 'Binaire' --height=250 --width=300", "r");
	if(!fp) return 0;
	char buf[32];
	if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); return 0; }
	pclose(fp);
	*binaire = (strstr(buf, "Binaire") != NULL);

	char cmd[512];
	snprintf(cmd, sizeof(cmd),
			"zenity --file-selection --%s --title='%s' --confirm-overwrite", 
			exporter == 1 ? "save" : "file-selection", 
			exporter == 1 ? "Exporter positions" : (exporter==0 ? "Importer positions" : "Sauvegarder positions"));
	fp = popen(cmd, "r");
	if(!fp) return 0;
	if (!fgets(chemin, max, fp)) { pclose(fp); return 0; }
	pclose(fp);
	char *nl = strchr(chemin, '\n');
	if(nl) *nl = 0;
	return 1;
}

void dessiner_texte_centre(SDL_Renderer *rendu, TTF_Font *police, const char *txt, SDL_Rect *rect, SDL_Color couleur) {
	SDL_Surface *surf = TTF_RenderUTF8_Blended(police, txt, couleur);
	SDL_Texture *tex = SDL_CreateTextureFromSurface(rendu, surf);
	SDL_Rect dst;
	dst.w = surf->w;
	dst.h = surf->h;
	dst.x = rect->x + (rect->w - dst.w) / 2;
	dst.y = rect->y + (rect->h - dst.h) / 2;
	SDL_RenderCopy(rendu, tex, NULL, &dst);
	SDL_DestroyTexture(tex);
	SDL_FreeSurface(surf);
}

void dessiner_bouton_rgb_arrondi(SDL_Renderer *rendu, SDL_Rect *zone, float temps, int survole, int appuye) {
	roundedBoxRGBA(rendu, zone->x+6, zone->y+9, zone->x+zone->w+6, zone->y+zone->h+4, 18, 0,0,30, 140);
	Uint8 fondA = appuye ? 210 : (survole ? 155 : 90);
	roundedBoxRGBA(rendu, zone->x, zone->y, zone->x+zone->w, zone->y+zone->h, 18, 32,38,60, fondA);
	float pulse = 0.12f * sinf(temps * 1.3f);
	int epaisseur_bord = survole ? 10 : (7 + (int)(2.5 * pulse));
	float pulse_alpha = survole ? 1.0f : (0.55f + 0.40f * (sinf(temps*1.6f)));
	for(int k=0; k<epaisseur_bord; ++k) {
		float angle = temps*1.4f + k*0.13f;
		float r = 137 + 97 * sinf(angle);
		float g = 110 + 115 * sinf(angle + 2.3f);
		float b = 157 + 97 * sinf(angle + 4.7f);
		Uint8 alpha_halo = (Uint8)((85-k*8)*pulse_alpha);
		roundedRectangleRGBA(rendu, zone->x-k, zone->y-k, zone->x+zone->w+k, zone->y+zone->h+k,
			18+k, (Uint8)r, (Uint8)g, (Uint8)b, alpha_halo);
	}
	for(int i=0;i<zone->h/3;i++) {
		float alpha = 60*(1.0f - (float)i/(zone->h/3));
		SDL_SetRenderDrawColor(rendu, 255,255,255, (Uint8)alpha);
		SDL_RenderDrawLine(rendu, zone->x+14, zone->y+i, zone->x+zone->w-14, zone->y+i);
	}
}

void dessiner_cercle_aa(SDL_Renderer *rendu, int cx, int cy, int r, SDL_Color col) {
	for (int y = -r; y <= r; y++) {
		for (int x = -r; x <= r; x++) {
			float dist = sqrtf(x*x + y*y);
			if (dist < r - 1) {
				SDL_SetRenderDrawColor(rendu, col.r, col.g, col.b, col.a);
				SDL_RenderDrawPoint(rendu, cx + x, cy + y);
			} else if (dist < r + 1) {
				float alpha = (r + 1 - dist) * col.a;
				if (alpha < 0) alpha = 0;
				if (alpha > 255) alpha = 255;
				SDL_SetRenderDrawColor(rendu, col.r, col.g, col.b, (Uint8)alpha);
				SDL_RenderDrawPoint(rendu, cx + x, cy + y);
			}
		}
	}
}

void dessiner_glow(SDL_Renderer *rendu, int cx, int cy, int base_rad, SDL_Color col, float intensite) {
	SDL_SetRenderDrawBlendMode(rendu, SDL_BLENDMODE_ADD);
	for(int r=base_rad+24; r>base_rad; r--) {
		float a = intensite * (1.0 - (float)(r-base_rad)/24.0);
		if(a<0) a=0;
		SDL_SetRenderDrawColor(rendu, col.r, col.g, col.b, (Uint8)(a*60));
		for(int i=0;i<360;i+=2) {
			int x = cx + (int)(r*cos(i*M_PI/180));
			int y = cy + (int)(r*sin(i*M_PI/180));
			SDL_RenderDrawPoint(rendu,x,y);
		}
	}
	SDL_SetRenderDrawBlendMode(rendu, SDL_BLENDMODE_NONE);
}

void dessiner_remplissage_gradient(SDL_Renderer *rendu, SDL_Rect *rect, SDL_Color c1, SDL_Color c2, int largeur_rempli) {
	for(int i = 0; i < largeur_rempli; i++) {
		float t = (float)i / rect->w;
		SDL_SetRenderDrawColor(rendu,
			(int)(c1.r * (1-t) + c2.r * t),
			(int)(c1.g * (1-t) + c2.g * t),
			(int)(c1.b * (1-t) + c2.b * t),
			200);
		SDL_RenderDrawLine(rendu, rect->x + i, rect->y, rect->x + i, rect->y + rect->h - 1);
	}
}

void dessiner_curseur(SDL_Renderer *rendu, TTF_Font *police_etiquette, TTF_Font *police_valeur, Curseur *curseur, float temps, int i) {
	float breathing = (0.8f + 0.2f * sinf(temps*4 + curseur->zone.x/55.0f));
	SDL_Color col_glow = curseur->survole ? curseur->couleur2 : curseur->couleur1;
	SDL_SetRenderDrawColor(rendu, 40,42,52,220);
	SDL_RenderFillRect(rendu, &curseur->zone);
	dessiner_remplissage_gradient(rendu, &curseur->zone, curseur->couleur1, curseur->couleur2, (curseur->valeur/180.0f)*curseur->zone.w);
	int largeur_rempli = (curseur->valeur/180.0f) * curseur->zone.w;
	int knob_cx = curseur->zone.x + largeur_rempli;
	int knob_cy = curseur->zone.y + curseur->zone.h/2;
	int knob_r = curseur->zone.h/2 + 12;
	dessiner_glow(rendu, knob_cx, knob_cy+2, knob_r+5, (SDL_Color){30,30,40,255}, 0.7*breathing);
	dessiner_glow(rendu, knob_cx, knob_cy, knob_r+3, col_glow, 0.8*breathing);
	dessiner_cercle_aa(rendu, knob_cx, knob_cy, knob_r, col_glow);
	for(int t=0;t<360;t+=2)
		SDL_SetRenderDrawColor(rendu,255,255,255,60),
		SDL_RenderDrawPoint(rendu, knob_cx+(int)(knob_r*cos(t*M_PI/180)), knob_cy+(int)(knob_r*sin(t*M_PI/180)));
	char buf[16];
	snprintf(buf, sizeof(buf), "%.0f", curseur->valeur);
	SDL_Rect valrect = {curseur->zone.x - 80, curseur->zone.y, 48, curseur->zone.h};
	dessiner_texte_centre(rendu, police_valeur, buf, &valrect, col_glow);
	SDL_Rect labrect = {curseur->zone.x, curseur->zone.y - 60, curseur->zone.w, 28};
	dessiner_texte_centre(rendu, police_etiquette, curseur->etiquette, &labrect, col_glow);
}

void dessiner_titre_rgb_centre(SDL_Renderer *rendu, TTF_Font *police, const char* txt, int W, int y, float temps) {
	int len = strlen(txt), i, largeur_titre_px = 0;
	for (i = 0; i < len; i++) {
		int w, h;
		char c[2] = { txt[i], 0 };
		TTF_SizeUTF8(police, c, &w, &h);
		largeur_titre_px += w;
	}
	int x = (W - largeur_titre_px) / 2;
	for(i=0; i<len; i++) {
		float t = (float)i/len + temps*0.19f;
		float r = 127 + 127 * sinf(2*M_PI*t);
		float g = 127 + 127 * sinf(2*M_PI*t + 2.094f);
		float b = 127 + 127 * sinf(2*M_PI*t + 4.188f);
		char c[2] = {txt[i],0};
		SDL_Surface *surf = TTF_RenderUTF8_Blended(police, c, (SDL_Color){(Uint8)r,(Uint8)g,(Uint8)b,255});
		SDL_Texture *tex = SDL_CreateTextureFromSurface(rendu, surf);
		SDL_Rect dst = {x, y, surf->w, surf->h};
		SDL_RenderCopy(rendu, tex, NULL, &dst);
		x += surf->w;
		SDL_DestroyTexture(tex);
		SDL_FreeSurface(surf);
	}
}

void disposition(int W, int H, SDL_Rect *zones_curseurs, SDL_Rect *zones_boutons) {
	int largeur_curseur = W * 0.35, hauteur_curseur = H/55;
	int espace = H/8;
	int DECALAGE_BAS = H/13;
	int curseurs_haut = H/2 - ((hauteur_curseur + espace)*NB_CURSEURS - espace)/2 + DECALAGE_BAS;
	int curseur_x = W/2 - largeur_curseur/2;
	for(int i=0;i<NB_CURSEURS;++i) {
		zones_curseurs[i].x = curseur_x;
		zones_curseurs[i].y = curseurs_haut + i*(hauteur_curseur + espace);
		zones_curseurs[i].w = largeur_curseur;
		zones_curseurs[i].h = hauteur_curseur;
	}
	int largeur_bouton = W/6, hauteur_bouton = H/9;
	int espace_col = H/13;
	int hauteur_groupe_boutons = (NB_BOUTONS/2) * hauteur_bouton + (NB_BOUTONS/2-1)*espace_col;
	int bouton_y = H/2 - hauteur_groupe_boutons/2 + DECALAGE_BAS;

	for(int i=0;i<NB_BOUTONS/2;i++) {
		zones_boutons[i].x = 30;
		zones_boutons[i].y = bouton_y + i*(hauteur_bouton+espace_col);
		zones_boutons[i].w = largeur_bouton; zones_boutons[i].h = hauteur_bouton;
	}

	for(int i=0;i<NB_BOUTONS/2;i++) {
		int idx = i+NB_BOUTONS/2;
		zones_boutons[idx].x = W - largeur_bouton - 30;
		zones_boutons[idx].y = bouton_y + i*(hauteur_bouton+espace_col);
		zones_boutons[idx].w = largeur_bouton; zones_boutons[idx].h = hauteur_bouton;
	}
}

int main(int argc, char *argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();

	static int temp1 = 90, temp2 = 90, temp3 = 90, temp4 = 90, temp5 = 90, temp6 = 90;

	const char *portname = "/dev/ttyACM0";
	int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) {
		perror("Erreur ouverture port série");
		// (option: SDL_ShowSimpleMessageBox("Erreur","Port série non trouvé",NULL))
		return 1;
	}
	// Configure port série (comme dans ton exemple)
	struct termios tty;
	memset(&tty, 0, sizeof tty);
	if (tcgetattr(fd, &tty) != 0) { perror("Erreur tcgetattr"); close(fd); return 1; }
	cfsetospeed(&tty, B9600);
	cfsetispeed(&tty, B9600);
	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	tty.c_iflag &= ~IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;
	if (tcsetattr(fd, TCSANOW, &tty) != 0) { perror("Erreur tcsetattr"); close(fd); return 1; }

	srand(SDL_GetTicks());
	TTF_Font *police_titre = TTF_OpenFont("./fonts/AVENGEANCE HEROIC AVENGER.ttf", 66);
	if(!police_titre) { printf("Police titre manquante\n"); return 1; }
	TTF_Font *police_etiquette = TTF_OpenFont("./fonts/Orbitron-VariableFont_wght.ttf", 32);
	if(!police_etiquette) { printf("Police label slider manquante\n"); return 1; }
	TTF_Font *police_valeur = TTF_OpenFont("./fonts/Orbitron-VariableFont_wght.ttf", 24);
	if(!police_valeur) { printf("Police valeur manquante\n"); return 1; }
	TTF_Font *police_bouton = TTF_OpenFont("./fonts/space age.ttf", 24.5);
	if(!police_bouton) { printf("Police bouton manquante\n"); return 1; }
	static char *etiquettes[NB_CURSEURS] = {"Base", "Épaule", "Coude", "Poignet", "Pinces", "Rotation"};
	static SDL_Color paires_couleurs[NB_CURSEURS][2] = {
		{{255, 80, 80, 255},   {255, 200, 80, 255}},
		{{255, 255, 80, 255},  {60, 255, 120, 255}},
		{{60, 255, 255, 255},  {80, 140, 255, 255}},
		{{170, 70, 255, 255},  {255, 55, 170, 255}},
		{{255, 120, 60, 255},  {255, 220, 60, 255}},
		{{50, 240, 120, 255},  {60, 255, 255, 255}},
	};
	static const char *textes_boutons[NB_BOUTONS] = {
		"SAUVEGARDER", "SÉQUENCE", "RÉINITIALISER",
		"EXPORTER", "IMPORTER", "STOP"
	};
	Curseur curseurs[NB_CURSEURS];
	Bouton boutons[NB_BOUTONS];
	SDL_Rect zones_curseurs[NB_CURSEURS];
	SDL_Rect zones_boutons[NB_BOUTONS];
	Etoile etoiles[NB_ETOILES];
	int quitter=0, curseur_en_glisse=-1;
	SDL_Event e;
	Uint32 debut_ticks = SDL_GetTicks();
	SDL_Window *fen = SDL_CreateWindow("Articulated Robot Arm", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 800, SDL_WINDOW_RESIZABLE);
	SDL_Renderer *rendu = SDL_CreateRenderer(fen, -1, SDL_RENDERER_ACCELERATED);
	int W=1400, H=800;
	disposition(W, H, zones_curseurs, zones_boutons);
	init_etoiles(etoiles, NB_ETOILES, W, H);

	static float last_time = 0;
	char nom_fichier[256] = "";
	Toast toast = {"", 0, 0, {255,255,255,255}, 0};

	for(int i=0; i<NB_CURSEURS; ++i) {
		curseurs[i].valeur = 90.0f;
		curseurs[i].glisser = 0;
		curseurs[i].couleur1 = paires_couleurs[i][0];
		curseurs[i].couleur2 = paires_couleurs[i][1];
		snprintf(curseurs[i].etiquette, sizeof(curseurs[i].etiquette), "%s", etiquettes[i]);
		curseurs[i].survole = 0;
	}

	for(int i=0;i<NB_BOUTONS;i++) {
		boutons[i].texte = textes_boutons[i];
		boutons[i].appuye = 0;
		boutons[i].survole = 0;
	}

	while(!quitter) {
		float temps = (SDL_GetTicks() - debut_ticks)/1000.0f;
		float dt = temps - last_time;
		last_time = temps;

		SDL_GetWindowSize(fen, &W, &H);
		disposition(W, H, zones_curseurs, zones_boutons);

		//update_etoiles(etoiles, NB_ETOILES, W, H, dt);

		for(int i=0;i<NB_CURSEURS;i++) curseurs[i].zone = zones_curseurs[i];
		for(int i=0;i<NB_BOUTONS;i++) boutons[i].zone = zones_boutons[i];
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		for(int i=0;i<NB_CURSEURS;++i)
			curseurs[i].survole = SDL_PointInRect(&(SDL_Point){mx, my}, &curseurs[i].zone);
		for(int i=0;i<NB_BOUTONS;++i)
			boutons[i].survole = SDL_PointInRect(&(SDL_Point){mx, my}, &boutons[i].zone);
		while(SDL_PollEvent(&e)) {
			if(e.type == SDL_QUIT) quitter=1;
			if(e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				SDL_GetWindowSize(fen, &W, &H);
				disposition(W, H, zones_curseurs, zones_boutons);
				init_etoiles(etoiles, NB_ETOILES, W, H);
			}
			if(e.type == SDL_MOUSEBUTTONDOWN) {
				for(int i=0; i<NB_CURSEURS; ++i)
					if(curseurs[i].survole) {
						curseur_en_glisse=i;
						curseurs[i].glisser=1;
					}
				for(int i=0; i<NB_BOUTONS; ++i)
					if(boutons[i].survole)
						boutons[i].appuye=1;
				// --- Gestion boutons (save/export/import/reset) ---
				if(boutons[0].survole) {
					char chemin[256]; int binaire = 0;
					if(demander_chemin_et_format(chemin, sizeof(chemin), &binaire, 2)) {
						if(binaire)
							enregistrer_angles_binaire(chemin, curseurs, NB_CURSEURS);
						else
							enregistrer_angles_texte(chemin, curseurs, NB_CURSEURS);
						afficher_toast(&toast, "Sauvegarde réussie", (SDL_Color){60,255,120,255});
						strncpy(nom_fichier, chemin, 255); nom_fichier[255]=0;
					}
				}
				if(boutons[2].survole) {
					for(int k=0; k<NB_CURSEURS; ++k)
						curseurs[k].valeur = 90.0f;
					afficher_toast(&toast, "Angles réinitialisés°", (SDL_Color){80,160,255,255});
					nom_fichier[0] = 0;
				}
				if(boutons[3].survole) {
					char chemin[256]; int binaire = 0;
					if(demander_chemin_et_format(chemin, sizeof(chemin), &binaire, 1)) {
						if(binaire)
							enregistrer_angles_binaire(chemin, curseurs, NB_CURSEURS);
						else
							enregistrer_angles_texte(chemin, curseurs, NB_CURSEURS);
						afficher_toast(&toast, "Exportation réussie", (SDL_Color){255,255,120,255});
						strncpy(nom_fichier, chemin, 255); nom_fichier[255]=0;
					}
				}
				if(boutons[4].survole) {
					char chemin[256]; int binaire = 0;
					if(demander_chemin_et_format(chemin, sizeof(chemin), &binaire, 0)) {
						if(binaire)
							charger_angles_binaire(chemin, curseurs, NB_CURSEURS);
						else
							charger_angles_texte(chemin, curseurs, NB_CURSEURS);
						afficher_toast(&toast, "Importation réussie", (SDL_Color){80,255,255,255});
						strncpy(nom_fichier, chemin, 255); nom_fichier[255]=0;
					}
				}
			}
			if(e.type == SDL_MOUSEBUTTONUP) {
				if(curseur_en_glisse!=-1) {
					curseurs[curseur_en_glisse].glisser=0;
					curseur_en_glisse=-1;
				}
				for(int i=0; i<NB_BOUTONS; ++i)
					boutons[i].appuye=0;
			}
			if(e.type == SDL_MOUSEMOTION && curseur_en_glisse!=-1) {
				int mx = e.motion.x;
				Curseur *c = &curseurs[curseur_en_glisse];
				int pos = mx - c->zone.x;
				if(pos<0) pos=0;
				if(pos>c->zone.w) pos=c->zone.w;
				c->valeur = (pos/((float)c->zone.w))*180.0f;
			}
		}

		// --- FOND SPATIAL ANIMÉ ---
		SDL_SetRenderDrawColor(rendu, 16,18,28,255);
		SDL_RenderClear(rendu);
		dessiner_etoiles(rendu, etoiles, NB_ETOILES, W, H, temps);

		dessiner_titre_rgb_centre(rendu, police_titre, "Articulated RobotArm", W, H*0.01, temps);
		for(int i=0; i<NB_CURSEURS; ++i)
			dessiner_curseur(rendu, police_etiquette, police_valeur, &curseurs[i], temps, i);
		for(int i=0; i<NB_BOUTONS; ++i) {
			dessiner_bouton_rgb_arrondi(rendu, &boutons[i].zone, temps, boutons[i].survole, boutons[i].appuye);
			dessiner_texte_centre(rendu, police_bouton, boutons[i].texte, &boutons[i].zone, (SDL_Color){255,255,255,255});
		}
		if(toast_actif(&toast)) {
			Uint32 elapsed = SDL_GetTicks() - toast.debut_ticks;
			Uint8 alpha = 255;
			if(elapsed > toast.duree_ms - 800) {
				float fade = (toast.duree_ms - elapsed) / 800.0f;
				if(fade < 0) fade = 0;
				if(fade > 1) fade = 1;
				alpha = (Uint8)(fade * 255);
			}
			int rect_w = 430, rect_h = 45;
			int x = 40;
			int y_final = 34;
			float y = -rect_h;
			float t_slide = (elapsed < 440) ? (elapsed / 440.0f) : 1.0f;
			if(t_slide < 1.0f) {
				y = (1.0f-t_slide)*(-rect_h) + t_slide*y_final;
			} else {
				float t_rebond = (elapsed-440)/530.0f;
				if(t_rebond > 1.0f) t_rebond = 1.0f;
				float nb_rebonds = 3.0f;
				float amplitude = 29;
				float damping = 3.4f;
				float overshoot = 0.0f;
				if(t_rebond < 1.0f)
					overshoot = amplitude * expf(-damping*t_rebond) * sinf(nb_rebonds*2*M_PI*t_rebond);
				y = y_final + overshoot;
			}
			SDL_Rect rect = {x, (int)y, rect_w, rect_h};
			roundedBoxRGBA(rendu, rect.x, rect.y, rect.x+rect.w, rect.y+rect.h, 18, 34,35,44, (Uint8)(240*alpha/255));
			SDL_Color col_txt = toast.couleur;
			col_txt.a = alpha;
			dessiner_texte_centre(rendu, police_bouton, toast.texte, &rect, col_txt);

			if(nom_fichier[0]) {
				int txt_w = 600, txt_h = 30;
				SDL_Rect rect2 = {W-txt_w-28, H-txt_h-18, txt_w, txt_h};
				roundedBoxRGBA(rendu, rect2.x, rect2.y, rect2.x+rect2.w, rect2.y+rect2.h, 10, 24,24,30, (Uint8)(170*alpha/255));
				SDL_Color col_file = {180,200,255,alpha};
				dessiner_texte_centre(rendu, police_valeur, nom_fichier, &rect2, col_file);
			}
		}
		else { nom_fichier[0] = 0; }

		if((int)curseurs[0].valeur != temp1 || (int)curseurs[1].valeur != temp2 || (int)curseurs[2].valeur != temp3 || (int)curseurs[3].valeur != temp4 || (int)curseurs[4].valeur != temp5 || (int)curseurs[5].valeur != temp6) {
			char cmd[100];
			snprintf(cmd, sizeof(cmd),
			"M1:%d,M2:%d,M3:%d,M4:%d,M5:%d,M6:%d\r\n",
			(int)curseurs[5].valeur,
			(int)curseurs[4].valeur,
			(int)curseurs[0].valeur,
			(int)curseurs[2].valeur,
			(int)curseurs[1].valeur,
			(int)curseurs[3].valeur);
	
			write(fd, cmd, strlen(cmd));

			temp1 = (int)curseurs[0].valeur;
			temp2 = (int)curseurs[1].valeur;
			temp3 = (int)curseurs[2].valeur;
			temp4 = (int)curseurs[3].valeur;
			temp5 = (int)curseurs[4].valeur;
			temp6 = (int)curseurs[5].valeur;
		}


		SDL_RenderPresent(rendu);
		SDL_Delay(16);
	}
	TTF_CloseFont(police_titre);
	TTF_CloseFont(police_etiquette);
	TTF_CloseFont(police_valeur);
	TTF_CloseFont(police_bouton);
	SDL_DestroyRenderer(rendu);
	SDL_DestroyWindow(fen);
	SDL_Quit(); TTF_Quit();
	return 0;
}
