#define RETRO_WINDOW_CAPTION "Cats"
#define RETRO_ARENA_SIZE Kilobytes(32)
#define RETRO_WINDOW_DEFAULT_WIDTH 1280
#define RETRO_WINDOW_DEFAULT_HEIGHT 480
#define RETRO_CANVAS_DEFAULT_WIDTH (RETRO_WINDOW_DEFAULT_WIDTH / 2)
#define RETRO_CANVAS_DEFAULT_HEIGHT (RETRO_WINDOW_DEFAULT_HEIGHT / 2)
#define MAX_CATS 8
#define MAX_JUMP_TIME 20

#define TILE_SIZE 16

#define SECTION_WIDTH 22  //(RETRO_CANVAS_DEFAULT_WIDTH / TILE_SIZE)
#define SECTION_HEIGHT 15 //(RETRO_CANVAS_DEFAULT_HEIGHT / TILE_SIZE)

#define LEVEL_WIDTH (SECTION_WIDTH * 2)
#define LEVEL_HEIGHT (SECTION_HEIGHT)
#define MAX_OBJECTS_PER_SECTION 16
#define SECTIONS_PER_LEVEL (4)

#define DEBUG_TILES 0
#define DEBUG_ARC 1
#define DEBUG_SCAN 1

#define FOCUS_X 48

#include "retro.c"

static Font           FONT_NEOSANS;
static Bitmap         SPRITESHEET;
static Bitmap         CATSHEET;
static Bitmap         PLAYERSHEET;
static Animation      ANIMATEDSPRITE_PLAYER_IDLE;
static Animation      ANIMATEDSPRITE_PLAYER_WALK;
static Animation      ANIMATEDSPRITE_CAT1_IDLE;
static Animation      ANIMATEDSPRITE_CAT1_WALK;
static Animation      ANIMATEDSPRITE_CAT2_IDLE;
static Animation      ANIMATEDSPRITE_CAT2_WALK;
static Animation      ANIMATEDSPRITE_CAT3_IDLE;
static Animation      ANIMATEDSPRITE_CAT3_WALK;
static Animation      ANIMATEDSPRITE_CAT_SHADOW_IDLE;
static Animation      ANIMATEDSPRITE_CAT_SHADOW_WALK;
static Sound          SOUND_JUMP1;
static Sound          SOUND_JUMP2;
static Sound          SOUND_HURT1;
static Sound          SOUND_HURT2;
static Sound          SOUND_PICKUP1;
static Sound          SOUND_PICKUP2;
static Sound          SOUND_SELECT;
static Bitmap         TILES1;


typedef enum 
{
  AC_JUMP,
  AC_RAGDOLL,
  AC_PAUSE,
  AC_CONFIRM,
  AC_CANCEL,
  AC_RESET,
  AC_DEBUG_LEFT,
  AC_DEBUG_RIGHT
} Actions;

void Init(Settings* settings)
{
  Palette_Make(&settings->palette);
  Palette_LoadFromBitmap("palette.png", &settings->palette);

  Input_BindKey(SDL_SCANCODE_SPACE, AC_JUMP);
  Input_BindKey(SDL_SCANCODE_LSHIFT, AC_RAGDOLL);
  Input_BindKey(SDL_SCANCODE_R, AC_RESET);
  Input_BindKey(SDL_SCANCODE_RETURN, AC_CONFIRM);
  Input_BindKey(SDL_SCANCODE_A, AC_DEBUG_LEFT);
  Input_BindKey(SDL_SCANCODE_D, AC_DEBUG_RIGHT);

  Font_Load("NeoSans.png", &FONT_NEOSANS, Colour_Make(0,0,255), Colour_Make(255,0,255));
  Bitmap_Load("cave.png", &SPRITESHEET, 0);
  Bitmap_Load("cats.png", &CATSHEET, 16);
  Bitmap_Load("player.png", &PLAYERSHEET, 16);

  Animation_LoadHorizontal(&ANIMATEDSPRITE_PLAYER_IDLE, &PLAYERSHEET, 1, 100, 0, 0, 46, 50);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_PLAYER_WALK, &PLAYERSHEET, 8, 120, 0, 150, 46, 50);

  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT1_IDLE, &CATSHEET, 1, 100, 0, 0, 18, 18);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT1_WALK, &CATSHEET, 3, 100, 0, 0, 18, 18);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT2_IDLE, &CATSHEET, 1, 100, 0, 18, 18, 18);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT2_WALK, &CATSHEET, 3, 100, 0, 18, 18, 18);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT3_IDLE, &CATSHEET, 1, 100, 0, 36, 18, 18);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT3_WALK, &CATSHEET, 3, 100, 0, 36, 18, 18);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT_SHADOW_IDLE, &CATSHEET, 1, 100, 0, 234, 18, 18);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT_SHADOW_WALK, &CATSHEET, 3, 100, 0, 234, 18, 18);

  Sound_Load(&SOUND_HURT1,   "hurt1.wav");
  Sound_Load(&SOUND_HURT2,   "hurt2.wav");
  Sound_Load(&SOUND_JUMP1,   "jump1.wav");
  Sound_Load(&SOUND_JUMP2,   "jump2.wav");
  Sound_Load(&SOUND_PICKUP1, "pickup1.wav");
  Sound_Load(&SOUND_PICKUP2, "pickup2.wav");
  Sound_Load(&SOUND_SELECT,  "select.wav");

  Bitmap_Load("tiles1.png", &TILES1, 16);

}

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
    src.x = (index % 16) * s;
    src.y = (index / 16) * s;
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
  PF_Idle,
  PF_Walking
} PlayerState;

