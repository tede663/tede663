#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define W 960
#define H 540

typedef struct {
    float x, y;
    float speed;
} Player;

typedef struct {
    float x, y, w, h;
    const char *image_path;
    const char *title;
    const char *text;
    bool collected;
    SDL_Texture *texture;
} Memory;

static SDL_Window *window;
static SDL_Renderer *renderer;
static TTF_Font *font;
static Mix_Music *music;

static Player player = {120, 420, 260.0f};

static const char *FONT_PATH = "/assets/NotoSans-Regular.ttf";
static const char *MUSIC_PATH = "/assets/music.ogg";

static int screen = 0; /* 0=start, 1=game, 2=memory popup, 3=door, 4=final */
static int active_memory = -1;
static bool running = true;
static Uint32 last_ticks;
static float target_x = 120, target_y = 420;

static const char *PLAYER_A[16] = {
    "....XX......",
    "...XXXX.....",
    "...XXXX.....",
    "..XXXXXX....",
    "..XX..XX....",
    "..XXXXXX....",
    "...XXXX.....",
    "..XXXXXX....",
    ".XXXXXXXX...",
    "..XX..XX....",
    "..XX..XX....",
    ".XXX..XXX...",
    ".XX....XX...",
    ".XX....XX...",
    "............",
    "............"
};

static const char *PLAYER_B[16] = {
    "....XX......",
    "...XXXX.....",
    "...XXXX.....",
    "..XXXXXX....",
    "..XX..XX....",
    "..XXXXXX....",
    "...XXXX.....",
    "..XXXXXX....",
    ".XXXXXXXX...",
    "..XX..XX....",
    "..XX..XX....",
    ".XX....XX...",
    ".XXX..XXX...",
    ".XX....XX...",
    "............",
    "............"
};

static Memory memories[1] = {
    {170, 145, 125, 125, "/assets/balloons.bmp",
     "Для Маши ❤️",
     "ПОЗДРАВЛЯЮ ТЕБЯ С ТВОИМ 19 ЛЕТЕЕМ! ЖЕЛАЮ ТЕБЕ ЗДОРОВЬЯ И ВСЕГО НАИИИИЛУЧШЕЕЕ, хочу скорее тебя увидеть уже!!!) P.S помни что тебе всегда 18, старушка)))",
     false, NULL}
};

static SDL_Texture *load_texture(const char *path) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    (void)surface;
    return NULL;
}

/* PNG/JPG loading without SDL_image: the supplied files are converted to BMP
   before the game build if desired. For this project we use SDL's image loader
   through Emscripten's bundled SDL2_image when available. */
