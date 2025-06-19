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
#include <sys/ioctl.h>

#include "../include/etoiles.h"
#include "../include/toast.h"
#include "../include/config.h"
#include "../include/utils.h"

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
    SDL_Texture *texture;
    SDL_Rect dst;
} CachedText;

// Trigonometric lookup tables
#define ANGLE_STEPS 360
float sin_table[ANGLE_STEPS], cos_table[ANGLE_STEPS];
float random_values[NB_ETOILES * 4];

// Initialize trigonometric tables
void init_trig_tables() {
    for (int i = 0; i < ANGLE_STEPS; i++) {
        sin_table[i] = sinf(i * M_PI / 180.0f);
        cos_table[i] = cosf(i * M_PI / 180.0f);
    }
}

// File I/O functions
void enregistrer_angles_texte(const char *fichier, Curseur *curseurs, int n) {
    FILE *f = fopen(fichier, "w");
    if (!f) {
        fprintf(stderr, "Erreur ouverture fichier %s pour écriture\n", fichier);
        return;
    }
    for (int i = 0; i < n; ++i)
        fprintf(f, "%.2f\n", curseurs[i].valeur);
    fclose(f);
}

void charger_angles_texte(const char *fichier, Curseur *curseurs, int n) {
    FILE *f = fopen(fichier, "r");
    if (!f) {
        fprintf(stderr, "Erreur ouverture fichier %s pour lecture\n", fichier);
        return;
    }
    for (int i = 0; i < n; ++i)
        fscanf(f, "%f", &curseurs[i].valeur);
    fclose(f);
}

void enregistrer_angles_binaire(const char *fichier, Curseur *curseurs, int n) {
    FILE *f = fopen(fichier, "wb");
    if (!f) {
        fprintf(stderr, "Erreur ouverture fichier %s pour écriture binaire\n", fichier);
        return;
    }
    for (int i = 0; i < n; ++i)
        fwrite(&curseurs[i].valeur, sizeof(float), 1, f);
    fclose(f);
}

void charger_angles_binaire(const char *fichier, Curseur *curseurs, int n) {
    FILE *f = fopen(fichier, "rb");
    if (!f) {
        fprintf(stderr, "Erreur ouverture fichier %s pour lecture binaire\n", fichier);
        return;
    }
    for (int i = 0; i < n; ++i)
        fread(&curseurs[i].valeur, sizeof(float), 1, f);
    fclose(f);
}

int demander_chemin_et_format(char *chemin, size_t max, int *binaire, int exporter) {
    FILE *fp = popen("zenity --list --radiolist --title='Format de fichier' --column='' --column='Format' TRUE 'Texte' FALSE 'Binaire' --height=250 --width=300", "r");
    if (!fp) {
        fprintf(stderr, "Erreur lancement zenity pour sélection format\n");
        return 0;
    }
    char buf[32];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        fprintf(stderr, "Erreur lecture réponse zenity\n");
        return 0;
    }
    pclose(fp);
    *binaire = (strstr(buf, "Binaire") != NULL);

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "zenity --file-selection --%s --title='%s' --confirm-overwrite",
        exporter == 1 ? "save" : "file-selection",
        exporter == 1 ? "Exporter positions" : (exporter == 0 ? "Importer positions" : "Sauvegarder positions"));
    fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Erreur lancement zenity pour sélection fichier\n");
        return 0;
    }
    if (!fgets(chemin, max, fp)) {
        pclose(fp);
        fprintf(stderr, "Erreur lecture chemin fichier\n");
        return 0;
    }
    pclose(fp);
    char *nl = strchr(chemin, '\n');
    if (nl) *nl = 0;
    return 1;
}

// Rendering functions
void render_cached_text(SDL_Renderer *rendu, CachedText *text) {
    if (text->texture) {
        SDL_RenderCopy(rendu, text->texture, NULL, &text->dst);
    }
}