typedef struct
{
  U8  col[SECTION_WIDTH * SECTION_HEIGHT];
  U8  non[SECTION_WIDTH * SECTION_HEIGHT];
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
  
  if (data == NULL)
    printf("Bad text file\n");

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

        if (v < 128)
        {
          section->col[i] = v;
          section->non[i] = 0;
        }
        else
        {
          section->col[i] = 0;
          section->non[i] = v;
        }
      }

      for(U32 j=0;j < (SECTION_HEIGHT); j++)
      {
        for(U32 i=0;i < (SECTION_WIDTH); i++)
        {
          printf("%i,", section->col[i + (j * SECTION_WIDTH)]);
        }
        printf("\n");
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
  U8  id, x, y;
  S32 wx, wy, sx, sy;
  bool exactCollision;
  U32 tile;
} CollisionPoint;

typedef struct
{
  S32                  jsX, jsY, jcX, jcY, jeX, jeY, jlX, jlY, jType;
  U32                  jT, jTMax, jTStop;
  bool                 valid;
} Jump;

typedef struct
{
  AnimatedSpriteObject sprite;
  U8                   objectType;
  U8                   jumpStrength;
  bool                 wantsToJump, isJumping;
  Jump                 jump, testJump;
  CollisionPoint       feet, front, diag, scan, scan2;
  U8                   w, h;
  S32                  x, y;
  bool                 alive;
} Object;

typedef struct
{
  U32 seed, id;
  U8  sectionId;
  U8  objectCount;
  S32 x0, x1, w0, w1;
  Object objects[MAX_OBJECTS_PER_SECTION];
} Section;

typedef struct
{
  U32 seed;
  U32 levelRandom, objectRandom;
  U32 speed;
  S32 frameMovementInput;
  U32 id;
  U32 nextSectionId;
  Object  playerObjects[1 + MAX_CATS]; // 0 = Player, 1+ = Cats
  Section sections[SECTIONS_PER_LEVEL];
  S32 camera, catCentre, catCentreMin, catCentreMax;
  U32 catCount;
  U32 jumpCount;
} Level;

typedef struct
{
  U32 seed;
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

void DrawJump(Jump* jump, U8 r, U8 g, U8 b);

void CalculateJump(Object* playerObject, Jump* jump);

bool IsJumpClear(Object* obj, Jump* jump);

int GetTile(U8 section, U32 x, U32 y)
{
  if (section >= SECTIONS_PER_LEVEL)
    return -1;

  U32 offset = x + (y * SECTION_WIDTH);
  
  if (offset >= (SECTION_WIDTH * SECTION_HEIGHT))
    return -1;

  Section* s = &LEVEL->sections[section];
  SectionData* d = SECTION_DATA[s->sectionId];

  return d->col[offset];
}

void ScreenPos_ToTilePos(S32 x, S32 y, U8* id, U8* tileX, U8* tileY)
{
  S32 lx = x; // - LEVEL->camera;

  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    Section* section = &LEVEL->sections[i];

    if (lx >= section->x0 && lx < section->x1)
    {
      (*id) = i;

      S32 tx = (lx - section->x0);
      (*tileX) = tx / TILE_SIZE;
      (*tileY) = y / TILE_SIZE;

      return;
    }
  }
}

