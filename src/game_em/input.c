/* 2000-08-13T15:29:40Z
 *
 * handle input from x11 and keyboard and joystick
 */

#include "main_em.h"


unsigned long RandomEM;

struct LEVEL lev;
struct PLAYER ply[MAX_PLAYERS];

short **Boom;
short **Cave;
short **Next;
short **Draw;

static short *Index[4][HEIGHT];
static short Array[4][HEIGHT][WIDTH];

extern int screen_x;
extern int screen_y;

struct EngineSnapshotInfo_EM engine_snapshot_em;

void game_init_vars(void)
{
  int x, y;

  RandomEM = 1684108901;

  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      Array[0][y][x] = ZBORDER;
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      Array[1][y][x] = ZBORDER;
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      Array[2][y][x] = ZBORDER;
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      Array[3][y][x] = Xblank;

  for (y = 0; y < HEIGHT; y++)
    Index[0][y] = Array[0][y];
  for (y = 0; y < HEIGHT; y++)
    Index[1][y] = Array[1][y];
  for (y = 0; y < HEIGHT; y++)
    Index[2][y] = Array[2][y];
  for (y = 0; y < HEIGHT; y++)
    Index[3][y] = Array[3][y];

  Cave = Index[0];
  Next = Index[1];
  Draw = Index[2];
  Boom = Index[3];
}

void InitGameEngine_EM()
{
  prepare_em_level();

  game_initscreen();
  game_animscreen();

#if 0
  /* blit playfield from scroll buffer to normal back buffer for fading in */
  BlitScreenToBitmap_EM(backbuffer);
#endif
}

void GameActions_EM(byte action[MAX_PLAYERS], boolean warp_mode)
{
  int i;

#if 0
  static int foo = -1;

  if (action[0] == 0 && foo != 0)
    printf("KEY RELEASED @ %05d\n", FrameCounter);

  foo = action[0];
#endif

#if 0
#if 1
  if (FrameCounter % 10 == 0)
#endif
    printf("::: %05d: %lu, %d\n", FrameCounter, RandomEM, frame);
#endif

#if 0
  game_animscreen();

#if 1
#if 0
  SyncDisplay();
#endif

  blitscreen();
#endif
#endif

  RandomEM = RandomEM * 129 + 1;

  frame = (frame - 1) & 7;

  for (i = 0; i < MAX_PLAYERS; i++)
    readjoy(action[i], &ply[i]);

  UpdateEngineValues(screen_x / TILEX, screen_y / TILEY);

  if (frame == 7)
  {
    synchro_1();
    synchro_2();
  }

  if (frame == 6)
  {
    synchro_3();
    sound_play();

    if (!warp_mode)		/* do not redraw values in warp mode */
      DrawGameDoorValues_EM();
  }

  CheckSingleStepMode_EM(action, frame, game_em.any_player_moving);

#if 1
  game_animscreen();

#if 1
#if 0
  SyncDisplay();
#endif

  blitscreen();
#endif
#endif
}

/* read input device for players */

void readjoy(byte action, struct PLAYER *ply)
{
  int north = 0, east = 0, south = 0, west = 0;
  int snap = 0, drop = 0;

  if (action & JOY_LEFT)
    west = 1;

  if (action & JOY_RIGHT)
    east = 1;

  if (action & JOY_UP)
    north = 1;

  if (action & JOY_DOWN)
    south = 1;

  if (action & JOY_BUTTON_1)
    snap = 1;

  if (action & JOY_BUTTON_2)
    drop = 1;

  ply->joy_snap = snap;
  ply->joy_drop = drop;

  if (ply->joy_stick || (north | east | south | west))
  {
    ply->joy_n = north;
    ply->joy_e = east;
    ply->joy_s = south;
    ply->joy_w = west;
  }
}

void SaveEngineSnapshotValues_EM()
{
  int i, j, k;

  engine_snapshot_em.game_em = game_em;
  engine_snapshot_em.lev = lev;

  engine_snapshot_em.RandomEM = RandomEM;
  engine_snapshot_em.frame = frame;

  engine_snapshot_em.screen_x = screen_x;
  engine_snapshot_em.screen_y = screen_y;

  engine_snapshot_em.Boom = Boom;
  engine_snapshot_em.Cave = Cave;
  engine_snapshot_em.Next = Next;
  engine_snapshot_em.Draw = Draw;

  for (i = 0; i < 4; i++)
    engine_snapshot_em.ply[i] = ply[i];

  for (i = 0; i < 4; i++)
    for (j = 0; j < HEIGHT; j++)
      for (k = 0; k < WIDTH; k++)
	engine_snapshot_em.Array[i][j][k] = Array[i][j][k];
}

void LoadEngineSnapshotValues_EM()
{
  int i, j, k;

  game_em = engine_snapshot_em.game_em;
  lev = engine_snapshot_em.lev;

  RandomEM = engine_snapshot_em.RandomEM;
  frame = engine_snapshot_em.frame;

  screen_x = engine_snapshot_em.screen_x;
  screen_y = engine_snapshot_em.screen_y;

  Boom = engine_snapshot_em.Boom;
  Cave = engine_snapshot_em.Cave;
  Next = engine_snapshot_em.Next;
  Draw = engine_snapshot_em.Draw;

  for (i = 0; i < 4; i++)
    ply[i] = engine_snapshot_em.ply[i];

  for (i = 0; i < 4; i++)
    for (j = 0; j < HEIGHT; j++)
      for (k = 0; k < WIDTH; k++)
	Array[i][j][k] = engine_snapshot_em.Array[i][j][k];
}
