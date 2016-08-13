#define RETRO_WINDOW_CAPTION "Cats"
#define RETRO_ARENA_SIZE Kilobytes(8)
#define RETRO_WINDOW_DEFAULT_WIDTH 1280
#define RETRO_WINDOW_DEFAULT_HEIGHT 480
#define RETRO_CANVAS_DEFAULT_WIDTH (RETRO_WINDOW_DEFAULT_WIDTH / 2)
#define RETRO_CANVAS_DEFAULT_HEIGHT (RETRO_WINDOW_DEFAULT_HEIGHT / 2)

#define TILE_SIZE 16

#define SECTION_WIDTH 22  //(RETRO_CANVAS_DEFAULT_WIDTH / TILE_SIZE)
#define SECTION_HEIGHT 15 //(RETRO_CANVAS_DEFAULT_HEIGHT / TILE_SIZE)

#define LEVEL_WIDTH (SECTION_WIDTH * 2)
#define LEVEL_HEIGHT (SECTION_HEIGHT)
#define MAX_OBJECTS_PER_SECTION 16
#define SECTIONS_PER_LEVEL (4)

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
  U8  level[SECTION_WIDTH * SECTION_HEIGHT];
} SectionData;

SectionData* SECTION_DATA[256];
U32 SECTION_DATA_COUNT;

char* skipWhitespace(char* s)
{
  while(*s != 0 && isspace(*s))
    s++;
  return s;
}

char* skipToDigit(char* s)
{
  while(*s != 0 && !isdigit(*s))
    s++;
  return s;
}

char* skipString(char* s, const char* str)
{
  s = skipWhitespace(s);
  s += strlen(str);
  s = skipWhitespace(s);
  return s;
}

bool skipToString(char* s, char** t, const char* str)
{
  (*t) = strstr(s, str);
  if ((*t) == NULL)
    return false;
  return true;
}

char* readUInt(char* s, U32* i)
{
  (*i) = 0;

  while(*s != 0 && isdigit(*s))
  {
    (*i) = (*i) * 10  + ((*s) - '0');
    s++;
  }

  return s;
}

void LoadSectionData()
{

  U8 sectionIndex = 0;

  U32 dataSize;
  char* data = TextFile_Load("sections.tmx", &dataSize);
  
  while(*data != 0)
  {
    char* t;
    SectionData* section = SECTION_DATA[sectionIndex++];
    if (skipToString(data, &t, "\"csv\">"))
    {
      data = skipString(t, "\"csv\">");
      for(U32 i=0;i < (SECTION_WIDTH * SECTION_HEIGHT); i++)
      {
        U32 v = 0;
        data = readUInt(data, &v);
        data = skipToDigit(data);
        section->level[i] = v;
      }
      printf("\n");
    }
    else
      break;
  }

  SECTION_DATA_COUNT = sectionIndex;
}

typedef struct
{
  AnimatedSpriteObject sprite;
  S32                  velocityX, velocitY;
  U8                   objectType;
} Object;

typedef struct
{
  U32 seed, id;
  U8  sectionId;
  U8  objectCount;
  Object objects[MAX_OBJECTS_PER_SECTION];
} Section;

typedef struct
{
  U32 seed;
  U32 levelRandom, objectRandom;
  U32 baseX, cameraOffset;
  U32 id;
  U32 nextSectionId;
  Section sections[SECTIONS_PER_LEVEL];
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
  section->id = LEVEL->nextSectionId++;

  if (section->id < 3)
  {
    section->sectionId = 0;
  }
  else
  {
    section->sectionId = 1 + (Random(&section->seed) % (SECTION_DATA_COUNT- 1)); // @TODO
  }
}

void PushSection(Level* level, U32 seed)
{
  // Move section along
  for (U32 i=0;i < (SECTIONS_PER_LEVEL - 1);i++)
  {
    MoveSection(level, i + 1, i);
  }

  MakeSection(&level->sections[(SECTIONS_PER_LEVEL - 1)], seed);

}

void DrawSection(Section* section, S32 xOffset)
{
  for(S32 i=0;i < SECTION_WIDTH;i++)
  {
    for (S32 j=0;j < SECTION_HEIGHT;j++)
    {
      SectionData* data = SECTION_DATA[section->sectionId];

      U8 index = data->level[i + (j * SECTION_WIDTH)];
      if (index > 0)
      {
        Splat_Tile(&TILES1, xOffset + (i * TILE_SIZE), (j * TILE_SIZE), TILE_SIZE, index - 1);
      }
    }
  }
  Canvas_PrintF(xOffset, 10, &FONT_NEOSANS, 15, "%i:%i", section->id, section->sectionId);
}

void PushLevel(U32 seed)
{
  Scope_Push('LEVL');
  LEVEL = Scope_New(Level);
  memset(LEVEL, 0, sizeof(Level));

  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    MakeSection(&LEVEL->sections[i], Random(&LEVEL->seed));
  }

  printf("%i", LEVEL->sections[0].sectionId);

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
  S32 offset = GAME->cameraX % (SECTION_WIDTH * TILE_SIZE * 2);

  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    DrawSection(&LEVEL->sections[i], -offset + (i * (SECTION_WIDTH * TILE_SIZE)));
  }

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
  // This is 'different' from the game memory, it's more of a RAM, so it shouldn't be part of the Arena mem.
  for (int i=0; i < 256;i++)
  {
    SectionData* data = malloc(sizeof(SectionData));
    memset(data, 0, sizeof(SectionData));

    SECTION_DATA[i] = data;
  }

  LoadSectionData();

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
  
  Canvas_PrintF(0, 0, &FONT_NEOSANS, 15, "%i", GAME->cameraX);

  // Generate new section on boundary.
  for (int i=0;i < 8;i++)
  {
    GAME->cameraX++;

    S32 y = (SECTION_WIDTH * TILE_SIZE);
    S32 x = GAME->cameraX % y;
    if (x == 0)
    {
      GAME->cameraX -= y;
      PushSection(LEVEL, Random(&LEVEL->seed));
    }
  }

  DrawLevel();

  //Canvas_Splat(&TILES1, 0, 0, NULL);
  Canvas_Debug(&FONT_NEOSANS);
}