bool TilePos_ToScreenPos(U8 sectionId, U8 tileX, U8 tileY, S32* screenX, S32* screenY)
{
  if (sectionId >= SECTIONS_PER_LEVEL)
    return false;

  Section* section = &LEVEL->sections[sectionId];

  (*screenX) = (section->w0 + (tileX * TILE_SIZE)) - LEVEL->camera;
  (*screenY) = (tileY * TILE_SIZE);

  return true;
}

S32 HeightAt(S32 x)
{
  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    Section* section = &LEVEL->sections[i];

    if (x >= section->w0 && x < section->x1)
    {
      S32 lx = x - LEVEL->camera;


      S32 tx = (lx - section->x0);

      tx /= TILE_SIZE;

      for (U32 y=0;y < SECTION_HEIGHT;y++)
      {
        int tile = GetTile(i, tx, y);
        if (tile > 0)
          return y * TILE_SIZE;
      }

      return SECTION_HEIGHT * TILE_SIZE;
    }
  }
  return SECTION_HEIGHT * TILE_SIZE;
}


void CollisionTest(U32 x, U32 y, U8 w, U8 h, CollisionPoint* point)
{
  // Tile test
  ScreenPos_ToTilePos(x, y, &point->id, &point->x, &point->y);

  point->tile = GetTile(point->id, point->x, point->y);
  point->wx = x;
  point->wy = y;

  TilePos_ToScreenPos(point->id, point->x, point->y, &point->sx, &point->sy);

  // @TODO Exact test.
}

void CollisionTest2(U8 sectionId, U8 tileX, U8 tileY, CollisionPoint* point)
{
  point->id = sectionId;
  point->x = tileX;

  if (point->x == 22)
  {
    point->id = sectionId++;
    point->x  = 0;
  }

  point->y = tileY;

  point->tile = GetTile(point->id, point->x, point->y);
}

void CollisionFind(Object* obj)
{
  CollisionTest(obj->x, obj->y + obj->h, obj->w, obj->h, &obj->feet);
  CollisionTest(obj->x + obj->w, obj->y, obj->w, obj->h, &obj->front);
  CollisionTest2(obj->feet.id, obj->feet.x + 1, obj->feet.y, &obj->diag);
}

void MoveSection(Level* level, U8 from, U8 to)
{
  memcpy(&level->sections[to], &level->sections[from], sizeof(Section));
}

void MakeSection(Section* section, U32 seed)
{
  // find highest x1, that becomes x0
  S32 x0=0, x1 = 0;
  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    Section* s = &LEVEL->sections[i];
    if (s->x1 > x0)
      x0 = s->x1;
  }

  x1 = x0 + (SECTION_WIDTH * TILE_SIZE);

  memset(section, 0, sizeof(Section));
  section->seed = seed;
  section->id = LEVEL->nextSectionId++;
  section->x0 = x0;
  section->x1 = x1;
  section->w0 = x0 - LEVEL->camera;
  section->w1 = x1 - LEVEL->camera;

  if (section->id < 1)
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

void DrawSection(Section* section, int idx)
{

  S32 xOffset = section->w0;

  for(S32 i=0;i < SECTION_WIDTH;i++)
  {
    for (S32 j=0;j < SECTION_HEIGHT;j++)
    {
      SectionData* data = SECTION_DATA[section->sectionId];

      U8 index = data->col[i + (j * SECTION_WIDTH)];
      if (index > 0)
      {
        Splat_Tile(&TILES1, xOffset + (i * TILE_SIZE), (j * TILE_SIZE), TILE_SIZE, index - 1);
      }

      index = data->non[i + (j * SECTION_WIDTH)];
      if (index > 0)
      {
        Splat_Tile(&TILES1, xOffset + (i * TILE_SIZE), (j * TILE_SIZE), TILE_SIZE, index - 1);
      }

    }
  }
  // Canvas_PrintF(xOffset, 10, &FONT_NEOSANS, 15, "%i/%i:%i", section->id, idx, section->sectionId);
}

