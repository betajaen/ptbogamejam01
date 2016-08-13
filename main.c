#define RETRO_WINDOW_CAPTION "Cats"

#include "retro.c"

static Font           FONT_NEOSANS;
static Bitmap         SPRITESHEET;
static Animation      ANIMATEDSPRITE_QUOTE_IDLE;
static Animation      ANIMATEDSPRITE_QUOTE_WALK;
static Sound          SOUND_COIN;

typedef enum 
{
  AC_JUMP,
  AC_RAGDOLL,
  AC_PAUSE,
  AC_CONFIRM,
  AC_CANCEL,
} Actions;


typedef enum
{
  PF_Idle,
  PF_Walking
} PlayerState;

typedef struct
{
  AnimatedSpriteObject player;
  Point velocity;
} GameState;

GameState* state;

void Init(Settings* settings)
{
  settings->windowWidth = 1280;
  settings->windowHeight = 720;

  Palette_Make(&settings->palette);
  Palette_LoadFromBitmap("palette.png", &settings->palette);

  Input_BindKey(SDL_SCANCODE_SPACE, AC_JUMP);
  Input_BindKey(SDL_SCANCODE_LSHIFT, AC_RAGDOLL);

  Font_Load("NeoSans.png", &FONT_NEOSANS, Colour_Make(0,0,255), Colour_Make(255,0,255));
  Bitmap_Load("cave.png", &SPRITESHEET, 0);

  Animation_LoadHorizontal(&ANIMATEDSPRITE_QUOTE_IDLE, &SPRITESHEET, 1, 100, 0, 80, 16, 16);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_QUOTE_WALK, &SPRITESHEET, 4, 120, 0, 80, 16, 16);

  Sound_Load(&SOUND_COIN, "coin.wav");
}

void Start()
{
  state = Scope_New(GameState);

  AnimatedSpriteObject_Make(&state->player, &ANIMATEDSPRITE_QUOTE_WALK, Canvas_GetWidth() / 2, Canvas_GetHeight() / 2);
  AnimatedSpriteObject_PlayAnimation(&state->player, true, true);

  state->velocity.x = 0;
  state->velocity.y = 0;

  // Music_Play("origin.mod");
}

void Step()
{
  Canvas_Debug(&FONT_NEOSANS);
}
