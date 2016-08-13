#define RETRO_WINDOW_CAPTION "Cats"
#define RETRO_ARENA_SIZE Kilobytes(8)
#define RETRO_WINDOW_DEFAULT_WIDTH 704
#define RETRO_WINDOW_DEFAULT_HEIGHT 480
#define RETRO_CANVAS_DEFAULT_WIDTH (RETRO_WINDOW_DEFAULT_WIDTH / 2)
#define RETRO_CANVAS_DEFAULT_HEIGHT (RETRO_WINDOW_DEFAULT_HEIGHT / 2)

#define TILE_WIDTH 16
#define TILE_HEIGHT 16

#define SECTION_WIDTH (RETRO_WINDOW_DEFAULT_WIDTH / TILE_WIDTH)
#define SECTION_HEIGHT (RETRO_WINDOW_DEFAULT_HEIGHT / TILE_HEIGHT)

#define LEVEL_WIDTH (SECTION_WIDTH * 2)
#define LEVEL_HEIGHT (SECTION_HEIGHT)
#define MAX_OBJECTS_PER_SECTION 16
#define SECTIONS_PER_LEVEL (2)

#include "retro.c"

static Font           FONT_NEOSANS;
static Bitmap         SPRITESHEET;
static Animation      ANIMATEDSPRITE_QUOTE_IDLE;
static Animation      ANIMATEDSPRITE_QUOTE_WALK;
static Sound          SOUND_COIN;
static Bitmap         TILES1;

void Splat_Tile(Bitmap* bitmap, S32 x, S32 y, S32 w, S32 h, U32 index)
{
  //U32 ox = bitmap->w % x;
  //U32 oy = bitmap->h % y;
}


typedef enum 
{
  AC_JUMP,
  AC_RAGDOLL,
  AC_PAUSE,
  AC_CONFIRM,
  AC_CANCEL,
  AC_RESET,
} Actions;

typedef enum
{
  PF_Idle,
  PF_Walking
} PlayerState;

typedef struct
{
  AnimatedSpriteObject sprite;
  S32                  velocityX, velocitY;
  U8                   objectType;
} Object;

typedef struct
{
  U32 seed;
  U8  objectCount;
  U8  level[SECTION_WIDTH * SECTION_HEIGHT];
  Object objects[MAX_OBJECTS_PER_SECTION];
} Section;

typedef struct
{
  U32 seed;
  U32 levelRandom, objectRandom;
  U32 baseX, cameraOffset;

  Section sections[2];
} Level;

typedef struct
{
  U32 seed;
  U32 cameraX;

  AnimatedSpriteObject player;
  Point velocity;
} Game;

Game*      GAME;
Level*     LEVEL;

U32  Random(U32* seed)
{
  // TODO
  (*seed) += 3;

  return *seed;
}

void MoveSection(Level* level, U8 from, U8 to)
{
  memcpy(&level->sections[to], &level->sections[from], sizeof(Section));
}

void MakeSection(Section* section, U32 seed)
{
  memset(section, 0, sizeof(Section));
}

void PushSection(Level* level, U32 seed)
{
  // Move section along
  MoveSection(level, (SECTIONS_PER_LEVEL - 1), (SECTIONS_PER_LEVEL - 2));
  MakeSection(&level->sections[(SECTIONS_PER_LEVEL - 1)], seed);
}

void PushLevel(U32 seed)
{
  Scope_Push('LEVL');
  LEVEL = Scope_New(Level);
  memset(LEVEL, 0, sizeof(Level));

  // Add the first two sections here.
  PushSection(LEVEL, Random(&LEVEL->seed));
  PushSection(LEVEL, Random(&LEVEL->seed));

  // TODO: Build player here.
  // TODO: Build cat player here.

}

void PopLevel()
{
  Scope_Pop();
  LEVEL = NULL;
}

void Init(Settings* settings)
{
  Palette_Make(&settings->palette);
  Palette_LoadFromBitmap("palette.png", &settings->palette);

  Input_BindKey(SDL_SCANCODE_SPACE, AC_JUMP);
  Input_BindKey(SDL_SCANCODE_LSHIFT, AC_RAGDOLL);
  Input_BindKey(SDL_SCANCODE_R, AC_RESET);

  Font_Load("NeoSans.png", &FONT_NEOSANS, Colour_Make(0,0,255), Colour_Make(255,0,255));
  Bitmap_Load("cave.png", &SPRITESHEET, 0);

  Animation_LoadHorizontal(&ANIMATEDSPRITE_QUOTE_IDLE, &SPRITESHEET, 1, 100, 0, 80, 16, 16);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_QUOTE_WALK, &SPRITESHEET, 4, 120, 0, 80, 16, 16);

  Sound_Load(&SOUND_COIN, "coin.wav");

  Bitmap_Load("tiles1.png", &TILES1, 0);
}

void Start()
{
  GAME = Scope_New(Game);
  //AnimatedSpriteObject_Make(&game->player, &ANIMATEDSPRITE_QUOTE_WALK, Canvas_GetWidth() / 2, Canvas_GetHeight() / 2);
  //AnimatedSpriteObject_PlayAnimation(&game->player, true, true);

  // Music_Play("origin.mod");
}

void Step()
{
  if (Input_GetActionReleased(AC_RESET))
  {
    PopLevel();
  }

  if (LEVEL == NULL)
  {
    GAME->seed = GAME->seed << 8 | GAME->seed;
    GAME->seed += 0x9328234;
    PushLevel(GAME->seed);
  }
  
  Canvas_PrintF(0, 0, &FONT_NEOSANS, 15, "%i", GAME->seed);

  //Canvas_Splat(&TILES1, 0, 0, NULL);
  Canvas_Debug(&FONT_NEOSANS);
}