void AddPlayerCat()
{
  Object* player = &LEVEL->playerObjects[0];

  for(U32 i=1;i < MAX_CATS;i++)
  {
    if (LEVEL->playerObjects[i].alive == false)
    {
      Object* cat = &LEVEL->playerObjects[i];

      cat->w = 18;
      cat->h = 12;
      cat->alive = true;

      U8 type = (Random(&LEVEL->seed) % 3);

      switch(type)
      {
        default:
        case 0:
          AnimatedSpriteObject_Make(&cat->sprite, &ANIMATEDSPRITE_CAT1_WALK, player->x, player->y);
        break;
        case 1:
          AnimatedSpriteObject_Make(&cat->sprite, &ANIMATEDSPRITE_CAT2_WALK, player->x, player->y);
        break;
        case 2:
          AnimatedSpriteObject_Make(&cat->sprite, &ANIMATEDSPRITE_CAT3_WALK, player->x, player->y);
        break;
      }

      AnimatedSpriteObject_PlayAnimation(&cat->sprite, true, true);

      cat->x += 32 +  Random(&LEVEL->seed) % 64;

      return;
    }
  }
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

  LEVEL->speed = 1;

  Object* player = &LEVEL->playerObjects[0];

  // Build player here.
  AnimatedSpriteObject_Make(&player->sprite, &ANIMATEDSPRITE_PLAYER_WALK, Canvas_GetWidth() / 2, Canvas_GetHeight() / 2);
  AnimatedSpriteObject_PlayAnimation(&player->sprite, true, true);

  player->x = FOCUS_X;
  player->y = 48;
  player->x = 0;
  player->y = 0;
  player->w = 48;
  player->h = 48;
  player->alive = true;

  // Build cat player here.
  AddPlayerCat();
  AddPlayerCat();
  AddPlayerCat();
  AddPlayerCat();

  LEVEL->camera = 0;
}

void PopLevel()
{
  Scope_Pop();
  LEVEL = NULL;
  printf("Level Reset");
}

void DrawTile(U8 id, U32 x, U32 y, U8 tile)
{
  int of = LEVEL->sections[id].w0;
  int cx = x * TILE_SIZE;
  int cy = y * TILE_SIZE;

  Splat_Tile(&TILES1, of + cx, cy, TILE_SIZE, tile);
}

void DrawLevel()
{
  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    DrawSection(&LEVEL->sections[i], i);
  }
}


void Start()
{
  // This is 'different' from the game memory, it's more of a ROM, so it shouldn't be part of the Arena mem.
  for (int i=0; i < 256;i++)
  {
    SectionData* data = malloc(sizeof(SectionData));
    memset(data, 0, sizeof(SectionData));

    SECTION_DATA[i] = data;
  }

  LoadSectionData();

  GAME = Scope_New(Game);
  GAME->seed = 1;


  Canvas_SetFlags(0, CNF_Render | CNF_Clear, 8);
  /// Music_Play("hiro4.mod");
}

void jumpCurve(Jump* jump, U32 t, S32* x, S32* y)
{
  float T = (float) t / (float) jump->jTMax;
  float xf = (powf(1.0f - T, 2.0f) * (float) jump->jsX) + (2.0f * (1.0f - T) * T * (float) jump->jcX) + (powf(T, 2.0f) * (float) jump->jeX);
  float yf = (powf(1.0f - T, 2.0f) * (float) jump->jsY) + (2.0f * (1.0f - T) * T * (float) jump->jcY) + (powf(T, 2.0f) * (float) jump->jeY);

  *x = (S32) xf;
  *y = (S32) yf;
}

void DrawJump(Jump* jump, U8 r, U8 g, U8 b)
{
  SDL_SetRenderDrawColor(gRenderer, r, g, b, 0xFF);

  S32 x, y, lx, ly;
  jumpCurve(jump, 0, &lx, &ly);
  for(U32 k=1;k < jump->jTStop;k++)
  {
    jumpCurve(jump, k, &x, &y);
    SDL_RenderDrawLine(gRenderer, x - LEVEL->camera, y, lx - LEVEL->camera, ly);
    lx = x;
    ly = y;
  }
  SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
}