static SDL_Texture *load_png_texture(const char *path) {
    SDL_Surface *surface = NULL;

    /* IMG_Load is declared by SDL_image. To keep the first build simple,
       the build command links SDL2_image too. */
    surface = IMG_Load(path);
    if (!surface) {
        fprintf(stderr, "Could not load %s: %s\n", path, IMG_GetError());
        return NULL;
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return tex;
}

static void draw_text(const char *text, int x, int y, int size, SDL_Color color) {
    TTF_Font *f = font;
    if (!f) return;

    TTF_SetFontSize(f, size);
    SDL_Surface *s = TTF_RenderUTF8_Blended_Wrapped(f, text, color, 760);
    if (!s) return;

    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(renderer, t, NULL, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

static void draw_centered_text(const char *text, int y, int size, SDL_Color color) {
    if (!font) return;
    TTF_SetFontSize(font, size);
    SDL_Surface *s = TTF_RenderUTF8_Blended_Wrapped(font, text, color, 820);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Rect dst = {(W - s->w) / 2, y, s->w, s->h};
    SDL_RenderCopy(renderer, t, NULL, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

static void fill_rect(int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

static void draw_button(int x, int y, int w, int h, const char *label) {
    fill_rect(x, y, w, h, (SDL_Color){238, 180, 205, 255});
    SDL_SetRenderDrawColor(renderer, 100, 65, 90, 255);
    SDL_Rect border = {x, y, w, h};
    SDL_RenderDrawRect(renderer, &border);
    draw_centered_text(label, y + 10, 24, (SDL_Color){60, 35, 55, 255});
}

static void draw_character(void) {
    const char **sprite = ((SDL_GetTicks() / 180) % 2) ? PLAYER_A : PLAYER_B;
    int px = (int)player.x - 24;
    int py = (int)player.y - 64;
    const int scale = 4;

    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 12; col++) {
            char c = sprite[row][col];
            if (c == 'X') {
                SDL_Color skin = {255, 205, 170, 255};
                SDL_Color shirt = {92, 135, 210, 255};
                SDL_Color hair = {55, 40, 48, 255};

                SDL_Color color = shirt;
                if (row < 7) color = hair;
                if (row == 5 || row == 6) color = skin;
                if (row >= 7 && row <= 9) color = shirt;

                fill_rect(px + col * scale, py + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_room(void) {
    /* wall */
    fill_rect(0, 0, W, H, (SDL_Color){248, 238, 225, 255});
    fill_rect(0, 0, W, 90, (SDL_Color){233, 210, 215, 255});
    fill_rect(0, 455, W, 85, (SDL_Color){205, 194, 208, 255});

    /* simple wall decoration */
    fill_rect(45, 120, 170, 4, (SDL_Color){210, 180, 185, 255});
    fill_rect(745, 115, 170, 4, (SDL_Color){210, 180, 185, 255});

    /* sofa */
    SDL_Texture *sofa = load_asset_texture("/assets/sofa.bmp");
    if (sofa) {
        SDL_Rect dst = {285, 300, 350, 220};
        SDL_RenderCopy(renderer, sofa, NULL, &dst);
        SDL_DestroyTexture(sofa);
    }

    /* table */
    SDL_Texture *table = load_asset_texture("/assets/table.bmp");
    if (table) {
        SDL_Rect dst = {25, 310, 220, 175};
        SDL_RenderCopy(renderer, table, NULL, &dst);
        SDL_DestroyTexture(table);
    }

    /* door */
    fill_rect(825, 150, 90, 305, (SDL_Color){116, 79, 87, 255});
    fill_rect(838, 165, 64, 290, (SDL_Color){173, 111, 122, 255});
    fill_rect(850, 300, 10, 10, (SDL_Color){245, 213, 128, 255});
    draw_text("Дверь", 836, 135, 18, (SDL_Color){90, 55, 70, 255});

    /* memory items */
    for (int i = 0; i < 1; i++) {
        if (memories[i].collected) continue;
        if (!memories[i].texture)
            memories[i].texture = load_asset_texture(memories[i].image_path);
        if (memories[i].texture) {
            SDL_Rect dst = {(int)memories[i].x, (int)memories[i].y,
                            (int)memories[i].w, (int)memories[i].h};
            SDL_RenderCopy(renderer, memories[i].texture, NULL, &dst);
        }
    }

    draw_character();
}

static void draw_popup(void) {
    fill_rect(70, 70, 820, 400, (SDL_Color){255, 250, 246, 250});
    SDL_SetRenderDrawColor(renderer, 125, 80, 105, 255);
    SDL_Rect border = {70, 70, 820, 400};
    SDL_RenderDrawRect(renderer, &border);

    draw_centered_text(memories[active_memory].title, 105, 30,
                       (SDL_Color){110, 55, 85, 255});
    draw_text(memories[active_memory].text, 115, 180, 25,
              (SDL_Color){55, 45, 55, 255});
    draw_button(350, 385, 260, 55, "Продолжить");
}

static void draw_door_screen(void) {
    fill_rect(0, 0, W, H, (SDL_Color){53, 39, 62, 255});
    draw_centered_text("Ты нашёл маленький подарок.", 110, 30,
                       (SDL_Color){255, 235, 245, 255});
    draw_centered_text("Продолжим нашу историю?", 175, 38,
                       (SDL_Color){255, 205, 225, 255});
    draw_button(260, 285, 190, 65, "Да ❤️");
    draw_button(510, 285, 190, 65, "Конечно!");
}

static void draw_final(void) {
    fill_rect(0, 0, W, H, (SDL_Color){43, 29, 48, 255});
    draw_centered_text("С днём рождения! ❤️", 75, 42,
                       (SDL_Color){255, 220, 235, 255});
    draw_centered_text("Маша, я хочу продолжить нашу историю.", 160, 30,
                       (SDL_Color){255, 245, 250, 255});
    draw_centered_text("Спасибо, что ты есть ❤️", 235, 28,
                       (SDL_Color){235, 220, 230, 255});
    draw_centered_text("С днём рождения, старушка!)))", 285, 25,
                       (SDL_Color){235, 220, 230, 255});
    draw_centered_text("❤️", 350, 48, (SDL_Color){255, 120, 160, 255});
}

static float dist(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return sqrtf(dx*dx + dy*dy);
}

static void try_collect(void) {
    if (!memories[0].collected &&
        dist(player.x, player.y, memories[0].x + memories[0].w/2,
             memories[0].y + memories[0].h/2) < 95) {
        memories[0].collected = true;
        active_memory = 0;
        screen = 2;
        return;
    }

    if (memories[0].collected &&
        player.x > 780 && player.y > 150 && player.y < 460)
        screen = 3;
}

static void handle_tap(float x, float y) {
    if (screen == 0) {
        screen = 1;
        if (!music) {
            music = Mix_LoadMUS(MUSIC_PATH);
            if (music) Mix_PlayMusic(music, -1);
        }
        return;
    }

    if (screen == 2) {
        if (x > 300 && x < 660 && y > 360 && y < 470)
            screen = 1;
        return;
    }

    if (screen == 3) {
        if (y > 260 && y < 370) {
            screen = 4;
        }
        return;
    }

    if (screen == 4) return;

    target_x = x;
    target_y = y;
}

static void handle_event(SDL_Event *e) {
    if (e->type == SDL_QUIT) running = false;

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        handle_tap((float)e->button.x, (float)e->button.y);
    }

    if (e->type == SDL_FINGERDOWN) {
        handle_tap(e->tfinger.x * W, e->tfinger.y * H);
    }

    if (e->type == SDL_KEYDOWN) {
        if (screen == 0) {
            handle_tap(W/2, H/2);
            return;
        }

        const float step = 35;
        if (e->key.keysym.sym == SDLK_LEFT)  player.x -= step;
        if (e->key.keysym.sym == SDLK_RIGHT) player.x += step;
        if (e->key.keysym.sym == SDLK_UP)    player.y -= step;
        if (e->key.keysym.sym == SDLK_DOWN)  player.y += step;

        if (e->key.keysym.sym == SDLK_RETURN && screen == 2)
            screen = 1;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    window = SDL_CreateWindow("Наша история",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_RenderSetLogicalSize(renderer, W, H);
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");

    font = TTF_OpenFont(FONT_PATH, 28);
    if (!font) {
        fprintf(stderr, "Font error: %s\n", TTF_GetError());
    }

    last_ticks = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            handle_event(&e);

        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) / 1000.0f;
        last_ticks = now;
        if (dt > 0.05f) dt = 0.05f;

        if (screen == 1) {
            float dx = target_x - player.x;
            float dy = target_y - player.y;
            float d = sqrtf(dx*dx + dy*dy);
            if (d > 4) {
                player.x += dx / d * player.speed * dt;
                player.y += dy / d * player.speed * dt;
            }
            if (player.x < 45) player.x = 45;
            if (player.x > 915) player.x = 915;
            if (player.y < 115) player.y = 115;
            if (player.y > 455) player.y = 455;

            try_collect();
        }

        SDL_SetRenderDrawColor(renderer, 20, 18, 25, 255);
        SDL_RenderClear(renderer);

        if (screen == 0) {
            fill_rect(0, 0, W, H, (SDL_Color){43, 29, 48, 255});
            draw_centered_text("Наша маленькая история", 130, 42,
                               (SDL_Color){255, 220, 235, 255});
            draw_centered_text("Нажми, чтобы начать", 235, 30,
                               (SDL_Color){255, 245, 250, 255});
            draw_button(330, 315, 300, 65, "Начать ❤️");
        } else if (screen == 1) {
            draw_room();
        } else if (screen == 2) {
            draw_room();
            draw_popup();
        } else if (screen == 3) {
            draw_door_screen();
        } else if (screen == 4) {
            draw_final();
        }

        SDL_RenderPresent(renderer);
    }

    for (int i = 0; i < 1; i++)
        if (memories[i].texture) SDL_DestroyTexture(memories[i].texture);

    if (music) Mix_FreeMusic(music);
    if (font) TTF_CloseFont(font);
    Mix_CloseAudio();
    Mix_Quit();
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
