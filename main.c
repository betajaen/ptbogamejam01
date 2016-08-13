#define RETRO_WINDOW_CAPTION "Cats"
#define RETRO_ARENA_SIZE Kilobytes(8)
#define RETRO_WINDOW_DEFAULT_WIDTH 704
#define RETRO_WINDOW_DEFAULT_HEIGHT 480
#define RETRO_CANVAS_DEFAULT_WIDTH (RETRO_WINDOW_DEFAULT_WIDTH / 2)
#define RETRO_CANVAS_DEFAULT_HEIGHT (RETRO_WINDOW_DEFAULT_HEIGHT / 2)

#define TILE_SIZE 16

#define SECTION_WIDTH (RETRO_WINDOW_DEFAULT_WIDTH / TILE_SIZE)
#define SECTION_HEIGHT (RETRO_WINDOW_DEFAULT_HEIGHT / TILE_SIZE)

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

void Splat_Tile(Bitmap* bitmap, S32 x, S32 y, S32 s, U32 index)
{
  SDL_Rect src, dst;
  
  if (index == 0)
  {
    src.x = 0;
    src.y = 0;
  }
  else
  {
    src.x = (bitmap->w) / (index * s);
    src.y = (bitmap->h) % (index * s);
  }

  src.w = s;
  src.h = s;

  dst.x = x;
  dst.y = y;
  dst.w = s;
  dst.h = s;

  Canvas_Splat3(bitmap, &dst, &src);
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
  const U32 a = 1103515245;
  const U32 m = UINT_MAX;
  const U32 c = 3459237;

  (*seed) = rand(); // (a * (*seed) + c) % m;
  return (*seed);
}

void MoveSection(Level* level, U8 from, U8 to)
{
  memcpy(&level->sections[to], &level->sections[from], sizeof(Section));
}

void MakeSection(Section* section, U32 seed)
{
  memset(section, 0, sizeof(Section));
  section->seed = seed;

  for(S32 i=0;i < SECTION_WIDTH;i++)
  {
    for (S32 j=0;j < SECTION_HEIGHT;j++)
    {
      U32 index = Random(&section->seed) % 3;
      section->level[i + (j * SECTION_WIDTH)] = index;
    }
  }
}

void PushSection(Level* level, U32 seed)
{
  // Move section along
  MoveSection(level, (SECTIONS_PER_LEVEL - 1), (SECTIONS_PER_LEVEL - 2));
  MakeSection(&level->sections[(SECTIONS_PER_LEVEL - 1)], seed);
}

void DrawSection(Section* section, S32 xOffset)
{
  for(S32 i=0;i < SECTION_WIDTH;i++)
  {
    for (S32 j=0;j < SECTION_HEIGHT;j++)
    {
      U8 index = section->level[i + (j * SECTION_WIDTH)];
      if (index > 0)
      {
        Splat_Tile(&TILES1, xOffset + (i * TILE_SIZE), (j * TILE_SIZE), TILE_SIZE, index - 1);
      }
    }
  }
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

void DrawLevel()
{
  S32 offset = 0; // @TODO
  DrawSection(&LEVEL->sections[0], offset);
  DrawSection(&LEVEL->sections[1], offset + (SECTION_WIDTH * TILE_SIZE));
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

  Bitmap_Load("tiles1.png", &TILES1, 16);
}

void Start()
{
  GAME = Scope_New(Game);
  GAME->seed = 1;
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
    PushLevel(Random(&GAME->seed));
  }
  
  Canvas_PrintF(0, 0, &FONT_NEOSANS, 15, "%i", GAME->seed);

  DrawLevel();

  //Canvas_Splat(&TILES1, 0, 0, NULL);
  Canvas_Debug(&FONT_NEOSANS);
}
