#define RETRO_WINDOW_CAPTION "Cats"
#define RETRO_ARENA_SIZE Kilobytes(8)
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

#include "retro.c"

static Font           FONT_NEOSANS;
static Bitmap         SPRITESHEET;
static Animation      ANIMATEDSPRITE_PLAYER_IDLE;
static Animation      ANIMATEDSPRITE_PLAYER_WALK;
static Animation      ANIMATEDSPRITE_CAT_IDLE;
static Animation      ANIMATEDSPRITE_CAT_WALK;
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
  AC_DEBUG_LEFT,
  AC_DEBUG_RIGHT
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

      for(U32 j=0;j < (SECTION_HEIGHT); j++)
      {
        for(U32 i=0;i < (SECTION_WIDTH); i++)
        {
          printf("%i,", section->level[i + (j * SECTION_WIDTH)]);
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
  bool exactCollision;
  U32 tile;
} CollisionPoint;

typedef struct
{
  AnimatedSpriteObject sprite;
  U8                   objectType;
  U8                   jumpStrength;
  bool                 wantsToJump, isJumping;
  S32                  jsX, jsY, jcX, jcY, jeX, jeY, jlX, jlY, jType;
  U32                  jT, jTMax;
  CollisionPoint       feet, front, diag;
  U8                   w, h;
  bool                 alive;
} Object;

typedef struct
{
  U32 seed, id;
  U8  sectionId;
  U8  objectCount;
  S32 x0, x1;
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

int GetTile(U8 section, U32 x, U32 y)
{
  if (section >= SECTIONS_PER_LEVEL)
    return -1;

  U32 offset = x + (y * SECTION_WIDTH);
  
  if (offset >= (SECTION_WIDTH * SECTION_HEIGHT))
    return -1;

  Section* s = &LEVEL->sections[section];
  SectionData* d = SECTION_DATA[s->sectionId];

  return d->level[offset];
}

void ScreenPos_ToTilePos(S32 x, S32 y, U8* id, U8* tileX, U8* tileY)
{

  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    Section* section = &LEVEL->sections[i];

    if (x >= section->x0 && x < section->x1)
    {
      (*id) = i;

      S32 tx = (x - section->x0);
      (*tileX) = tx / TILE_SIZE;
      (*tileY) = y / TILE_SIZE;

      return;
    }
  }
}

S32 HeightAt(S32 x)
{
  for (U32 i=0;i < SECTIONS_PER_LEVEL;i++)
  {
    Section* section = &LEVEL->sections[i];

    if (x >= section->x0 && x < section->x1)
    {
      S32 tx = (x - section->x0);

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

  // @TODO Exact test.

}

void CollisionFind(Object* obj)
{
  CollisionTest(obj->sprite.x, obj->sprite.y + obj->h, obj->w, obj->h, &obj->feet);
  CollisionTest(obj->sprite.x + obj->w, obj->sprite.y, obj->w, obj->h, &obj->front);
  CollisionTest(obj->sprite.x + obj->w, obj->sprite.y + obj->h, obj->w, obj->h, &obj->diag);
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

  S32 xOffset = section->x0;

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
  Canvas_PrintF(xOffset, 10, &FONT_NEOSANS, 15, "%i/%i:%i", section->id, idx, section->sectionId);
}

void AddPlayerCat()
{
  Object* player = &LEVEL->playerObjects[0];

  for(U32 i=1;i < MAX_CATS;i++)
  {
    if (LEVEL->playerObjects[i].alive == false)
    {
      Object* cat = &LEVEL->playerObjects[i];

      cat->w = 16;
      cat->h = 5;
      cat->alive = true;

      AnimatedSpriteObject_Make(&cat->sprite, &ANIMATEDSPRITE_CAT_WALK, player->sprite.x, player->sprite.y);
      AnimatedSpriteObject_PlayAnimation(&cat->sprite, true, true);

      cat->sprite.x += 8 + Random(&LEVEL->seed) % 48;

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

  player->sprite.x = 48;
  player->sprite.y = 48;
  player->w = 16;
  player->h = 16;
  player->alive = true;

  // Build cat player here.
  AddPlayerCat();
  AddPlayerCat();
  AddPlayerCat();
  AddPlayerCat();
}

void PopLevel()
{
  Scope_Pop();
  LEVEL = NULL;
}

void DrawTile(U8 id, U32 x, U32 y, U8 tile)
{
  int of = LEVEL->sections[id].x0;
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

void Init(Settings* settings)
{
  Palette_Make(&settings->palette);
  Palette_LoadFromBitmap("palette.png", &settings->palette);

  Input_BindKey(SDL_SCANCODE_SPACE, AC_JUMP);
  Input_BindKey(SDL_SCANCODE_LSHIFT, AC_RAGDOLL);
  Input_BindKey(SDL_SCANCODE_R, AC_RESET);
  Input_BindKey(SDL_SCANCODE_A, AC_DEBUG_LEFT);
  Input_BindKey(SDL_SCANCODE_D, AC_DEBUG_RIGHT);

  Font_Load("NeoSans.png", &FONT_NEOSANS, Colour_Make(0,0,255), Colour_Make(255,0,255));
  Bitmap_Load("cave.png", &SPRITESHEET, 0);

  Animation_LoadHorizontal(&ANIMATEDSPRITE_PLAYER_IDLE, &SPRITESHEET, 1, 100, 0, 80, 16, 16);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_PLAYER_WALK, &SPRITESHEET, 4, 120, 0, 80, 16, 16);

  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT_IDLE, &SPRITESHEET, 1, 100, 0, 91, 16, 5);
  Animation_LoadHorizontal(&ANIMATEDSPRITE_CAT_WALK, &SPRITESHEET, 4, 120, 0, 91, 16, 5);

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

  // Music_Play("origin.mod");
}

void jumpCurve(Object* playerObject, U32 t, S32* x, S32* y)
{
  float T = (float) t / (float) playerObject->jTMax;
  float xf = (powf(1.0f - T, 2.0f) * (float) playerObject->jsX) + (2.0f * (1.0f - T) * T * (float) playerObject->jcX) + (powf(T, 2.0f) * (float) playerObject->jeX);
  float yf = (powf(1.0f - T, 2.0f) * (float) playerObject->jsY) + (2.0f * (1.0f - T) * T * (float) playerObject->jcY) + (powf(T, 2.0f) * (float) playerObject->jeY);

  *x = (S32) xf;
  *y = (S32) yf;
}

bool HandlePlayerObject(Object* playerObject, bool isPlayer)
{
  CollisionFind(playerObject);

  bool isAlive = true;

  // In air - Gravity
  if (playerObject->feet.tile <= 0)
  {
  #if 0
    // Holding jump over edge
    if (playerObject->wantsToJump && playerObject->jumpTime > 10)
    {
      if (playerObject->jumpTime > MAX_JUMP_TIME)
        playerObject->jumpTime = MAX_JUMP_TIME;

      playerObject->isJumping = true;
      playerObject->wantsToJump = false; 
      playerObject->jumpForward += 4;
    }

    if (playerObject->isJumping && playerObject->jumpTime <= 0)
    {
      playerObject->sprite.y += 4;
      playerObject->isJumping = false;

      if (playerObject->forwardFalling)
      {
        playerObject->sprite.x += 1;
      }

    }
    else 
    #endif
    if (!playerObject->isJumping)
    {
      playerObject->sprite.y += 4;
    }
  }

  // Jumping
  if (playerObject->isJumping)
  {
    S32 x, y;
    jumpCurve(playerObject, playerObject->jT, &x, &y);
    playerObject->jlX = playerObject->sprite.x;
    playerObject->jlY = playerObject->sprite.y;

    playerObject->sprite.x = x;
    playerObject->sprite.y = y;

    playerObject->jT++;
    
    if (playerObject->jType == 0 && playerObject->jT >= playerObject->jTMax)
    {
      playerObject->isJumping = false;
    }
    else if (playerObject->jType == 1 && playerObject->jT >= playerObject->jTMax / 2)
    {
      playerObject->isJumping = false;
    }

  }

  // Out of bounds check
  if (isAlive && playerObject->sprite.y + playerObject->h >= RETRO_CANVAS_DEFAULT_HEIGHT)
  {
    printf("Out of bounds\n");
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

  LEVEL->frameMovementInput = 0;
  if (Input_GetActionDown(AC_DEBUG_LEFT))
  {
    LEVEL->frameMovementInput -= 4;
  }

  if (Input_GetActionDown(AC_DEBUG_RIGHT))
  {
    LEVEL->frameMovementInput += 4;
  }

  if (Input_GetActionPressed(AC_JUMP))
  {
    for (U32 i=0;i < (MAX_CATS + 1);i++)
    {
      Object* playerObject = &LEVEL->playerObjects[i];
      if ((!playerObject->alive || playerObject->isJumping))
        continue;

      if (playerObject->feet.tile <= 0)
        continue;

      playerObject->wantsToJump = true;

      if (i > 0)
      {
        playerObject->jumpStrength = (Random(&LEVEL->seed) % 3 == 0) ? 0 : 1;
      }
      else
      {
        playerObject->jumpStrength = 1;
      }
    }
  }

  if (Input_GetActionDown(AC_JUMP))
  {
    for (U32 i=0;i < (MAX_CATS + 1);i++)
    {
      Object* playerObject = &LEVEL->playerObjects[i];
      if (!playerObject->alive)
        continue;

      if ((playerObject->jumpStrength < MAX_JUMP_TIME) && playerObject->wantsToJump)
      {
        if (i > 0)
        {
          playerObject->jumpStrength += 1 + (Random(&LEVEL->seed) % 3);
        }
        else
        {
          playerObject->jumpStrength += 1;
        }

      }
    }
  }

  if (Input_GetActionReleased(AC_JUMP))
  { 
    for (U32 i=0;i < (MAX_CATS + 1);i++)
    {
      Object* playerObject = &LEVEL->playerObjects[i];
      if (!playerObject->alive  || playerObject->isJumping)
        continue;

      if (playerObject->wantsToJump )
      {
        if (playerObject->jumpStrength > MAX_JUMP_TIME)
          playerObject->jumpStrength = MAX_JUMP_TIME;

        playerObject->isJumping = true;
        playerObject->wantsToJump = false;

        playerObject->jsX = playerObject->sprite.x;
        playerObject->jsY = playerObject->sprite.y;
        
        playerObject->jeX = playerObject->jsX + 60;
        playerObject->jeY = HeightAt(playerObject->jeX) - playerObject->h;

        playerObject->jcX = playerObject->jsX + 30;
        playerObject->jcY = playerObject->jeY - 30;
        
        playerObject->jTMax = 30;
        playerObject->jT = 0;
        playerObject->jType = 0;

      }
    }
  }

  // Generate new section on boundary.
  for (U32 i=0;i < LEVEL->speed;i++)
  {
    for (U32 k=0;k < SECTIONS_PER_LEVEL;k++)
    {
      Section* section = &LEVEL->sections[k];
      section->x0--;
      section->x1 = section->x0 + (SECTION_WIDTH * TILE_SIZE);

      if (section->x1 <= 0)
      {
        section->x0 = 0;
        section->x1 = 0;
        MakeSection(section, Random(&LEVEL->seed));
      }
    }
  }

  for (U32 i=0;i < (MAX_CATS + 1);i++)
  {
    Object* playerObject = &LEVEL->playerObjects[i];
    if (!playerObject->alive)
      continue;

    if (HandlePlayerObject(playerObject, i == 0) == false)
    {
      playerObject->alive = false;
    }
  }
  
  DrawLevel();

  // Draw
  Object* player = &LEVEL->playerObjects[0];
  S32 leashX0 = player->sprite.x + player->w / 3;
  S32 leashY0 = player->sprite.y + player->h / 2;

  for (U32 i=0;i < (MAX_CATS + 1);i++)
  {
    Object* playerObject = &LEVEL->playerObjects[i];
    if (!playerObject->alive)
      continue;
    
    Canvas_PlaceAnimated(&playerObject->sprite, true);

    if (i > 0)
    {
      if (playerObject->feet.tile <= 0)
        SDL_SetRenderDrawColor(gRenderer, 0x55, 0x55, 0x55, 0xFF);
      else
        SDL_SetRenderDrawColor(gRenderer, 0x55, 0xFF, 0x55, 0xFF);
      SDL_RenderDrawLine(gRenderer, leashX0, leashY0, playerObject->sprite.x + playerObject->w / 2, playerObject->sprite.y);
      SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    if (playerObject->isJumping)
    {
      S32 x, y, lx, ly;
      jumpCurve(playerObject, 0, &lx, &ly);
      for(U32 k=1;k < playerObject->jTMax;k++)
      {
        jumpCurve(playerObject, k, &x, &y);
        SDL_RenderDrawLine(gRenderer, x, y, lx, ly);
        lx = x;
        ly = y;
      }
    }
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

  // Cats dead or player dead.
  if (!hasCats || !LEVEL->playerObjects[0].alive)
  {
    PopLevel();
  }

  //Canvas_Splat(&TILES1, 0, 0, NULL);
  Canvas_Debug(&FONT_NEOSANS);
}