void CalculateJump(Object* playerObject, Jump* jump)
{
  jump->jsX = playerObject->x;
  jump->jsY = playerObject->y;

  bool haveValidPoint = false;

  if (playerObject->scan.tile > 0)
  {
    S32 scanX, scanY;
    CollisionPoint scan = playerObject->scan;
    if (TilePos_ToScreenPos(scan.id, scan.x, scan.y, &scanX, &scanY))
    {
      haveValidPoint = true;
      // scanX += playerObject->w / 2;

      jump->jeX = scan.wx;
      jump->jeY = scan.wy;

      //printf("%i => %i, %i to %i, %i\n", i,  playerObject->jsX,  playerObject->jsY, scanX, scanY);
    }
  }

  if (haveValidPoint == false)
  {
    //printf("%i => Non valid point\n", i);
    jump->jeX = jump->jsX + 60;
    jump->jeY = HeightAt(jump->jeX);
  }

  // @TODO We can alter this based upon speed, so the curve moves forwards or backwards, and up/down based on cat profile/speed.

  //jump->jeY -= playerObject->h;

  jump->jcX = jump->jsX + ((jump->jeX - jump->jsX) / 2);
  jump->jcY = jump->jsY + ((jump->jeY - jump->jsY) / 2);
  jump->jcY -= 16 * 4;

  jump->jTMax = 30;
  jump->jTStop = jump->jTMax;
  jump->jT = 0;
  jump->jType = 0;
  jump->valid = false;
}

bool LimitJump(Object* obj, Jump* jump)
{
  S32 x, y;
  for(U32 k=0;k < jump->jTMax;k++)
  {
    jumpCurve(jump, k, &x, &y);

    CollisionTest(x, y, obj->w, obj->h, &obj->scan2);

    if (obj->scan2.tile > 0)
    {
      jump->jTStop = k;
      return true;
    }
  }
  return false;
}

bool IsJumpClear(Object* obj, Jump* jump)
{
  S32 x, y;
  for(U32 k=0;k < jump->jTMax;k++)
  {
    jumpCurve(jump, k, &x, &y);

    CollisionTest(x, y, obj->w, obj->h, &obj->scan2);

    if (obj->scan2.tile > 0)
    {
      return false; // Hit something.
    }
  }

  return true;
}

#define DEG2RAD (3.14f / 180.0f)
#define RAD2DEG (180.0f / 3.14f)

void ScanJump(Object* obj, U32 maxLength)
{
  float maxLengthF = maxLength;

  int ox = obj->x + (obj->w / 2);
  int oy = obj->y + (obj->h / 2) - 1;

  U32   angleSteps = 4;
  U32   lengthSteps = 8;
  float lengthDelta = maxLengthF / (float) lengthSteps;
  float delta = 10.0f * DEG2RAD;
  float deltaOffset = -15.0f * DEG2RAD;

  for(U32 i=0;i < angleSteps;i++)
  {
    // for(U32 j=1;j < lengthSteps;j++)
    {
      float length = maxLengthF; // maxLengthF - (lengthDelta * j);

      int tx = ox + (cos(deltaOffset + (delta * i)) * length);
      int ty = oy + (sin(deltaOffset + (delta * i)) * length);
      int by = 0;
      
      for (int k=0;k < SECTION_HEIGHT;k++)
      {
        by = (ty / TILE_SIZE) + k;
        by *= TILE_SIZE;

        CollisionTest(tx, by, obj->w, obj->h, &obj->scan2);
        if (obj->scan2.tile > 0)
          break;
      }

      #if 0
        SDL_RenderDrawLine(gRenderer, ox - LEVEL->camera, oy, tx - LEVEL->camera, ty);
        SDL_RenderDrawLine(gRenderer, tx - LEVEL->camera, ty, tx - LEVEL->camera, by);
      #endif

      CollisionTest(tx, by, obj->w, obj->h, &obj->scan);

      CalculateJump(obj, &obj->testJump);

      if (LimitJump(obj, &obj->testJump))
      {
        DrawJump(&obj->testJump, 208, 70, 72);
      }
      else
      {
        DrawJump(&obj->testJump, 210, 125, 44);
      }

      obj->testJump.valid = true;

      return;
    }
  }

}