void init_cached_text(SDL_Renderer *rendu, TTF_Font *police_titre, TTF_Font *police_bouton, TTF_Font *police_etiquette, CachedText *title_chars, CachedText *buttons, CachedText *labels, int W, int H, SDL_Rect *zones_curseurs, SDL_Rect *zones_boutons, const char *textes_boutons[], char *etiquettes[]) {
    const char *title = "Articulated RobotArm";
    int total_width = 0;
    for (int i = 0; title[i]; i++) {
        char ch[2] = {title[i], 0};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(police_titre, ch, (SDL_Color){255,255,255,255});
        if (!surf) continue;
        title_chars[i].texture = SDL_CreateTextureFromSurface(rendu, surf);
        title_chars[i].dst = (SDL_Rect){0, (int)(H*0.01), surf->w, surf->h};
        total_width += surf->w;
        SDL_FreeSurface(surf);
    }
    int x_start = (W - total_width) / 2;
    for (int i = 0; title[i]; i++) {
        title_chars[i].dst.x = x_start;
        x_start += title_chars[i].dst.w;
    }

    for (int i = 0; i < NB_BOUTONS; i++) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(police_bouton, textes_boutons[i], (SDL_Color){255,255,255,255});
        if (!surf) continue;
        buttons[i].texture = SDL_CreateTextureFromSurface(rendu, surf);
        buttons[i].dst = (SDL_Rect){zones_boutons[i].x + (zones_boutons[i].w - surf->w)/2, zones_boutons[i].y + (zones_boutons[i].h - surf->h)/2, surf->w, surf->h};
        SDL_FreeSurface(surf);
    }

    for (int i = 0; i < NB_CURSEURS; i++) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(police_etiquette, etiquettes[i], (SDL_Color){255,255,255,255});
        if (!surf) continue;
        labels[i].texture = SDL_CreateTextureFromSurface(rendu, surf);
        labels[i].dst = (SDL_Rect){zones_curseurs[i].x + (zones_curseurs[i].w - surf->w)/2, zones_curseurs[i].y - 60, surf->w, surf->h};
        SDL_FreeSurface(surf);
    }
}

void update_text_positions(CachedText *title_chars, CachedText *buttons, CachedText *labels, SDL_Rect *zones_curseurs, SDL_Rect *zones_boutons, int W, int H) {
    if (W <= 0 || H <= 0) return; // Prevent division by zero
    const char *title = "Articulated RobotArm";
    int total_width = 0;
    for (int i = 0; title[i]; i++) {
        total_width += title_chars[i].dst.w;
    }
    int x_start = (W - total_width) / 2;
    for (int i = 0; title[i]; i++) {
        title_chars[i].dst.x = x_start;
        title_chars[i].dst.y = (int)(H*0.01);
        x_start += title_chars[i].dst.w;
    }

    for (int i = 0; i < NB_BOUTONS; i++) {
        buttons[i].dst.x = zones_boutons[i].x + (zones_boutons[i].w - buttons[i].dst.w)/2;
        buttons[i].dst.y = zones_boutons[i].y + (zones_boutons[i].h - buttons[i].dst.h)/2;
    }

    for (int i = 0; i < NB_CURSEURS; i++) {
        labels[i].dst.x = zones_curseurs[i].x + (zones_curseurs[i].w - labels[i].dst.w)/2;
        labels[i].dst.y = zones_curseurs[i].y - 60;
    }
}

void dessiner_titre_rgb_centre(SDL_Renderer *rendu, CachedText *title_chars, float temps) {
    const char *title = "Articulated RobotArm";
    for (int i = 0; title[i]; i++) {
        float angle = temps + i * 0.2f;
        Uint8 r = (Uint8)(127 + 127 * sin_table[(int)(angle * 180 / M_PI) % ANGLE_STEPS]);
        Uint8 g = (Uint8)(127 + 127 * sin_table[(int)((angle + 2.0f) * 180 / M_PI) % ANGLE_STEPS]);
        Uint8 b = (Uint8)(127 + 127 * sin_table[(int)((angle + 4.0f) * 180 / M_PI) % ANGLE_STEPS]);
        if (title_chars[i].texture) {
            SDL_SetTextureColorMod(title_chars[i].texture, r, g, b);
            render_cached_text(rendu, &title_chars[i]);
        }
    }
}