bool HandlePlayerObject(Object* playerObject, bool isPlayer, bool reduce, S32 catCentre)
{
  CollisionFind(playerObject);

  bool isAlive = true;
  bool didSomething = false;

  // In air - Gravity
  if (playerObject->feet.tile <= 0)
  {
    if (!playerObject->isJumping)
    {
      playerObject->y += 4;
    }
  }

  // Jumping
  if (playerObject->isJumping)
  {
    S32 x, y;
    jumpCurve(&playerObject->jump, playerObject->jump.jT, &x, &y);
    playerObject->jump.jlX = playerObject->x;
    playerObject->jump.jlY = playerObject->y;

    y -= playerObject->h;

    playerObject->x = x;
    playerObject->y = y;

    playerObject->jump.jT++;
    
    if (playerObject->jump.jType == 0 && playerObject->jump.jT >= playerObject->jump.jTStop)
    {
      playerObject->isJumping = false;
    }
    else if (playerObject->jump.jType == 1 && playerObject->jump.jT >= playerObject->jump.jTMax / 2)
    {
      playerObject->isJumping = false;
    }
  }
  else
  {
    if (isPlayer)
    {
      if (playerObject->x > LEVEL->catCentre)
        playerObject->x -= 2;
      else if (playerObject->x < LEVEL->catCentre)
        playerObject->x += 2;
      else
        playerObject->x++;
    }
    else
    {
      if (reduce)
      {
        if (playerObject->x > catCentre)
          playerObject -= 2;
        else if (playerObject->x < catCentre)
          playerObject += 2;
        else
          playerObject->x++;
      }
      else
      {
        playerObject->x++;
      }
    }
  }

  // Out of bounds check
  if (isAlive && playerObject->y + playerObject->h >= RETRO_CANVAS_DEFAULT_HEIGHT)
  {
    printf("Out of bounds\n");
    isAlive = false;
  }

  S32 k = (playerObject->x - LEVEL->camera);
  if (isAlive && (playerObject->x - LEVEL->camera) < -playerObject->w)
  {
    printf("Left behind! k=%i x=%i cam=%i\n", k, playerObject->x, LEVEL->camera);
    isAlive = false;
  }

  // Front Collision check
  if (isAlive && !isPlayer && playerObject->front.tile > 0)
  {
    // @TODO Better collisions
    printf("Front collision\n");
    isAlive = false;
  }

  if (isAlive == false)
  {
    printf("Cat died\n");
  }

  return isAlive;
}

void CalculateCentreX()
{
  LEVEL->catCentre = 0;
  LEVEL->catCentreMin = 10000000;
  LEVEL->catCentreMax = -10000000;

  LEVEL->catCount = 0;
  for (U32 i=1;i < (MAX_CATS + 1);i++)
  {
    Object* playerObject = &LEVEL->playerObjects[i];
    if (!playerObject->alive)
      continue;


    int sx = playerObject->x - LEVEL->camera;
    LEVEL->catCount++;
    if (sx > 0)
    {
      if (playerObject->x < LEVEL->catCentreMin)
        LEVEL->catCentreMin = playerObject->x;
      else if (playerObject->x > LEVEL->catCentreMax)
        LEVEL->catCentreMax = playerObject->x;

    }
    
  }

  LEVEL->catCentre = LEVEL->catCentreMin + (LEVEL->catCentreMax - LEVEL->catCentreMin) / 2;
}

void UpdateCamera()
{
  CalculateCentreX();
  LEVEL->camera = LEVEL->catCentre - FOCUS_X;
}

void Step()
{
  bool sfxJump = false, sfxHurt = false, sfxPickup = false;

  int scope = Scope_GetName();

  if (LEVEL == NULL)
  {
    PushLevel(Random(&GAME->seed));
  }

  if (scope == 'LEVL')
  {

    UpdateCamera();

    // Generate new section on boundary.

    for (U32 k=0;k < SECTIONS_PER_LEVEL;k++)
    {
      Section* section = &LEVEL->sections[k];
      section->w0 = section->x0 - LEVEL->camera;
      section->w1 = section->w0 + (SECTION_WIDTH * TILE_SIZE);

      if (section->w1 <= 0)
      {
        printf("%i => %i (Out of bounds)\n", k, section->x1);
        section->x0 = 0;
        section->x1 = 0;
        MakeSection(section, Random(&LEVEL->seed));
      }
    }

  }

  DrawLevel();

  if (scope == 'DEAT')
  {
    int s = 40;
    SDL_Rect r;
    r.x = s;
    r.y = s;
    r.w = Canvas_GetWidth() - s * 2;
    r.h = Canvas_GetHeight() - s * 2;

    SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderFillRect(gRenderer, &r);

    SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    r.x--;
    r.y--;
    r.w+=2;
    r.h+=2;
    SDL_RenderDrawRect(gRenderer, &r);
    
    U32 score = (LEVEL->camera * 5) + (LEVEL->jumpCount * 10);

    S32 gameoverLength = Canvas_LengthStr(&FONT_NEOSANS, "GAME OVER!");
    S32 scoreLength =  Canvas_LengthF(&FONT_NEOSANS, "Score: %08d", score);
    S32 instructionsLength = Canvas_LengthStr(&FONT_NEOSANS, "<RETURN> to play again");

    Canvas_PrintStr(r.x + r.w / 2 - gameoverLength / 2, r.y + 4, &FONT_NEOSANS, 15, "GAME OVER!");
    Canvas_PrintF(r.x + r.w / 2 - scoreLength / 2, r.y + r.h / 2 - 9, &FONT_NEOSANS, 15, "Score: %08d", score);

    Canvas_PrintStr(r.x + r.w / 2 - instructionsLength / 2, r.y + r.h - 10, &FONT_NEOSANS, 15, "<RETURN> to play again");


    if (Input_GetActionPressed(AC_CONFIRM))
    {
      Scope_Pop();
      PopLevel();
    }
    return;
  }


  if (Input_GetActionPressed(AC_JUMP))
  {
    for (U32 i=0;i < (MAX_CATS + 1);i++)
    {
      Object* playerObject = &LEVEL->playerObjects[i];
      if ((!playerObject->alive || playerObject->wantsToJump || playerObject->isJumping || playerObject->feet.tile <= 0))
        continue;

      playerObject->wantsToJump = true;
      playerObject->jumpStrength = 0;
    }
  }

  if (Input_GetActionDown(AC_JUMP))
  {
    for (U32 i=0;i < (MAX_CATS + 1);i++)
    {
      Object* playerObject = &LEVEL->playerObjects[i];
      if (!playerObject->alive || !playerObject->wantsToJump)
        continue;

      if (playerObject->jumpStrength < 60)
        playerObject->jumpStrength += 2;

    }
  }

  for (U32 i=0;i < (MAX_CATS + 1);i++)
  {
    Object* playerObject = &LEVEL->playerObjects[i];
    if (!playerObject->alive)
      continue;
    
    if (playerObject->wantsToJump)
    {
      S32 bonus = 0;
      if (playerObject->feet.tile <= 0)
      {
        bonus = TILE_SIZE * LEVEL->speed;
      }

      ScanJump(playerObject, (TILE_SIZE * 4) + playerObject->jumpStrength + bonus);
    }
  }

  for (U32 i=0;i < (MAX_CATS + 1);i++)
  {
    Object* playerObject = &LEVEL->playerObjects[i];
    if (!playerObject->alive  || playerObject->isJumping)
      continue;

    if (Input_GetActionReleased(AC_JUMP) || (playerObject->wantsToJump && playerObject->feet.tile <= 0))
    {
      if (playerObject->testJump.valid)
      {
        playerObject->isJumping = true;
        playerObject->wantsToJump = false;
        playerObject->jumpStrength = 0;

        playerObject->jump = playerObject->testJump;

        sfxJump = true;
      }
      else
      {
        playerObject->jumpStrength = 0;
        playerObject->isJumping = false;
        playerObject->wantsToJump = false;
      }
    }
  }

  S32 catCentre = LEVEL->catCentreMin + (LEVEL->catCentreMax - LEVEL->catCentreMin);
  bool reduce = (LEVEL->catCentreMax - LEVEL->catCentreMin) > 100;

  if (reduce)
  {
    printf("Reduce = %i\n", (LEVEL->catCentreMax - LEVEL->catCentreMin));
  }


  for (U32 i=0;i < (MAX_CATS + 1);i++)
  {
    Object* playerObject = &LEVEL->playerObjects[i];
    if (!playerObject->alive)
      continue;

    if (HandlePlayerObject(playerObject, i == 0, reduce, catCentre) == false)
    {
      playerObject->alive = false;
      sfxHurt = true;
    }
  }

  // Draw
  Object* player = &LEVEL->playerObjects[0];
  S32 leashX0 = player->x + player->w / 2;
  S32 leashY0 = player->y + player->h / 2 + 4;
  
  for (U32 i=0;i < (MAX_CATS + 1);i++)
  {
    Object* playerObject = &LEVEL->playerObjects[i];

    playerObject->sprite.x = playerObject->x - LEVEL->camera;
    playerObject->sprite.y = playerObject->y;
    

    Canvas_PlaceAnimated(&playerObject->sprite, playerObject->alive);

    if (i > 0 && playerObject->alive)
    {
      S32 leashX1 = playerObject->x + 12;
      S32 leashY1 = playerObject->y + 4;

      SDL_SetRenderDrawColor(gRenderer, 20, 12, 28, 0xFF);
      SDL_RenderDrawLine(gRenderer, leashX0 - LEVEL->camera, leashY0, leashX1 - LEVEL->camera, leashY1);
      SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    #if DEBUG_ARC == 1
    if (playerObject->isJumping)
    {
      //DrawJump(&playerObject->jump, 0x55, 0xFF, 0x55);
      //DrawJump(&playerObject->testJump, 0xFF, 0x55, 0x55);
    }
    #endif

    #if DEBUG_TILES == 1
    S32 debugX = LEVEL->sections[playerObject->diag.id].w0 + (playerObject->diag.x * TILE_SIZE);
    S32 debugY = (playerObject->diag.y * TILE_SIZE);

    if (playerObject->diag.tile <= 0)
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0x55, 0x55, 0xFF);
    else
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0xFF, 0x55, 0xFF);
    
    SDL_RenderDrawLine(gRenderer, debugX, debugY, debugX + TILE_SIZE, debugY + TILE_SIZE);

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);


    debugX = LEVEL->sections[playerObject->feet.id].w0 + (playerObject->feet.x * TILE_SIZE);
    debugY = (playerObject->feet.y * TILE_SIZE);

    if (playerObject->feet.tile <= 0)
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0x55, 0x55, 0xFF);
    else
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0xFF, 0x55, 0xFF);

    SDL_RenderDrawLine(gRenderer, debugX, debugY, debugX + TILE_SIZE, debugY);

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);

    debugX = LEVEL->sections[playerObject->front.id].w0 + (playerObject->front.x * TILE_SIZE);
    debugY = (playerObject->front.y * TILE_SIZE);

    if (playerObject->front.tile <= 0)
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0x55, 0x55, 0xFF);
    else
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0xFF, 0x55, 0xFF);

    SDL_RenderDrawLine(gRenderer, debugX, debugY, debugX, debugY + TILE_SIZE);

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);

    debugX = LEVEL->sections[playerObject->scan.id].w0 + (playerObject->scan.x * TILE_SIZE);
    debugY = (playerObject->scan.y * TILE_SIZE);

    if (playerObject->scan.tile <= 0)
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0x55, 0x55, 0xFF);
    else
      SDL_SetRenderDrawColor(gRenderer, 0x55, 0xFF, 0x55, 0xFF);

    SDL_RenderDrawLine(gRenderer, debugX + TILE_SIZE, debugY, debugX, debugY + TILE_SIZE);

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);

    #endif
  }

  U32 score = (LEVEL->camera * 5) + (LEVEL->jumpCount * 10);

  Canvas_PrintF(0, 0, &FONT_NEOSANS, 15, "%08d", score);

  for (U32 i=0;i < LEVEL->catCount;i++)
  {
    Splat_Tile(&TILES1, i * 14, 9, 16, 0xFF);
  }

  // Out of cats death
  bool hasCats = false;
  for (U32 i=1;i < (MAX_CATS + 1);i++)
  {
    if (LEVEL->playerObjects[i].alive)
    {
      hasCats = true;
      break;
    }
  }

  if (sfxJump)
  {
    LEVEL->jumpCount++;
    Sound_Play(Random(&GAME->seed) % 2 == 0 ? &SOUND_JUMP1 : &SOUND_JUMP2, SDL_MIX_MAXVOLUME);
  } 

  if (sfxHurt)
  {
    Sound_Play(Random(&GAME->seed) % 2 == 0 ? &SOUND_HURT1 : &SOUND_HURT2, SDL_MIX_MAXVOLUME);
  } 

  if (sfxPickup)
  {
    Sound_Play(Random(&GAME->seed) % 2 == 0 ? &SOUND_PICKUP1 : &SOUND_PICKUP2, SDL_MIX_MAXVOLUME);
  } 

  // Cats dead or player dead.
  if (!hasCats || !LEVEL->playerObjects[0].alive)
  {
    Scope_Push('DEAT');
  }

  //Canvas_Splat(&TILES1, 0, 0, NULL);
  Canvas_Debug(&FONT_NEOSANS);
}