void dessiner_texte_centre(SDL_Renderer *rendu, TTF_Font *police, const char *txt, const SDL_Rect *rect, SDL_Color couleur) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(police, txt, couleur);
    if (!surf) {
        fprintf(stderr, "Erreur rendu texte: %s\n", SDL_GetError());
        return;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rendu, surf);
    if (!tex) {
        fprintf(stderr, "Erreur création texture texte: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return;
    }
    SDL_Rect dst = {rect->x + (rect->w - surf->w)/2, rect->y + (rect->h - surf->h)/2, surf->w, surf->h};
    SDL_RenderCopy(rendu, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void dessiner_bouton_rgb_arrondi(SDL_Renderer *rendu, const SDL_Rect *zone, float temps, int survole, int appuye) {
    roundedBoxRGBA(rendu, zone->x + 6, zone->y + 9, zone->x + zone->w + 6, zone->y + zone->h + 4, 18, 0, 0, 30, 140);
    Uint8 fondA = appuye ? 210 : (survole ? 155 : 90);
    roundedBoxRGBA(rendu, zone->x, zone->y, zone->x + zone->w, zone->y + zone->h, 18, 32, 38, 60, fondA);

    float pulse = 0.12f * sin_table[(int)(temps * 1.3f * 180 / M_PI) % ANGLE_STEPS];
    int epaisseur_bord = survole ? 10 : (7 + (int)(2.5f * pulse));
    float pulse_alpha = survole ? 1.0f : (0.55f + 0.40f * sin_table[(int)(temps * 1.6f * 180 / M_PI) % ANGLE_STEPS]);

    for (int k = 0; k < epaisseur_bord; k += 2) {
        float angle = temps * 1.4f + k * 0.13f;
        Uint8 r = (Uint8)(137 + 97 * sin_table[(int)(angle * 180 / M_PI) % ANGLE_STEPS]);
        Uint8 g = (Uint8)(110 + 115 * sin_table[(int)((angle + 2.3f) * 180 / M_PI) % ANGLE_STEPS]);
        Uint8 b = (Uint8)(157 + 97 * sin_table[(int)((angle + 4.7f) * 180 / M_PI) % ANGLE_STEPS]);
        Uint8 alpha_halo = (Uint8)((85 - k * 8) * pulse_alpha);
        roundedRectangleRGBA(rendu, zone->x - k, zone->y - k, zone->x + zone->w + k, zone->y + zone->h + k, 18 + k, r, g, b, alpha_halo);
    }

    SDL_SetRenderDrawColor(rendu, 255, 255, 255, 60);
    int h_third = zone->h / 3;
    int x_start = zone->x + 14;
    int x_end = zone->x + zone->w - 14;
    for (int i = 0; i < h_third; i++) {
        float alpha = 60.0f * (1.0f - (float)i / h_third);
        SDL_SetRenderDrawColor(rendu, 255, 255, 255, (Uint8)alpha);
        SDL_RenderDrawLine(rendu, x_start, zone->y + i, x_end, zone->y + i);
    }
}

void dessiner_cercle_aa(SDL_Renderer *rendu, int cx, int cy, int r, SDL_Color col) {
    filledCircleRGBA(rendu, cx, cy, r, col.r, col.g, col.b, col.a);
    aacircleRGBA(rendu, cx, cy, r, col.r, col.g, col.b, col.a);
}

void dessiner_glow(SDL_Renderer *rendu, int cx, int cy, int base_rad, SDL_Color col, float intensite) {
    SDL_SetRenderDrawBlendMode(rendu, SDL_BLENDMODE_ADD);
    for (int r = base_rad + 24; r > base_rad; r--) {
        float a = intensite * (1.0f - (float)(r - base_rad) / 24.0f);
        if (a < 0) a = 0;
        Uint8 alpha = (Uint8)(a * 60);
        aacircleRGBA(rendu, cx, cy, r, col.r, col.g, col.b, alpha);
    }
    SDL_SetRenderDrawBlendMode(rendu, SDL_BLENDMODE_NONE);
}

void dessiner_remplissage_gradient(SDL_Renderer *rendu, SDL_Rect *rect, SDL_Color c1, SDL_Color c2, int largeur_rempli) {
    for (int i = 0; i < largeur_rempli; i++) {
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
    static char value_buf[NB_CURSEURS][16];
    static float last_values[NB_CURSEURS] = {-1};
    if (curseur->valeur != last_values[i]) {
        snprintf(value_buf[i], sizeof(value_buf[i]), "%.0f", curseur->valeur);
        last_values[i] = curseur->valeur;
    }

    float breathing = (0.8f + 0.2f * sin_table[(int)(temps * 4 * 180 / M_PI) % ANGLE_STEPS]);
    SDL_Color col_glow = curseur->survole ? curseur->couleur2 : curseur->couleur1;
    SDL_SetRenderDrawColor(rendu, 40, 42, 52, 220);
    SDL_RenderFillRect(rendu, &curseur->zone);
    dessiner_remplissage_gradient(rendu, &curseur->zone, curseur->couleur1, curseur->couleur2, (curseur->valeur/180.0f)*curseur->zone.w);
    int largeur_rempli = (curseur->valeur/180.0f) * curseur->zone.w;
    int knob_cx = curseur->zone.x + largeur_rempli;
    int knob_cy = curseur->zone.y + curseur->zone.h/2;
    int knob_r = curseur->zone.h/2 + 12;
    dessiner_glow(rendu, knob_cx, knob_cy+2, knob_r+5, (SDL_Color){30,30,40,255}, 0.7*breathing);
    dessiner_glow(rendu, knob_cx, knob_cy, knob_r+3, col_glow, 0.8*breathing);
    dessiner_cercle_aa(rendu, knob_cx, knob_cy, knob_r, col_glow);
    for (int t = 0; t < 360; t += 2)
        SDL_SetRenderDrawColor(rendu, 255, 255, 255, 60),
        SDL_RenderDrawPoint(rendu, knob_cx + (int)(knob_r * cos_table[t]), knob_cy + (int)(knob_r * sin_table[t]));
    SDL_Rect valrect = {curseur->zone.x - 80, curseur->zone.y, 48, curseur->zone.h};
    dessiner_texte_centre(rendu, police_valeur, value_buf[i], &valrect, col_glow);
}

void disposition(int W, int H, SDL_Rect *zones_curseurs, SDL_Rect *zones_boutons) {
    if (W <= 0 || H <= 0) return; // Prevent division by zero
    int largeur_curseur = W * 0.35, hauteur_curseur = H/55;
    int espace = H/8;
    int DECALAGE_BAS = H/13;
    int curseurs_haut = H/2 - ((hauteur_curseur + espace)*NB_CURSEURS - espace)/2 + DECALAGE_BAS;
    int curseur_x = W/2 - largeur_curseur/2;
    for (int i = 0; i < NB_CURSEURS; ++i) {
        zones_curseurs[i].x = curseur_x;
        zones_curseurs[i].y = curseurs_haut + i*(hauteur_curseur + espace);
        zones_curseurs[i].w = largeur_curseur;
        zones_curseurs[i].h = hauteur_curseur;
    }
    int largeur_bouton = W/6, hauteur_bouton = H/9;
    int espace_col = H/13;
    int hauteur_groupe_boutons = (NB_BOUTONS/2) * hauteur_bouton + (NB_BOUTONS/2-1)*espace_col;
    int bouton_y = H/2 - hauteur_groupe_boutons/2 + DECALAGE_BAS;

    for (int i = 0; i < NB_BOUTONS/2; i++) {
        zones_boutons[i].x = 30;
        zones_boutons[i].y = bouton_y + i*(hauteur_bouton+espace_col);
        zones_boutons[i].w = largeur_bouton;
        zones_boutons[i].h = hauteur_bouton;
    }

    for (int i = 0; i < NB_BOUTONS/2; i++) {
        int idx = i + NB_BOUTONS/2;
        zones_boutons[idx].x = W - largeur_bouton - 30;
        zones_boutons[idx].y = bouton_y + i*(hauteur_bouton+espace_col);
        zones_boutons[idx].w = largeur_bouton;
        zones_boutons[idx].h = hauteur_bouton;
    }
}

// Function to open serial port
int open_serial_port(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        fprintf(stderr, "Erreur ouverture port série %s\n", port);
        return -1;
    }
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        fprintf(stderr, "Erreur configuration port série %s\n", port);
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, FNDELAY); // Non-blocking mode
    printf("Port série %s ouvert à 115200 bauds\n", port);
    return fd;
}

int main(int argc, char *argv[]) {
    // Initialize serial port
    const char *serial_port = "/dev/ttyACM0"; // Adjust as needed (e.g., "COM3" on Windows)
    int fd = open_serial_port(serial_port);

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    static int temp1 = 90, temp2 = 90, temp3 = 90, temp4 = 90, temp5 = 90, temp6 = 90;
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

    srand(SDL_GetTicks());
    init_trig_tables();
    init_random_values();

    TTF_Font *police_titre = TTF_OpenFont("./fonts/AVENGEANCE HEROIC AVENGER.ttf", 66);
    if (!police_titre) { fprintf(stderr, "Police titre manquante\n"); return 1; }
    TTF_Font *police_etiquette = TTF_OpenFont("./fonts/Orbitron-VariableFont_wght.ttf", 32);
    if (!police_etiquette) { fprintf(stderr, "Police label slider manquante\n"); return 1; }
    TTF_Font *police_valeur = TTF_OpenFont("./fonts/Orbitron-VariableFont_wght.ttf", 24);
    if (!police_valeur) { fprintf(stderr, "Police valeur manquante\n"); return 1; }
    TTF_Font *police_bouton = TTF_OpenFont("./fonts/space age.ttf", 24.5);
    if (!police_bouton) { fprintf(stderr, "Police bouton manquante\n"); return 1; }

    Curseur curseurs[NB_CURSEURS];
    Bouton boutons[NB_BOUTONS];
    SDL_Rect zones_curseurs[NB_CURSEURS];
    SDL_Rect zones_boutons[NB_BOUTONS];
    Etoile etoiles[NB_ETOILES];
    CachedText cached_title_chars[20] = {0}; // Initialize to prevent uninitialized access
    CachedText cached_button_texts[NB_BOUTONS] = {0};
    CachedText cached_cursor_labels[NB_CURSEURS] = {0};
    int quitter = 0, curseur_en_glisse = -1;
    int prev_mx = -1, prev_my = -1, prev_W = -1, prev_H = -1;
    Uint32 debut_ticks = SDL_GetTicks();
    Uint32 last_serial_update = 0;
    const Uint32 serial_update_interval = 300; // Increased to 300ms to reduce serial load

    SDL_Window *fen = SDL_CreateWindow("Articulated Robot Arm", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 800, SDL_WINDOW_RESIZABLE);
    if (!fen) { fprintf(stderr, "Erreur création fenêtre: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *rendu = SDL_CreateRenderer(fen, -1, SDL_RENDERER_ACCELERATED);
    if (!rendu) { fprintf(stderr, "Erreur création rendu: %s\n", SDL_GetError()); SDL_DestroyWindow(fen); return 1; }
    int W = 1400, H = 800;
    disposition(W, H, zones_curseurs, zones_boutons);
    init_etoiles(etoiles, NB_ETOILES, W, H);
    init_cached_text(rendu, police_titre, police_bouton, police_etiquette, cached_title_chars, cached_button_texts, cached_cursor_labels, W, H, zones_curseurs, zones_boutons, textes_boutons, etiquettes);

    for (int i = 0; i < NB_CURSEURS; ++i) {
        curseurs[i].valeur = 90.0f;
        curseurs[i].glisser = 0;
        curseurs[i].couleur1 = paires_couleurs[i][0];
        curseurs[i].couleur2 = paires_couleurs[i][1];
        snprintf(curseurs[i].etiquette, sizeof(curseurs[i].etiquette), "%s", etiquettes[i]);
        curseurs[i].survole = 0;
    }

    for (int i = 0; i < NB_BOUTONS; i++) {
        boutons[i].texte = textes_boutons[i];
        boutons[i].appuye = 0;
        boutons[i].survole = 0;
    }

    static float last_time = 0;
    char nom_fichier[256] = "";
    Toast toast = {"", 0, 0, {255,255,255,255}, 0};

    while (!quitter) {
        float temps = (SDL_GetTicks() - debut_ticks)/1000.0f;
        float dt = temps - last_time;
        last_time = temps;

        SDL_GetWindowSize(fen, &W, &H);
        if (W != prev_W || H != prev_H) {
            disposition(W, H, zones_curseurs, zones_boutons);
            init_etoiles(etoiles, NB_ETOILES, W, H);
            update_text_positions(cached_title_chars, cached_button_texts, cached_cursor_labels, zones_curseurs, zones_boutons, W, H);
            prev_W = W;
            prev_H = H;
        }

        //update_etoiles(etoiles, NB_ETOILES, W, H, dt);

        for (int i = 0; i < NB_CURSEURS; i++) curseurs[i].zone = zones_curseurs[i];
        for (int i = 0; i < NB_BOUTONS; i++) boutons[i].zone = zones_boutons[i];

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quitter = 1;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                SDL_GetWindowSize(fen, &W, &H);
                disposition(W, H, zones_curseurs, zones_boutons);
                init_etoiles(etoiles, NB_ETOILES, W, H);
                update_text_positions(cached_title_chars, cached_button_texts, cached_cursor_labels, zones_curseurs, zones_boutons, W, H);
                prev_W = W;
                prev_H = H;
            }
            if (e.type == SDL_MOUSEMOTION) {
                prev_mx = e.motion.x;
                prev_my = e.motion.y;
                for (int i = 0; i < NB_CURSEURS; ++i)
                    curseurs[i].survole = SDL_PointInRect(&(SDL_Point){prev_mx, prev_my}, &curseurs[i].zone);
                for (int i = 0; i < NB_BOUTONS; ++i)
                    boutons[i].survole = SDL_PointInRect(&(SDL_Point){prev_mx, prev_my}, &boutons[i].zone);
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                for (int i = 0; i < NB_CURSEURS; ++i)
                    if (curseurs[i].survole) {
                        curseur_en_glisse = i;
                        curseurs[i].glisser = 1;
                    }
                for (int i = 0; i < NB_BOUTONS; ++i)
                    if (boutons[i].survole)
                        boutons[i].appuye = 1;
                if (boutons[0].survole) {
                    char chemin[256]; int binaire = 0;
                    if (demander_chemin_et_format(chemin, sizeof(chemin), &binaire, 2)) {
                        if (binaire)
                            enregistrer_angles_binaire(chemin, curseurs, NB_CURSEURS);
                        else
                            enregistrer_angles_texte(chemin, curseurs, NB_CURSEURS);
                        afficher_toast(&toast, "Sauvegarde réussie", (SDL_Color){60,255,120,255});
                        strncpy(nom_fichier, chemin, 255); nom_fichier[255] = 0;
                    }
                }
                if (boutons[2].survole) {
                    for (int k = 0; k < NB_CURSEURS; ++k)
                        curseurs[k].valeur = 90.0f;
                    afficher_toast(&toast, "Angles réinitialisés", (SDL_Color){80,160,255,255});
                    nom_fichier[0] = 0;
                }
                if (boutons[3].survole) {
                    char chemin[256]; int binaire = 0;
                    if (demander_chemin_et_format(chemin, sizeof(chemin), &binaire, 1)) {
                        if (binaire)
                            enregistrer_angles_binaire(chemin, curseurs, NB_CURSEURS);
                        else
                            enregistrer_angles_texte(chemin, curseurs, NB_CURSEURS);
                        afficher_toast(&toast, "Exportation réussie", (SDL_Color){255,255,120,255});
                        strncpy(nom_fichier, chemin, 255); nom_fichier[255] = 0;
                    }
                }
                if (boutons[4].survole) {
                    char chemin[256]; int binaire = 0;
                    if (demander_chemin_et_format(chemin, sizeof(chemin), &binaire, 0)) {
                        if (binaire)
                            charger_angles_binaire(chemin, curseurs, NB_CURSEURS);
                        else
                            charger_angles_texte(chemin, curseurs, NB_CURSEURS);
                        afficher_toast(&toast, "Importation réussie", (SDL_Color){80,255,255,255});
                        strncpy(nom_fichier, chemin, 255); nom_fichier[255] = 0;
                    }
                }
            }
            if (e.type == SDL_MOUSEBUTTONUP) {
                if (curseur_en_glisse != -1) {
                    curseurs[curseur_en_glisse].glisser = 0;
                    curseur_en_glisse = -1;
                }
                for (int i = 0; i < NB_BOUTONS; ++i)
                    boutons[i].appuye = 0;
            }
            if (e.type == SDL_MOUSEMOTION && curseur_en_glisse != -1) {
                int mx = e.motion.x;
                Curseur *c = &curseurs[curseur_en_glisse];
                int pos = mx - c->zone.x;
                if (pos < 0) pos = 0;
                if (pos > c->zone.w) pos = c->zone.w;
                c->valeur = (pos/((float)c->zone.w))*180.0f;
            }
        }

        SDL_SetRenderDrawColor(rendu, 16, 18, 28, 255);
        SDL_RenderClear(rendu);
        //dessiner_etoiles(rendu, etoiles, NB_ETOILES, W, H, temps);
        dessiner_titre_rgb_centre(rendu, cached_title_chars, temps);
        for (int i = 0; i < NB_CURSEURS; ++i) {
            dessiner_curseur(rendu, police_etiquette, police_valeur, &curseurs[i], temps, i);
            render_cached_text(rendu, &cached_cursor_labels[i]);
        }
        for (int i = 0; i < NB_BOUTONS; ++i) {
            dessiner_bouton_rgb_arrondi(rendu, &boutons[i].zone, temps, boutons[i].survole, boutons[i].appuye);
            render_cached_text(rendu, &cached_button_texts[i]);
        }
        if (toast_actif(&toast)) {
            Uint32 elapsed = SDL_GetTicks() - toast.debut_ticks;
            Uint8 alpha = 255;
            if (elapsed > toast.duree_ms - 800) {
                float fade = (toast.duree_ms - elapsed) / 800.0f;
                if (fade < 0) fade = 0;
                if (fade > 1) fade = 1;
                alpha = (Uint8)(fade * 255);
            }
            int rect_w = 430, rect_h = 45;
            int x = 40, y_final = 34;
            float y = -rect_h;
            float t_slide = (elapsed < 440) ? (elapsed / 440.0f) : 1.0f;
            if (t_slide < 1.0f) {
                y = (1.0f - t_slide) * (-rect_h) + t_slide * y_final;
            } else {
                float t_rebond = (elapsed - 440) / 530.0f;
                if (t_rebond > 1.0f) t_rebond = 1.0f;
                y = y_final + (t_rebond < 1.0f ? 29 * expf(-3.4f * t_rebond) * sin_table[(int)(3.0f * 2 * M_PI * t_rebond * 180 / M_PI) % ANGLE_STEPS] : 0);
            }
            SDL_Rect rect = {x, (int)y, rect_w, rect_h};
            roundedBoxRGBA(rendu, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, 18, 34, 35, 44, (Uint8)(240 * alpha / 255));
            SDL_Color col_txt = toast.couleur;
            col_txt.a = alpha;
            dessiner_texte_centre(rendu, police_bouton, toast.texte, &rect, col_txt);

            if (nom_fichier[0]) {
                int txt_w = 600, txt_h = 30;
                SDL_Rect rect2 = {W - txt_w - 28, H - txt_h - 18, txt_w, txt_h};
                roundedBoxRGBA(rendu, rect2.x, rect2.y, rect2.x + rect2.w, rect2.y + rect2.h, 10, 24, 24, 30, (Uint8)(170 * alpha / 255));
                SDL_Color col_file = {180, 200, 255, alpha};
                dessiner_texte_centre(rendu, police_valeur, nom_fichier, &rect2, col_file);
            }
        } else {
            nom_fichier[0] = 0;
        }

        if ((int)curseurs[0].valeur != temp1 || (int)curseurs[1].valeur != temp2 || 
            (int)curseurs[2].valeur != temp3 || (int)curseurs[3].valeur != temp4 || 
            (int)curseurs[4].valeur != temp5 || (int)curseurs[5].valeur != temp6) {
            Uint32 current_ticks = SDL_GetTicks();
            if (current_ticks - last_serial_update >= serial_update_interval && fd != -1) {
                // Try to reopen serial port if it was closed
                if (fd == -1) {
                    fd = open_serial_port(serial_port);
                }
                if (fd != -1) {
                    // Check if serial buffer is not full
                    int bytes_pending;
                    if (ioctl(fd, TIOCOUTQ, &bytes_pending) == 0 && bytes_pending < 64) {
                        // Send commands in the format S<num><angle>\n
                        for (int i = 0; i < NB_CURSEURS; i++) {
                            if ((int)curseurs[i].valeur != (&temp1)[i]) {
                                char cmd[16];
                                snprintf(cmd, sizeof(cmd), "S%d%d\n", i+1, (int)curseurs[i].valeur);
                                if (write(fd, cmd, strlen(cmd)) == -1) {
                                    fprintf(stderr, "Erreur écriture port série\n");
                                    close(fd);
                                    fd = -1;
                                    break;
                                } else {
                                    printf("Envoyé: %s", cmd); // Debug output to console
                                }
                            }
                        }
                    }
                }
                last_serial_update = current_ticks;
                temp1 = (int)curseurs[0].valeur;
                temp2 = (int)curseurs[1].valeur;
                temp3 = (int)curseurs[2].valeur;
                temp4 = (int)curseurs[3].valeur;
                temp5 = (int)curseurs[4].valeur;
                temp6 = (int)curseurs[5].valeur;
            }
        }

        SDL_RenderPresent(rendu);
        SDL_Delay(50); // Increased delay to reduce CPU load
    }

    if (fd != -1) {
        close(fd); // Close serial port
    }
    for (int i = 0; i < 20; i++) if (cached_title_chars[i].texture) SDL_DestroyTexture(cached_title_chars[i].texture);
    for (int i = 0; i < NB_BOUTONS; i++) if (cached_button_texts[i].texture) SDL_DestroyTexture(cached_button_texts[i].texture);
    for (int i = 0; i < NB_CURSEURS; i++) if (cached_cursor_labels[i].texture) SDL_DestroyTexture(cached_cursor_labels[i].texture);
    TTF_CloseFont(police_titre);
    TTF_CloseFont(police_etiquette);
    TTF_CloseFont(police_valeur);
    TTF_CloseFont(police_bouton);
    SDL_DestroyRenderer(rendu);
    SDL_DestroyWindow(fen);
    SDL_Quit();
    TTF_Quit();
    return 0;
}