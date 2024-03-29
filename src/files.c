/***********************************************************
* Rocks'n'Diamonds -- McDuffin Strikes Back!               *
*----------------------------------------------------------*
* (c) 1995-2006 Artsoft Entertainment                      *
*               Holger Schemel                             *
*               Detmolder Strasse 189                      *
*               33604 Bielefeld                            *
*               Germany                                    *
*               e-mail: info@artsoft.org                   *
*----------------------------------------------------------*
* files.c                                                  *
***********************************************************/

#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <math.h>

#include "libgame/libgame.h"

#include "files.h"
#include "init.h"
#include "tools.h"
#include "tape.h"


#define CHUNK_ID_LEN		4	/* IFF style chunk id length  */
#define CHUNK_SIZE_UNDEFINED	0	/* undefined chunk size == 0  */
#define CHUNK_SIZE_NONE		-1	/* do not write chunk size    */

#define LEVEL_CHUNK_NAME_SIZE	MAX_LEVEL_NAME_LEN
#define LEVEL_CHUNK_AUTH_SIZE	MAX_LEVEL_AUTHOR_LEN

#define LEVEL_CHUNK_VERS_SIZE	8	/* size of file version chunk */
#define LEVEL_CHUNK_DATE_SIZE	4	/* size of file date chunk    */
#define LEVEL_CHUNK_HEAD_SIZE	80	/* size of level file header  */
#define LEVEL_CHUNK_HEAD_UNUSED	0	/* unused level header bytes  */
#define LEVEL_CHUNK_CNT2_SIZE	160	/* size of level CNT2 chunk   */
#define LEVEL_CHUNK_CNT2_UNUSED	11	/* unused CNT2 chunk bytes    */
#define LEVEL_CHUNK_CNT3_HEADER	16	/* size of level CNT3 header  */
#define LEVEL_CHUNK_CNT3_UNUSED	10	/* unused CNT3 chunk bytes    */
#define LEVEL_CPART_CUS3_SIZE	134	/* size of CUS3 chunk part    */
#define LEVEL_CPART_CUS3_UNUSED	15	/* unused CUS3 bytes / part   */
#define LEVEL_CHUNK_GRP1_SIZE	74	/* size of level GRP1 chunk   */

/* (element number, number of change pages, change page number) */
#define LEVEL_CHUNK_CUSX_UNCHANGED	(2 + (1 + 1) + (1 + 1))

/* (element number only) */
#define LEVEL_CHUNK_GRPX_UNCHANGED	2
#define LEVEL_CHUNK_NOTE_UNCHANGED	2

/* (nothing at all if unchanged) */
#define LEVEL_CHUNK_ELEM_UNCHANGED	0

#define TAPE_CHUNK_VERS_SIZE	8	/* size of file version chunk */
#define TAPE_CHUNK_HEAD_SIZE	20	/* size of tape file header   */
#define TAPE_CHUNK_HEAD_UNUSED	3	/* unused tape header bytes   */

#define LEVEL_CHUNK_CNT3_SIZE(x)	 (LEVEL_CHUNK_CNT3_HEADER + (x))
#define LEVEL_CHUNK_CUS3_SIZE(x)	 (2 + (x) * LEVEL_CPART_CUS3_SIZE)
#define LEVEL_CHUNK_CUS4_SIZE(x)	 (96 + (x) * 48)

/* file identifier strings */
#define LEVEL_COOKIE_TMPL		"ROCKSNDIAMONDS_LEVEL_FILE_VERSION_x.x"
#define TAPE_COOKIE_TMPL		"ROCKSNDIAMONDS_TAPE_FILE_VERSION_x.x"
#define SCORE_COOKIE			"ROCKSNDIAMONDS_SCORE_FILE_VERSION_1.2"

/* values for deciding when (not) to save configuration data */
#define SAVE_CONF_NEVER			0
#define SAVE_CONF_ALWAYS		1
#define SAVE_CONF_WHEN_CHANGED		-1

/* values for chunks using micro chunks */
#define CONF_MASK_1_BYTE		0x00
#define CONF_MASK_2_BYTE		0x40
#define CONF_MASK_4_BYTE		0x80
#define CONF_MASK_MULTI_BYTES		0xc0

#define CONF_MASK_BYTES			0xc0
#define CONF_MASK_TOKEN			0x3f

#define CONF_VALUE_1_BYTE(x)		(CONF_MASK_1_BYTE	| (x))
#define CONF_VALUE_2_BYTE(x)		(CONF_MASK_2_BYTE	| (x))
#define CONF_VALUE_4_BYTE(x)		(CONF_MASK_4_BYTE	| (x))
#define CONF_VALUE_MULTI_BYTES(x)	(CONF_MASK_MULTI_BYTES	| (x))

/* these definitions are just for convenience of use and readability */
#define CONF_VALUE_8_BIT(x)		CONF_VALUE_1_BYTE(x)
#define CONF_VALUE_16_BIT(x)		CONF_VALUE_2_BYTE(x)
#define CONF_VALUE_32_BIT(x)		CONF_VALUE_4_BYTE(x)
#define CONF_VALUE_BYTES(x)		CONF_VALUE_MULTI_BYTES(x)

#define CONF_VALUE_NUM_BYTES(x)		((x) == CONF_MASK_1_BYTE ? 1 :	\
					 (x) == CONF_MASK_2_BYTE ? 2 :	\
					 (x) == CONF_MASK_4_BYTE ? 4 : 0)

#define CONF_CONTENT_NUM_ELEMENTS	(3 * 3)
#define CONF_CONTENT_NUM_BYTES		(CONF_CONTENT_NUM_ELEMENTS * 2)
#define CONF_ELEMENT_NUM_BYTES		(2)

#define CONF_ENTITY_NUM_BYTES(t)	((t) == TYPE_ELEMENT ||		\
					 (t) == TYPE_ELEMENT_LIST ?	\
					 CONF_ELEMENT_NUM_BYTES :	\
					 (t) == TYPE_CONTENT ||		\
					 (t) == TYPE_CONTENT_LIST ?	\
					 CONF_CONTENT_NUM_BYTES : 1)

#define CONF_ELEMENT_BYTE_POS(i)	((i) * CONF_ELEMENT_NUM_BYTES)
#define CONF_ELEMENTS_ELEMENT(b,i)     ((b[CONF_ELEMENT_BYTE_POS(i)] << 8) |  \
					(b[CONF_ELEMENT_BYTE_POS(i) + 1]))

#define CONF_CONTENT_ELEMENT_POS(c,x,y)	((c) * CONF_CONTENT_NUM_ELEMENTS +    \
					 (y) * 3 + (x))
#define CONF_CONTENT_BYTE_POS(c,x,y)	(CONF_CONTENT_ELEMENT_POS(c,x,y) *    \
					 CONF_ELEMENT_NUM_BYTES)
#define CONF_CONTENTS_ELEMENT(b,c,x,y) ((b[CONF_CONTENT_BYTE_POS(c,x,y)]<< 8)|\
					(b[CONF_CONTENT_BYTE_POS(c,x,y) + 1]))

/* temporary variables used to store pointers to structure members */
static struct LevelInfo li;
static struct ElementInfo xx_ei, yy_ei;
static struct ElementChangeInfo xx_change;
static struct ElementGroupInfo xx_group;
static struct EnvelopeInfo xx_envelope;
static unsigned int xx_event_bits[NUM_CE_BITFIELDS];
static char xx_default_description[MAX_ELEMENT_NAME_LEN + 1];
static int xx_num_contents;
static int xx_current_change_page;
static char xx_default_string_empty[1] = "";
static int xx_string_length_unused;

struct LevelFileConfigInfo
{
  int element;			/* element for which data is to be stored */
  int save_type;		/* save data always, never or when changed */
  int data_type;		/* data type (used internally, not stored) */
  int conf_type;		/* micro chunk identifier (stored in file) */

  /* (mandatory) */
  void *value;			/* variable that holds the data to be stored */
  int default_value;		/* initial default value for this variable */

  /* (optional) */
  void *value_copy;		/* variable that holds the data to be copied */
  void *num_entities;		/* number of entities for multi-byte data */
  int default_num_entities;	/* default number of entities for this data */
  int max_num_entities;		/* maximal number of entities for this data */
  char *default_string;		/* optional default string for string data */
};

static struct LevelFileConfigInfo chunk_config_INFO[] =
{
  /* ---------- values not related to single elements ----------------------- */

  {
    -1,					SAVE_CONF_ALWAYS,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(1),
    &li.game_engine_type,		GAME_ENGINE_TYPE_RND
  },

  {
    -1,					SAVE_CONF_ALWAYS,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.fieldx,				STD_LEV_FIELDX
  },
  {
    -1,					SAVE_CONF_ALWAYS,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.fieldy,				STD_LEV_FIELDY
  },

  {
    -1,					SAVE_CONF_ALWAYS,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(3),
    &li.time,				100
  },

  {
    -1,					SAVE_CONF_ALWAYS,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(4),
    &li.gems_needed,			0
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_32_BIT(2),
    &li.random_seed,			0
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(2),
    &li.use_step_counter,		FALSE
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_8_BIT(4),
    &li.wind_direction_initial,		MV_NONE
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(5),
    &li.em_slippery_gems,		FALSE
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(6),
    &li.use_custom_template,		FALSE
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_32_BIT(1),
    &li.can_move_into_acid_bits,	~0	/* default: everything can */
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_8_BIT(7),
    &li.dont_collide_with_bits,		~0	/* default: always deadly */
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(8),
    &li.em_explodes_by_fire,		FALSE
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(5),
    &li.score[SC_TIME_BONUS],		1
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(9),
    &li.auto_exit_sokoban,		FALSE
  },

  {
    -1,					-1,
    -1,					-1,
    NULL,				-1
  }
};

static struct LevelFileConfigInfo chunk_config_ELEM[] =
{
  /* (these values are the same for each player) */
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(1),
    &li.block_last_field,		FALSE	/* default case for EM levels */
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(2),
    &li.sp_block_last_field,		TRUE	/* default case for SP levels */
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(3),
    &li.instant_relocation,		FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(4),
    &li.can_pass_to_walkable,		FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(5),
    &li.block_snap_field,		TRUE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(6),
    &li.continuous_snapping,		TRUE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(12),
    &li.shifted_relocation,		FALSE
  },

  /* (these values are different for each player) */
  {
    EL_PLAYER_1,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(7),
    &li.initial_player_stepsize[0],	STEPSIZE_NORMAL
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(8),
    &li.initial_player_gravity[0],	FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(9),
    &li.use_start_element[0],		FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &li.start_element[0],		EL_PLAYER_1
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(10),
    &li.use_artwork_element[0],		FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(2),
    &li.artwork_element[0],		EL_PLAYER_1
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(11),
    &li.use_explosion_element[0],	FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(3),
    &li.explosion_element[0],		EL_PLAYER_1
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(13),
    &li.use_initial_inventory[0],	FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(14),
    &li.initial_inventory_size[0],	1
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_ELEMENT_LIST,			CONF_VALUE_BYTES(1),
    &li.initial_inventory_content[0][0],EL_EMPTY, NULL,
    &li.initial_inventory_size[0],	1, MAX_INITIAL_INVENTORY_SIZE
  },

  {
    EL_PLAYER_2,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(7),
    &li.initial_player_stepsize[1],	STEPSIZE_NORMAL
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(8),
    &li.initial_player_gravity[1],	FALSE
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(9),
    &li.use_start_element[1],		FALSE
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &li.start_element[1],		EL_PLAYER_2
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(10),
    &li.use_artwork_element[1],		FALSE
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(2),
    &li.artwork_element[1],		EL_PLAYER_2
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(11),
    &li.use_explosion_element[1],	FALSE
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(3),
    &li.explosion_element[1],		EL_PLAYER_2
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(13),
    &li.use_initial_inventory[1],	FALSE
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(14),
    &li.initial_inventory_size[1],	1
  },
  {
    EL_PLAYER_2,			-1,
    TYPE_ELEMENT_LIST,			CONF_VALUE_BYTES(1),
    &li.initial_inventory_content[1][0],EL_EMPTY, NULL,
    &li.initial_inventory_size[1],	1, MAX_INITIAL_INVENTORY_SIZE
  },

  {
    EL_PLAYER_3,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(7),
    &li.initial_player_stepsize[2],	STEPSIZE_NORMAL
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(8),
    &li.initial_player_gravity[2],	FALSE
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(9),
    &li.use_start_element[2],		FALSE
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &li.start_element[2],		EL_PLAYER_3
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(10),
    &li.use_artwork_element[2],		FALSE
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(2),
    &li.artwork_element[2],		EL_PLAYER_3
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(11),
    &li.use_explosion_element[2],	FALSE
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(3),
    &li.explosion_element[2],		EL_PLAYER_3
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(13),
    &li.use_initial_inventory[2],	FALSE
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(14),
    &li.initial_inventory_size[2],	1
  },
  {
    EL_PLAYER_3,			-1,
    TYPE_ELEMENT_LIST,			CONF_VALUE_BYTES(1),
    &li.initial_inventory_content[2][0],EL_EMPTY, NULL,
    &li.initial_inventory_size[2],	1, MAX_INITIAL_INVENTORY_SIZE
  },

  {
    EL_PLAYER_4,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(7),
    &li.initial_player_stepsize[3],	STEPSIZE_NORMAL
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(8),
    &li.initial_player_gravity[3],	FALSE
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(9),
    &li.use_start_element[3],		FALSE
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &li.start_element[3],		EL_PLAYER_4
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(10),
    &li.use_artwork_element[3],		FALSE
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(2),
    &li.artwork_element[3],		EL_PLAYER_4
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(11),
    &li.use_explosion_element[3],	FALSE
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(3),
    &li.explosion_element[3],		EL_PLAYER_4
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(13),
    &li.use_initial_inventory[3],	FALSE
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(14),
    &li.initial_inventory_size[3],	1
  },
  {
    EL_PLAYER_4,			-1,
    TYPE_ELEMENT_LIST,			CONF_VALUE_BYTES(1),
    &li.initial_inventory_content[3][0],EL_EMPTY, NULL,
    &li.initial_inventory_size[3],	1, MAX_INITIAL_INVENTORY_SIZE
  },

  {
    EL_EMERALD,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_EMERALD],		10
  },

  {
    EL_DIAMOND,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_DIAMOND],		10
  },

  {
    EL_BUG,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_BUG],			10
  },

  {
    EL_SPACESHIP,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_SPACESHIP],		10
  },

  {
    EL_PACMAN,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_PACMAN],		10
  },

  {
    EL_NUT,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_NUT],			10
  },

  {
    EL_DYNAMITE,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_DYNAMITE],		10
  },

  {
    EL_KEY_1,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_KEY],			10
  },

  {
    EL_PEARL,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_PEARL],		10
  },

  {
    EL_CRYSTAL,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_CRYSTAL],		10
  },

  {
    EL_BD_AMOEBA,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &li.amoeba_content,			EL_DIAMOND
  },
  {
    EL_BD_AMOEBA,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.amoeba_speed,			10
  },
  {
    EL_BD_AMOEBA,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(1),
    &li.grow_into_diggable,		TRUE
  },

  {
    EL_YAMYAM,				-1,
    TYPE_CONTENT_LIST,			CONF_VALUE_BYTES(1),
    &li.yamyam_content,			EL_ROCK, NULL,
    &li.num_yamyam_contents,		4, MAX_ELEMENT_CONTENTS
  },
  {
    EL_YAMYAM,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_YAMYAM],		10
  },

  {
    EL_ROBOT,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_ROBOT],		10
  },
  {
    EL_ROBOT,				-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.slurp_score,			10
  },

  {
    EL_ROBOT_WHEEL,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.time_wheel,			10
  },

  {
    EL_MAGIC_WALL,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.time_magic_wall,		10
  },

  {
    EL_GAME_OF_LIFE,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(1),
    &li.game_of_life[0],		2
  },
  {
    EL_GAME_OF_LIFE,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(2),
    &li.game_of_life[1],		3
  },
  {
    EL_GAME_OF_LIFE,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(3),
    &li.game_of_life[2],		3
  },
  {
    EL_GAME_OF_LIFE,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(4),
    &li.game_of_life[3],		3
  },

  {
    EL_BIOMAZE,				-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(1),
    &li.biomaze[0],			2
  },
  {
    EL_BIOMAZE,				-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(2),
    &li.biomaze[1],			3
  },
  {
    EL_BIOMAZE,				-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(3),
    &li.biomaze[2],			3
  },
  {
    EL_BIOMAZE,				-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(4),
    &li.biomaze[3],			3
  },

  {
    EL_TIMEGATE_SWITCH,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.time_timegate,			10
  },

  {
    EL_LIGHT_SWITCH_ACTIVE,		-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.time_light,			10
  },

  {
    EL_SHIELD_NORMAL,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.shield_normal_time,		10
  },
  {
    EL_SHIELD_NORMAL,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.score[SC_SHIELD],		10
  },

  {
    EL_SHIELD_DEADLY,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.shield_deadly_time,		10
  },
  {
    EL_SHIELD_DEADLY,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.score[SC_SHIELD],		10
  },

  {
    EL_EXTRA_TIME,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.extra_time,			10
  },
  {
    EL_EXTRA_TIME,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.extra_time_score,		10
  },

  {
    EL_TIME_ORB_FULL,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.time_orb_time,			10
  },
  {
    EL_TIME_ORB_FULL,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(1),
    &li.use_time_orb_bug,		FALSE
  },

  {
    EL_SPRING,				-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(1),
    &li.use_spring_bug,			FALSE
  },

  {
    EL_EMC_ANDROID,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.android_move_time,		10
  },
  {
    EL_EMC_ANDROID,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.android_clone_time,		10
  },
  {
    EL_EMC_ANDROID,			-1,
    TYPE_ELEMENT_LIST,			CONF_VALUE_BYTES(1),
    &li.android_clone_element[0],	EL_EMPTY, NULL,
    &li.num_android_clone_elements,	1, MAX_ANDROID_ELEMENTS
  },

  {
    EL_EMC_LENSES,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.lenses_score,			10
  },
  {
    EL_EMC_LENSES,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.lenses_time,			10
  },

  {
    EL_EMC_MAGNIFIER,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.magnify_score,			10
  },
  {
    EL_EMC_MAGNIFIER,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.magnify_time,			10
  },

  {
    EL_EMC_MAGIC_BALL,			-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.ball_time,			10
  },
  {
    EL_EMC_MAGIC_BALL,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(1),
    &li.ball_random,			FALSE
  },
  {
    EL_EMC_MAGIC_BALL,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(2),
    &li.ball_state_initial,		FALSE
  },
  {
    EL_EMC_MAGIC_BALL,			-1,
    TYPE_CONTENT_LIST,			CONF_VALUE_BYTES(1),
    &li.ball_content,			EL_EMPTY, NULL,
    &li.num_ball_contents,		4, MAX_ELEMENT_CONTENTS
  },

  /* ---------- unused values ----------------------------------------------- */

  {
    EL_UNKNOWN,				SAVE_CONF_NEVER,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(1),
    &li.score[SC_UNKNOWN_14],		10
  },
  {
    EL_UNKNOWN,				SAVE_CONF_NEVER,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &li.score[SC_UNKNOWN_15],		10
  },

  {
    -1,					-1,
    -1,					-1,
    NULL,				-1
  }
};

static struct LevelFileConfigInfo chunk_config_NOTE[] =
{
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(1),
    &xx_envelope.xsize,			MAX_ENVELOPE_XSIZE,
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(2),
    &xx_envelope.ysize,			MAX_ENVELOPE_YSIZE,
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(3),
    &xx_envelope.autowrap,		FALSE
  },
  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(4),
    &xx_envelope.centered,		FALSE
  },

  {
    -1,					-1,
    TYPE_STRING,			CONF_VALUE_BYTES(1),
    &xx_envelope.text,			-1, NULL,
    &xx_string_length_unused,		-1, MAX_ENVELOPE_TEXT_LEN,
    &xx_default_string_empty[0]
  },

  {
    -1,					-1,
    -1,					-1,
    NULL,				-1
  }
};

static struct LevelFileConfigInfo chunk_config_CUSX_base[] =
{
  {
    -1,					-1,
    TYPE_STRING,			CONF_VALUE_BYTES(1),
    &xx_ei.description[0],		-1,
    &yy_ei.description[0],
    &xx_string_length_unused,		-1, MAX_ELEMENT_NAME_LEN,
    &xx_default_description[0]
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_32_BIT(1),
    &xx_ei.properties[EP_BITFIELD_BASE_NR], EP_BITMASK_BASE_DEFAULT,
    &yy_ei.properties[EP_BITFIELD_BASE_NR]
  },
#if 0
  /* (reserved) */
  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_32_BIT(2),
    &xx_ei.properties[EP_BITFIELD_BASE_NR + 1], EP_BITMASK_DEFAULT,
    &yy_ei.properties[EP_BITFIELD_BASE_NR + 1]
  },
#endif

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(1),
    &xx_ei.use_gfx_element,		FALSE,
    &yy_ei.use_gfx_element
  },
  {
    -1,					-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &xx_ei.gfx_element_initial,		EL_EMPTY_SPACE,
    &yy_ei.gfx_element_initial
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_8_BIT(2),
    &xx_ei.access_direction,		MV_ALL_DIRECTIONS,
    &yy_ei.access_direction
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &xx_ei.collect_score_initial,	10,
    &yy_ei.collect_score_initial
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(3),
    &xx_ei.collect_count_initial,	1,
    &yy_ei.collect_count_initial
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(4),
    &xx_ei.ce_value_fixed_initial,	0,
    &yy_ei.ce_value_fixed_initial
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(5),
    &xx_ei.ce_value_random_initial,	0,
    &yy_ei.ce_value_random_initial
  },
  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(3),
    &xx_ei.use_last_ce_value,		FALSE,
    &yy_ei.use_last_ce_value
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(6),
    &xx_ei.push_delay_fixed,		8,
    &yy_ei.push_delay_fixed
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(7),
    &xx_ei.push_delay_random,		8,
    &yy_ei.push_delay_random
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(8),
    &xx_ei.drop_delay_fixed,		0,
    &yy_ei.drop_delay_fixed
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(9),
    &xx_ei.drop_delay_random,		0,
    &yy_ei.drop_delay_random
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(10),
    &xx_ei.move_delay_fixed,		0,
    &yy_ei.move_delay_fixed
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(11),
    &xx_ei.move_delay_random,		0,
    &yy_ei.move_delay_random
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_32_BIT(3),
    &xx_ei.move_pattern,		MV_ALL_DIRECTIONS,
    &yy_ei.move_pattern
  },
  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_8_BIT(4),
    &xx_ei.move_direction_initial,	MV_START_AUTOMATIC,
    &yy_ei.move_direction_initial
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(5),
    &xx_ei.move_stepsize,		TILEX / 8,
    &yy_ei.move_stepsize
  },

  {
    -1,					-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(12),
    &xx_ei.move_enter_element,		EL_EMPTY_SPACE,
    &yy_ei.move_enter_element
  },
  {
    -1,					-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(13),
    &xx_ei.move_leave_element,		EL_EMPTY_SPACE,
    &yy_ei.move_leave_element
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(6),
    &xx_ei.move_leave_type,		LEAVE_TYPE_UNLIMITED,
    &yy_ei.move_leave_type
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(7),
    &xx_ei.slippery_type,		SLIPPERY_ANY_RANDOM,
    &yy_ei.slippery_type
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(8),
    &xx_ei.explosion_type,		EXPLODES_3X3,
    &yy_ei.explosion_type
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(14),
    &xx_ei.explosion_delay,		16,
    &yy_ei.explosion_delay
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(15),
    &xx_ei.ignition_delay,		8,
    &yy_ei.ignition_delay
  },

  {
    -1,					-1,
    TYPE_CONTENT_LIST,			CONF_VALUE_BYTES(2),
    &xx_ei.content,			EL_EMPTY_SPACE,
    &yy_ei.content,
    &xx_num_contents,			1, 1
  },

  /* ---------- "num_change_pages" must be the last entry ------------------- */

  {
    -1,					SAVE_CONF_ALWAYS,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(9),
    &xx_ei.num_change_pages,		1,
    &yy_ei.num_change_pages
  },

  {
    -1,					-1,
    -1,					-1,
    NULL,				-1,
    NULL
  }
};

static struct LevelFileConfigInfo chunk_config_CUSX_change[] =
{
  /* ---------- "current_change_page" must be the first entry --------------- */

  {
    -1,					SAVE_CONF_ALWAYS,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(1),
    &xx_current_change_page,		-1
  },

  /* ---------- (the remaining entries can be in any order) ----------------- */

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(2),
    &xx_change.can_change,		FALSE
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_32_BIT(1),
    &xx_event_bits[0],			0
  },
  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_32_BIT(2),
    &xx_event_bits[1],			0
  },

  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_8_BIT(3),
    &xx_change.trigger_player,		CH_PLAYER_ANY
  },
  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_8_BIT(4),
    &xx_change.trigger_side,		CH_SIDE_ANY
  },
  {
    -1,					-1,
    TYPE_BITFIELD,			CONF_VALUE_32_BIT(3),
    &xx_change.trigger_page,		CH_PAGE_ANY
  },

  {
    -1,					-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &xx_change.target_element,		EL_EMPTY_SPACE
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(2),
    &xx_change.delay_fixed,		0
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(3),
    &xx_change.delay_random,		0
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(4),
    &xx_change.delay_frames,		FRAMES_PER_SECOND
  },

  {
    -1,					-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(5),
    &xx_change.initial_trigger_element,	EL_EMPTY_SPACE
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(6),
    &xx_change.explode,			FALSE
  },
  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(7),
    &xx_change.use_target_content,	FALSE
  },
  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(8),
    &xx_change.only_if_complete,	FALSE
  },
  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(9),
    &xx_change.use_random_replace,	FALSE
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(10),
    &xx_change.random_percentage,	100
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(11),
    &xx_change.replace_when,		CP_WHEN_EMPTY
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(12),
    &xx_change.has_action,		FALSE
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(13),
    &xx_change.action_type,		CA_NO_ACTION
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(14),
    &xx_change.action_mode,		CA_MODE_UNDEFINED
  },
  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_16_BIT(6),
    &xx_change.action_arg,		CA_ARG_UNDEFINED
  },

  {
    -1,					-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(7),
    &xx_change.action_element,		EL_EMPTY_SPACE
  },

  {
    -1,					-1,
    TYPE_CONTENT_LIST,			CONF_VALUE_BYTES(1),
    &xx_change.target_content,		EL_EMPTY_SPACE, NULL,
    &xx_num_contents,			1, 1
  },

  {
    -1,					-1,
    -1,					-1,
    NULL,				-1
  }
};

static struct LevelFileConfigInfo chunk_config_GRPX[] =
{
  {
    -1,					-1,
    TYPE_STRING,			CONF_VALUE_BYTES(1),
    &xx_ei.description[0],		-1, NULL,
    &xx_string_length_unused,		-1, MAX_ELEMENT_NAME_LEN,
    &xx_default_description[0]
  },

  {
    -1,					-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(1),
    &xx_ei.use_gfx_element,		FALSE
  },
  {
    -1,					-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &xx_ei.gfx_element_initial,		EL_EMPTY_SPACE
  },

  {
    -1,					-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(2),
    &xx_group.choice_mode,		ANIM_RANDOM
  },

  {
    -1,					-1,
    TYPE_ELEMENT_LIST,			CONF_VALUE_BYTES(2),
    &xx_group.element[0],		EL_EMPTY_SPACE, NULL,
    &xx_group.num_elements,		1, MAX_ELEMENTS_IN_GROUP
  },

  {
    -1,					-1,
    -1,					-1,
    NULL,				-1
  }
};

static struct LevelFileConfigInfo chunk_config_CONF[] =		/* (OBSOLETE) */
{
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(9),
    &li.block_snap_field,		TRUE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(13),
    &li.continuous_snapping,		TRUE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_INTEGER,			CONF_VALUE_8_BIT(1),
    &li.initial_player_stepsize[0],	STEPSIZE_NORMAL
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(10),
    &li.use_start_element[0],		FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(1),
    &li.start_element[0],		EL_PLAYER_1
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(11),
    &li.use_artwork_element[0],		FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(2),
    &li.artwork_element[0],		EL_PLAYER_1
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_BOOLEAN,			CONF_VALUE_8_BIT(12),
    &li.use_explosion_element[0],	FALSE
  },
  {
    EL_PLAYER_1,			-1,
    TYPE_ELEMENT,			CONF_VALUE_16_BIT(3),
    &li.explosion_element[0],		EL_PLAYER_1
  },

  {
    -1,					-1,
    -1,					-1,
    NULL,				-1
  }
};

static struct
{
  int filetype;
  char *id;
}
filetype_id_list[] =
{
  { LEVEL_FILE_TYPE_RND,	"RND"	},
  { LEVEL_FILE_TYPE_BD,		"BD"	},
  { LEVEL_FILE_TYPE_EM,		"EM"	},
  { LEVEL_FILE_TYPE_SP,		"SP"	},
  { LEVEL_FILE_TYPE_DX,		"DX"	},
  { LEVEL_FILE_TYPE_SB,		"SB"	},
  { LEVEL_FILE_TYPE_DC,		"DC"	},
  { -1,				NULL	},
};


/* ========================================================================= */
/* level file functions                                                      */
/* ========================================================================= */

static boolean check_special_flags(char *flag)
{
#if 0
  printf("::: '%s', '%s', '%s'\n",
	 flag,
	 options.special_flags,
	 leveldir_current->special_flags);
#endif

  if (strEqual(options.special_flags, flag) ||
      strEqual(leveldir_current->special_flags, flag))
    return TRUE;

  return FALSE;
}

static struct DateInfo getCurrentDate()
{
  time_t epoch_seconds = time(NULL);
  struct tm *now = localtime(&epoch_seconds);
  struct DateInfo date;

  date.year  = now->tm_year + 1900;
  date.month = now->tm_mon  + 1;
  date.day   = now->tm_mday;

  date.src   = DATE_SRC_CLOCK;

  return date;
}

static void resetEventFlags(struct ElementChangeInfo *change)
{
  int i;

  for (i = 0; i < NUM_CHANGE_EVENTS; i++)
    change->has_event[i] = FALSE;
}

static void resetEventBits()
{
  int i;

  for (i = 0; i < NUM_CE_BITFIELDS; i++)
    xx_event_bits[i] = 0;
}

static void setEventFlagsFromEventBits(struct ElementChangeInfo *change)
{
  int i;

  /* important: only change event flag if corresponding event bit is set
     (this is because all xx_event_bits[] values are loaded separately,
     and all xx_event_bits[] values are set back to zero before loading
     another value xx_event_bits[x] (each value representing 32 flags)) */

  for (i = 0; i < NUM_CHANGE_EVENTS; i++)
    if (xx_event_bits[CH_EVENT_BITFIELD_NR(i)] & CH_EVENT_BIT(i))
      change->has_event[i] = TRUE;
}

static void setEventBitsFromEventFlags(struct ElementChangeInfo *change)
{
  int i;

  /* in contrast to the above function setEventFlagsFromEventBits(), it
     would also be possible to set all bits in xx_event_bits[] to 0 or 1
     depending on the corresponding change->has_event[i] values here, as
     all xx_event_bits[] values are reset in resetEventBits() before */

  for (i = 0; i < NUM_CHANGE_EVENTS; i++)
    if (change->has_event[i])
      xx_event_bits[CH_EVENT_BITFIELD_NR(i)] |= CH_EVENT_BIT(i);
}

static char *getDefaultElementDescription(struct ElementInfo *ei)
{
  static char description[MAX_ELEMENT_NAME_LEN + 1];
  char *default_description = (ei->custom_description != NULL ?
			       ei->custom_description :
			       ei->editor_description);
  int i;

  /* always start with reliable default values */
  for (i = 0; i < MAX_ELEMENT_NAME_LEN + 1; i++)
    description[i] = '\0';

  /* truncate element description to MAX_ELEMENT_NAME_LEN bytes */
  strncpy(description, default_description, MAX_ELEMENT_NAME_LEN);

  return &description[0];
}

static void setElementDescriptionToDefault(struct ElementInfo *ei)
{
  char *default_description = getDefaultElementDescription(ei);
  int i;

  for (i = 0; i < MAX_ELEMENT_NAME_LEN + 1; i++)
    ei->description[i] = default_description[i];
}

static void setConfigToDefaultsFromConfigList(struct LevelFileConfigInfo *conf)
{
  int i;

  for (i = 0; conf[i].data_type != -1; i++)
  {
    int default_value = conf[i].default_value;
    int data_type = conf[i].data_type;
    int conf_type = conf[i].conf_type;
    int byte_mask = conf_type & CONF_MASK_BYTES;

    if (byte_mask == CONF_MASK_MULTI_BYTES)
    {
      int default_num_entities = conf[i].default_num_entities;
      int max_num_entities = conf[i].max_num_entities;

      *(int *)(conf[i].num_entities) = default_num_entities;

      if (data_type == TYPE_STRING)
      {
	char *default_string = conf[i].default_string;
	char *string = (char *)(conf[i].value);

	strncpy(string, default_string, max_num_entities);
      }
      else if (data_type == TYPE_ELEMENT_LIST)
      {
	int *element_array = (int *)(conf[i].value);
	int j;

	for (j = 0; j < max_num_entities; j++)
	  element_array[j] = default_value;
      }
      else if (data_type == TYPE_CONTENT_LIST)
      {
	struct Content *content = (struct Content *)(conf[i].value);
	int c, x, y;

	for (c = 0; c < max_num_entities; c++)
	  for (y = 0; y < 3; y++)
	    for (x = 0; x < 3; x++)
	      content[c].e[x][y] = default_value;
      }
    }
    else	/* constant size configuration data (1, 2 or 4 bytes) */
    {
      if (data_type == TYPE_BOOLEAN)
	*(boolean *)(conf[i].value) = default_value;
      else
	*(int *)    (conf[i].value) = default_value;
    }
  }
}

static void copyConfigFromConfigList(struct LevelFileConfigInfo *conf)
{
  int i;

  for (i = 0; conf[i].data_type != -1; i++)
  {
    int data_type = conf[i].data_type;
    int conf_type = conf[i].conf_type;
    int byte_mask = conf_type & CONF_MASK_BYTES;

    if (byte_mask == CONF_MASK_MULTI_BYTES)
    {
      int max_num_entities = conf[i].max_num_entities;

      if (data_type == TYPE_STRING)
      {
	char *string      = (char *)(conf[i].value);
	char *string_copy = (char *)(conf[i].value_copy);

	strncpy(string_copy, string, max_num_entities);
      }
      else if (data_type == TYPE_ELEMENT_LIST)
      {
	int *element_array      = (int *)(conf[i].value);
	int *element_array_copy = (int *)(conf[i].value_copy);
	int j;

	for (j = 0; j < max_num_entities; j++)
	  element_array_copy[j] = element_array[j];
      }
      else if (data_type == TYPE_CONTENT_LIST)
      {
	struct Content *content      = (struct Content *)(conf[i].value);
	struct Content *content_copy = (struct Content *)(conf[i].value_copy);
	int c, x, y;

	for (c = 0; c < max_num_entities; c++)
	  for (y = 0; y < 3; y++)
	    for (x = 0; x < 3; x++)
	      content_copy[c].e[x][y] = content[c].e[x][y];
      }
    }
    else	/* constant size configuration data (1, 2 or 4 bytes) */
    {
      if (data_type == TYPE_BOOLEAN)
	*(boolean *)(conf[i].value_copy) = *(boolean *)(conf[i].value);
      else
	*(int *)    (conf[i].value_copy) = *(int *)    (conf[i].value);
    }
  }
}

void copyElementInfo(struct ElementInfo *ei_from, struct ElementInfo *ei_to)
{
  int i;

  xx_ei = *ei_from;	/* copy element data into temporary buffer */
  yy_ei = *ei_to;	/* copy element data into temporary buffer */

  copyConfigFromConfigList(chunk_config_CUSX_base);

  *ei_from = xx_ei;
  *ei_to   = yy_ei;

  /* ---------- reinitialize and copy change pages ---------- */

  ei_to->num_change_pages = ei_from->num_change_pages;
  ei_to->current_change_page = ei_from->current_change_page;

  setElementChangePages(ei_to, ei_to->num_change_pages);

  for (i = 0; i < ei_to->num_change_pages; i++)
    ei_to->change_page[i] = ei_from->change_page[i];

  /* ---------- copy group element info ---------- */
  if (ei_from->group != NULL && ei_to->group != NULL)	/* group or internal */
    *ei_to->group = *ei_from->group;

  /* mark this custom element as modified */
  ei_to->modified_settings = TRUE;
}

void setElementChangePages(struct ElementInfo *ei, int change_pages)
{
  int change_page_size = sizeof(struct ElementChangeInfo);

  ei->num_change_pages = MAX(1, change_pages);

  ei->change_page =
    checked_realloc(ei->change_page, ei->num_change_pages * change_page_size);

  if (ei->current_change_page >= ei->num_change_pages)
    ei->current_change_page = ei->num_change_pages - 1;

  ei->change = &ei->change_page[ei->current_change_page];
}

void setElementChangeInfoToDefaults(struct ElementChangeInfo *change)
{
  xx_change = *change;		/* copy change data into temporary buffer */

#if 0
  /* (not needed; set by setConfigToDefaultsFromConfigList()) */
  xx_num_contents = 1;
#endif

  setConfigToDefaultsFromConfigList(chunk_config_CUSX_change);

  *change = xx_change;

  resetEventFlags(change);

  change->direct_action = 0;
  change->other_action = 0;

  change->pre_change_function = NULL;
  change->change_function = NULL;
  change->post_change_function = NULL;
}

static void setLevelInfoToDefaults(struct LevelInfo *level)
{
  static boolean clipboard_elements_initialized = FALSE;
  int i, x, y;

  InitElementPropertiesStatic();

  li = *level;		/* copy level data into temporary buffer */

  setConfigToDefaultsFromConfigList(chunk_config_INFO);
  setConfigToDefaultsFromConfigList(chunk_config_ELEM);

  *level = li;		/* copy temporary buffer back to level data */

  setLevelInfoToDefaults_EM();
  setLevelInfoToDefaults_SP();

  level->native_em_level = &native_em_level;
  level->native_sp_level = &native_sp_level;

  level->file_version = FILE_VERSION_ACTUAL;
  level->game_version = GAME_VERSION_ACTUAL;

  level->creation_date = getCurrentDate();

  level->encoding_16bit_field  = TRUE;
  level->encoding_16bit_yamyam = TRUE;
  level->encoding_16bit_amoeba = TRUE;

  for (x = 0; x < MAX_LEV_FIELDX; x++)
    for (y = 0; y < MAX_LEV_FIELDY; y++)
      level->field[x][y] = EL_SAND;

  for (i = 0; i < MAX_LEVEL_NAME_LEN; i++)
    level->name[i] = '\0';
  for (i = 0; i < MAX_LEVEL_AUTHOR_LEN; i++)
    level->author[i] = '\0';

  strcpy(level->name, NAMELESS_LEVEL_NAME);
  strcpy(level->author, ANONYMOUS_NAME);

  level->field[0][0] = EL_PLAYER_1;
  level->field[STD_LEV_FIELDX - 1][STD_LEV_FIELDY - 1] = EL_EXIT_CLOSED;

  for (i = 0; i < MAX_NUM_ELEMENTS; i++)
  {
    int element = i;
    struct ElementInfo *ei = &element_info[element];

    /* never initialize clipboard elements after the very first time */
    /* (to be able to use clipboard elements between several levels) */
    if (IS_CLIPBOARD_ELEMENT(element) && clipboard_elements_initialized)
      continue;

    if (IS_ENVELOPE(element))
    {
      int envelope_nr = element - EL_ENVELOPE_1;

      setConfigToDefaultsFromConfigList(chunk_config_NOTE);

      level->envelope[envelope_nr] = xx_envelope;
    }

    if (IS_CUSTOM_ELEMENT(element) ||
	IS_GROUP_ELEMENT(element) ||
	IS_INTERNAL_ELEMENT(element))
    {
      xx_ei = *ei;	/* copy element data into temporary buffer */

      setConfigToDefaultsFromConfigList(chunk_config_CUSX_base);

      *ei = xx_ei;
    }

    setElementChangePages(ei, 1);
    setElementChangeInfoToDefaults(ei->change);

    if (IS_CUSTOM_ELEMENT(element) ||
	IS_GROUP_ELEMENT(element) ||
	IS_INTERNAL_ELEMENT(element))
    {
      setElementDescriptionToDefault(ei);

      ei->modified_settings = FALSE;
    }

    if (IS_CUSTOM_ELEMENT(element) ||
	IS_INTERNAL_ELEMENT(element))
    {
      /* internal values used in level editor */

      ei->access_type = 0;
      ei->access_layer = 0;
      ei->access_protected = 0;
      ei->walk_to_action = 0;
      ei->smash_targets = 0;
      ei->deadliness = 0;

      ei->can_explode_by_fire = FALSE;
      ei->can_explode_smashed = FALSE;
      ei->can_explode_impact = FALSE;

      ei->current_change_page = 0;
    }

    if (IS_GROUP_ELEMENT(element) ||
	IS_INTERNAL_ELEMENT(element))
    {
      struct ElementGroupInfo *group;

      /* initialize memory for list of elements in group */
      if (ei->group == NULL)
	ei->group = checked_malloc(sizeof(struct ElementGroupInfo));

      group = ei->group;

      xx_group = *group;	/* copy group data into temporary buffer */

      setConfigToDefaultsFromConfigList(chunk_config_GRPX);

      *group = xx_group;
    }
  }

  clipboard_elements_initialized = TRUE;

  BorderElement = EL_STEELWALL;

  level->no_valid_file = FALSE;

  level->changed = FALSE;

  /* set all bug compatibility flags to "false" => do not emulate this bug */
  level->use_action_after_change_bug = FALSE;

  if (leveldir_current == NULL)		/* only when dumping level */
    return;

  /* try to determine better author name than 'anonymous' */
  if (!strEqual(leveldir_current->author, ANONYMOUS_NAME))
  {
    strncpy(level->author, leveldir_current->author, MAX_LEVEL_AUTHOR_LEN);
    level->author[MAX_LEVEL_AUTHOR_LEN] = '\0';
  }
  else
  {
    switch (LEVELCLASS(leveldir_current))
    {
      case LEVELCLASS_TUTORIAL:
	strcpy(level->author, PROGRAM_AUTHOR_STRING);
	break;

      case LEVELCLASS_CONTRIB:
	strncpy(level->author, leveldir_current->name, MAX_LEVEL_AUTHOR_LEN);
	level->author[MAX_LEVEL_AUTHOR_LEN] = '\0';
	break;

      case LEVELCLASS_PRIVATE:
	strncpy(level->author, getRealName(), MAX_LEVEL_AUTHOR_LEN);
	level->author[MAX_LEVEL_AUTHOR_LEN] = '\0';
	break;

      default:
	/* keep default value */
	break;
    }
  }
}

static void setFileInfoToDefaults(struct LevelFileInfo *level_file_info)
{
  level_file_info->nr = 0;
  level_file_info->type = LEVEL_FILE_TYPE_UNKNOWN;
  level_file_info->packed = FALSE;
  level_file_info->basename = NULL;
  level_file_info->filename = NULL;
}

static void ActivateLevelTemplate()
{
  int x, y;

  /* Currently there is no special action needed to activate the template
     data, because 'element_info' property settings overwrite the original
     level data, while all other variables do not change. */

  /* Exception: 'from_level_template' elements in the original level playfield
     are overwritten with the corresponding elements at the same position in
     playfield from the level template. */

  for (x = 0; x < level.fieldx; x++)
    for (y = 0; y < level.fieldy; y++)
      if (level.field[x][y] == EL_FROM_LEVEL_TEMPLATE)
	level.field[x][y] = level_template.field[x][y];

  if (check_special_flags("load_xsb_to_ces"))
  {
    struct LevelInfo level_backup = level;

    /* overwrite all individual level settings from template level settings */
    level = level_template;

    /* restore playfield size */
    level.fieldx = level_backup.fieldx;
    level.fieldy = level_backup.fieldy;

    /* restore playfield content */
    for (x = 0; x < level.fieldx; x++)
      for (y = 0; y < level.fieldy; y++)
	level.field[x][y] = level_backup.field[x][y];

    /* restore name and author from individual level */
    strcpy(level.name,   level_backup.name);
    strcpy(level.author, level_backup.author);

    /* restore flag "use_custom_template" */
    level.use_custom_template = level_backup.use_custom_template;
  }
}

static char *getLevelFilenameFromBasename(char *basename)
{
  static char *filename = NULL;

  checked_free(filename);

  filename = getPath2(getCurrentLevelDir(), basename);

  return filename;
}

static int getFileTypeFromBasename(char *basename)
{
  /* !!! ALSO SEE COMMENT IN checkForPackageFromBasename() !!! */

  static char *filename = NULL;
  struct stat file_status;

  /* ---------- try to determine file type from filename ---------- */

  /* check for typical filename of a Supaplex level package file */
#if 1
  if (strlen(basename) == 10 && strPrefixLower(basename, "levels.d"))
    return LEVEL_FILE_TYPE_SP;
#else
  if (strlen(basename) == 10 && (strncmp(basename, "levels.d", 8) == 0 ||
				 strncmp(basename, "LEVELS.D", 8) == 0))
    return LEVEL_FILE_TYPE_SP;
#endif

  /* check for typical filename of a Diamond Caves II level package file */
  if (strSuffixLower(basename, ".dc") ||
      strSuffixLower(basename, ".dc2"))
    return LEVEL_FILE_TYPE_DC;

  /* check for typical filename of a Sokoban level package file */
  if (strSuffixLower(basename, ".xsb") &&
      strchr(basename, '%') == NULL)
    return LEVEL_FILE_TYPE_SB;

  /* ---------- try to determine file type from filesize ---------- */

  checked_free(filename);
  filename = getPath2(getCurrentLevelDir(), basename);

  if (stat(filename, &file_status) == 0)
  {
    /* check for typical filesize of a Supaplex level package file */
    if (file_status.st_size == 170496)
      return LEVEL_FILE_TYPE_SP;
  }

  return LEVEL_FILE_TYPE_UNKNOWN;
}

static boolean checkForPackageFromBasename(char *basename)
{
  /* !!! WON'T WORK ANYMORE IF getFileTypeFromBasename() ALSO DETECTS !!!
     !!! SINGLE LEVELS (CURRENTLY ONLY DETECTS LEVEL PACKAGES         !!! */

  return (getFileTypeFromBasename(basename) != LEVEL_FILE_TYPE_UNKNOWN);
}

static char *getSingleLevelBasenameExt(int nr, char *extension)
{
  static char basename[MAX_FILENAME_LEN];

  if (nr < 0)
    sprintf(basename, "template.%s", extension);
  else
    sprintf(basename, "%03d.%s", nr, extension);

  return basename;
}

static char *getSingleLevelBasename(int nr)
{
  return getSingleLevelBasenameExt(nr, LEVELFILE_EXTENSION);
}

static char *getPackedLevelBasename(int type)
{
  static char basename[MAX_FILENAME_LEN];
  char *directory = getCurrentLevelDir();
  DIR *dir;
  struct dirent *dir_entry;

  strcpy(basename, UNDEFINED_FILENAME);		/* default: undefined file */

  if ((dir = opendir(directory)) == NULL)
  {
    Error(ERR_WARN, "cannot read current level directory '%s'", directory);

    return basename;
  }

  while ((dir_entry = readdir(dir)) != NULL)	/* loop until last dir entry */
  {
    char *entry_basename = dir_entry->d_name;
    int entry_type = getFileTypeFromBasename(entry_basename);

    if (entry_type != LEVEL_FILE_TYPE_UNKNOWN)	/* found valid level package */
    {
      if (type == LEVEL_FILE_TYPE_UNKNOWN ||
	  type == entry_type)
      {
	strcpy(basename, entry_basename);

	break;
      }
    }
  }

  closedir(dir);

  return basename;
}

static char *getSingleLevelFilename(int nr)
{
  return getLevelFilenameFromBasename(getSingleLevelBasename(nr));
}

#if 0
static char *getPackedLevelFilename(int type)
{
  return getLevelFilenameFromBasename(getPackedLevelBasename(type));
}
#endif

char *getDefaultLevelFilename(int nr)
{
  return getSingleLevelFilename(nr);
}

#if 0
static void setLevelFileInfo_SingleLevelFilename(struct LevelFileInfo *lfi,
						 int type)
{
  lfi->type = type;
  lfi->packed = FALSE;
  lfi->basename = getSingleLevelBasename(lfi->nr, lfi->type);
  lfi->filename = getLevelFilenameFromBasename(lfi->basename);
}
#endif

static void setLevelFileInfo_FormatLevelFilename(struct LevelFileInfo *lfi,
						 int type, char *format, ...)
{
  static char basename[MAX_FILENAME_LEN];
  va_list ap;

  va_start(ap, format);
  vsprintf(basename, format, ap);
  va_end(ap);

  lfi->type = type;
  lfi->packed = FALSE;
  lfi->basename = basename;
  lfi->filename = getLevelFilenameFromBasename(lfi->basename);
}

static void setLevelFileInfo_PackedLevelFilename(struct LevelFileInfo *lfi,
						 int type)
{
  lfi->type = type;
  lfi->packed = TRUE;
  lfi->basename = getPackedLevelBasename(lfi->type);
  lfi->filename = getLevelFilenameFromBasename(lfi->basename);
}

static int getFiletypeFromID(char *filetype_id)
{
  char *filetype_id_lower;
  int filetype = LEVEL_FILE_TYPE_UNKNOWN;
  int i;

  if (filetype_id == NULL)
    return LEVEL_FILE_TYPE_UNKNOWN;

  filetype_id_lower = getStringToLower(filetype_id);

  for (i = 0; filetype_id_list[i].id != NULL; i++)
  {
    char *id_lower = getStringToLower(filetype_id_list[i].id);
    
    if (strEqual(filetype_id_lower, id_lower))
      filetype = filetype_id_list[i].filetype;

    free(id_lower);

    if (filetype != LEVEL_FILE_TYPE_UNKNOWN)
      break;
  }

  free(filetype_id_lower);

  return filetype;
}

static void determineLevelFileInfo_Filename(struct LevelFileInfo *lfi)
{
  int nr = lfi->nr;

  /* special case: level number is negative => check for level template file */
  if (nr < 0)
  {
#if 1
    /* global variable "leveldir_current" must be modified in the loop below */
    LevelDirTree *leveldir_current_last = leveldir_current;

    /* check for template level in path from current to topmost tree node */

    while (leveldir_current != NULL)
    {
      setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_RND,
					   "template.%s", LEVELFILE_EXTENSION);

      if (fileExists(lfi->filename))
	break;

      leveldir_current = leveldir_current->node_parent;
    }

    /* restore global variable "leveldir_current" modified in above loop */
    leveldir_current = leveldir_current_last;

#else

    setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_RND,
					 "template.%s", LEVELFILE_EXTENSION);

#endif

    /* no fallback if template file not existing */
    return;
  }

  /* special case: check for file name/pattern specified in "levelinfo.conf" */
  if (leveldir_current->level_filename != NULL)
  {
    int filetype = getFiletypeFromID(leveldir_current->level_filetype);

    setLevelFileInfo_FormatLevelFilename(lfi, filetype,
					 leveldir_current->level_filename, nr);

    lfi->packed = checkForPackageFromBasename(leveldir_current->level_filename);

    if (fileExists(lfi->filename))
      return;
  }

  /* check for native Rocks'n'Diamonds level file */
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_RND,
				       "%03d.%s", nr, LEVELFILE_EXTENSION);
  if (fileExists(lfi->filename))
    return;

  /* check for Emerald Mine level file (V1) */
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_EM, "a%c%c",
				       'a' + (nr / 10) % 26, '0' + nr % 10);
  if (fileExists(lfi->filename))
    return;
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_EM, "A%c%c",
				       'A' + (nr / 10) % 26, '0' + nr % 10);
  if (fileExists(lfi->filename))
    return;

  /* check for Emerald Mine level file (V2 to V5) */
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_EM, "%d", nr);
  if (fileExists(lfi->filename))
    return;

  /* check for Emerald Mine level file (V6 / single mode) */
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_EM, "%02ds", nr);
  if (fileExists(lfi->filename))
    return;
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_EM, "%02dS", nr);
  if (fileExists(lfi->filename))
    return;

  /* check for Emerald Mine level file (V6 / teamwork mode) */
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_EM, "%02dt", nr);
  if (fileExists(lfi->filename))
    return;
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_EM, "%02dT", nr);
  if (fileExists(lfi->filename))
    return;

  /* check for various packed level file formats */
  setLevelFileInfo_PackedLevelFilename(lfi, LEVEL_FILE_TYPE_UNKNOWN);
  if (fileExists(lfi->filename))
    return;

  /* no known level file found -- use default values (and fail later) */
  setLevelFileInfo_FormatLevelFilename(lfi, LEVEL_FILE_TYPE_RND,
				       "%03d.%s", nr, LEVELFILE_EXTENSION);
}

static void determineLevelFileInfo_Filetype(struct LevelFileInfo *lfi)
{
  if (lfi->type == LEVEL_FILE_TYPE_UNKNOWN)
    lfi->type = getFileTypeFromBasename(lfi->basename);
}

static void setLevelFileInfo(struct LevelFileInfo *level_file_info, int nr)
{
  /* always start with reliable default values */
  setFileInfoToDefaults(level_file_info);

  level_file_info->nr = nr;	/* set requested level number */

  determineLevelFileInfo_Filename(level_file_info);
  determineLevelFileInfo_Filetype(level_file_info);
}

/* ------------------------------------------------------------------------- */
/* functions for loading R'n'D level                                         */
/* ------------------------------------------------------------------------- */

int getMappedElement(int element)
{
  /* remap some (historic, now obsolete) elements */

  switch (element)
  {
    case EL_PLAYER_OBSOLETE:
      element = EL_PLAYER_1;
      break;

    case EL_KEY_OBSOLETE:
      element = EL_KEY_1;
      break;

    case EL_EM_KEY_1_FILE_OBSOLETE:
      element = EL_EM_KEY_1;
      break;

    case EL_EM_KEY_2_FILE_OBSOLETE:
      element = EL_EM_KEY_2;
      break;

    case EL_EM_KEY_3_FILE_OBSOLETE:
      element = EL_EM_KEY_3;
      break;

    case EL_EM_KEY_4_FILE_OBSOLETE:
      element = EL_EM_KEY_4;
      break;

    case EL_ENVELOPE_OBSOLETE:
      element = EL_ENVELOPE_1;
      break;

    case EL_SP_EMPTY:
      element = EL_EMPTY;
      break;

    default:
      if (element >= NUM_FILE_ELEMENTS)
      {
	Error(ERR_WARN, "invalid level element %d", element);

	element = EL_UNKNOWN;
      }
      break;
  }

  return element;
}

int getMappedElementByVersion(int element, int game_version)
{
  /* remap some elements due to certain game version */

  if (game_version <= VERSION_IDENT(2,2,0,0))
  {
    /* map game font elements */
    element = (element == EL_CHAR('[')  ? EL_CHAR_AUMLAUT :
	       element == EL_CHAR('\\') ? EL_CHAR_OUMLAUT :
	       element == EL_CHAR(']')  ? EL_CHAR_UUMLAUT :
	       element == EL_CHAR('^')  ? EL_CHAR_COPYRIGHT : element);
  }

  if (game_version < VERSION_IDENT(3,0,0,0))
  {
    /* map Supaplex gravity tube elements */
    element = (element == EL_SP_GRAVITY_PORT_LEFT  ? EL_SP_PORT_LEFT  :
	       element == EL_SP_GRAVITY_PORT_RIGHT ? EL_SP_PORT_RIGHT :
	       element == EL_SP_GRAVITY_PORT_UP    ? EL_SP_PORT_UP    :
	       element == EL_SP_GRAVITY_PORT_DOWN  ? EL_SP_PORT_DOWN  :
	       element);
  }

  return element;
}

static int LoadLevel_VERS(FILE *file, int chunk_size, struct LevelInfo *level)
{
  level->file_version = getFileVersion(file);
  level->game_version = getFileVersion(file);

  return chunk_size;
}

static int LoadLevel_DATE(FILE *file, int chunk_size, struct LevelInfo *level)
{
  level->creation_date.year  = getFile16BitBE(file);
  level->creation_date.month = getFile8Bit(file);
  level->creation_date.day   = getFile8Bit(file);

  level->creation_date.src   = DATE_SRC_LEVELFILE;

  return chunk_size;
}

static int LoadLevel_HEAD(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int initial_player_stepsize;
  int initial_player_gravity;
  int i, x, y;

  level->fieldx = getFile8Bit(file);
  level->fieldy = getFile8Bit(file);

  level->time		= getFile16BitBE(file);
  level->gems_needed	= getFile16BitBE(file);

  for (i = 0; i < MAX_LEVEL_NAME_LEN; i++)
    level->name[i] = getFile8Bit(file);
  level->name[MAX_LEVEL_NAME_LEN] = 0;

  for (i = 0; i < LEVEL_SCORE_ELEMENTS; i++)
    level->score[i] = getFile8Bit(file);

  level->num_yamyam_contents = STD_ELEMENT_CONTENTS;
  for (i = 0; i < STD_ELEMENT_CONTENTS; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	level->yamyam_content[i].e[x][y] = getMappedElement(getFile8Bit(file));

  level->amoeba_speed		= getFile8Bit(file);
  level->time_magic_wall	= getFile8Bit(file);
  level->time_wheel		= getFile8Bit(file);
  level->amoeba_content		= getMappedElement(getFile8Bit(file));

  initial_player_stepsize	= (getFile8Bit(file) == 1 ? STEPSIZE_FAST :
				   STEPSIZE_NORMAL);

  for (i = 0; i < MAX_PLAYERS; i++)
    level->initial_player_stepsize[i] = initial_player_stepsize;

  initial_player_gravity	= (getFile8Bit(file) == 1 ? TRUE : FALSE);

  for (i = 0; i < MAX_PLAYERS; i++)
    level->initial_player_gravity[i] = initial_player_gravity;

  level->encoding_16bit_field	= (getFile8Bit(file) == 1 ? TRUE : FALSE);
  level->em_slippery_gems	= (getFile8Bit(file) == 1 ? TRUE : FALSE);

  level->use_custom_template	= (getFile8Bit(file) == 1 ? TRUE : FALSE);

  level->block_last_field	= (getFile8Bit(file) == 1 ? TRUE : FALSE);
  level->sp_block_last_field	= (getFile8Bit(file) == 1 ? TRUE : FALSE);
  level->can_move_into_acid_bits = getFile32BitBE(file);
  level->dont_collide_with_bits = getFile8Bit(file);

  level->use_spring_bug		= (getFile8Bit(file) == 1 ? TRUE : FALSE);
  level->use_step_counter	= (getFile8Bit(file) == 1 ? TRUE : FALSE);

  level->instant_relocation	= (getFile8Bit(file) == 1 ? TRUE : FALSE);
  level->can_pass_to_walkable	= (getFile8Bit(file) == 1 ? TRUE : FALSE);
  level->grow_into_diggable	= (getFile8Bit(file) == 1 ? TRUE : FALSE);

  level->game_engine_type	= getFile8Bit(file);

  ReadUnusedBytesFromFile(file, LEVEL_CHUNK_HEAD_UNUSED);

  return chunk_size;
}

static int LoadLevel_NAME(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int i;

  for (i = 0; i < MAX_LEVEL_NAME_LEN; i++)
    level->name[i] = getFile8Bit(file);
  level->name[MAX_LEVEL_NAME_LEN] = 0;

  return chunk_size;
}

static int LoadLevel_AUTH(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int i;

  for (i = 0; i < MAX_LEVEL_AUTHOR_LEN; i++)
    level->author[i] = getFile8Bit(file);
  level->author[MAX_LEVEL_AUTHOR_LEN] = 0;

  return chunk_size;
}

static int LoadLevel_BODY(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int x, y;
  int chunk_size_expected = level->fieldx * level->fieldy;

  /* Note: "chunk_size" was wrong before version 2.0 when elements are
     stored with 16-bit encoding (and should be twice as big then).
     Even worse, playfield data was stored 16-bit when only yamyam content
     contained 16-bit elements and vice versa. */

  if (level->encoding_16bit_field && level->file_version >= FILE_VERSION_2_0)
    chunk_size_expected *= 2;

  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size);
    return chunk_size_expected;
  }

  for (y = 0; y < level->fieldy; y++)
    for (x = 0; x < level->fieldx; x++)
      level->field[x][y] =
	getMappedElement(level->encoding_16bit_field ? getFile16BitBE(file) :
			 getFile8Bit(file));
  return chunk_size;
}

static int LoadLevel_CONT(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int i, x, y;
  int header_size = 4;
  int content_size = MAX_ELEMENT_CONTENTS * 3 * 3;
  int chunk_size_expected = header_size + content_size;

  /* Note: "chunk_size" was wrong before version 2.0 when elements are
     stored with 16-bit encoding (and should be twice as big then).
     Even worse, playfield data was stored 16-bit when only yamyam content
     contained 16-bit elements and vice versa. */

  if (level->encoding_16bit_field && level->file_version >= FILE_VERSION_2_0)
    chunk_size_expected += content_size;

  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size);
    return chunk_size_expected;
  }

  getFile8Bit(file);
  level->num_yamyam_contents = getFile8Bit(file);
  getFile8Bit(file);
  getFile8Bit(file);

  /* correct invalid number of content fields -- should never happen */
  if (level->num_yamyam_contents < 1 ||
      level->num_yamyam_contents > MAX_ELEMENT_CONTENTS)
    level->num_yamyam_contents = STD_ELEMENT_CONTENTS;

  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	level->yamyam_content[i].e[x][y] =
	  getMappedElement(level->encoding_16bit_field ?
			   getFile16BitBE(file) : getFile8Bit(file));
  return chunk_size;
}

static int LoadLevel_CNT2(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int i, x, y;
  int element;
  int num_contents, content_xsize, content_ysize;
  int content_array[MAX_ELEMENT_CONTENTS][3][3];

  element = getMappedElement(getFile16BitBE(file));
  num_contents = getFile8Bit(file);
  content_xsize = getFile8Bit(file);
  content_ysize = getFile8Bit(file);

  ReadUnusedBytesFromFile(file, LEVEL_CHUNK_CNT2_UNUSED);

  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	content_array[i][x][y] = getMappedElement(getFile16BitBE(file));

  /* correct invalid number of content fields -- should never happen */
  if (num_contents < 1 || num_contents > MAX_ELEMENT_CONTENTS)
    num_contents = STD_ELEMENT_CONTENTS;

  if (element == EL_YAMYAM)
  {
    level->num_yamyam_contents = num_contents;

    for (i = 0; i < num_contents; i++)
      for (y = 0; y < 3; y++)
	for (x = 0; x < 3; x++)
	  level->yamyam_content[i].e[x][y] = content_array[i][x][y];
  }
  else if (element == EL_BD_AMOEBA)
  {
    level->amoeba_content = content_array[0][0][0];
  }
  else
  {
    Error(ERR_WARN, "cannot load content for element '%d'", element);
  }

  return chunk_size;
}

static int LoadLevel_CNT3(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int i;
  int element;
  int envelope_nr;
  int envelope_len;
  int chunk_size_expected;

  element = getMappedElement(getFile16BitBE(file));
  if (!IS_ENVELOPE(element))
    element = EL_ENVELOPE_1;

  envelope_nr = element - EL_ENVELOPE_1;

  envelope_len = getFile16BitBE(file);

  level->envelope[envelope_nr].xsize = getFile8Bit(file);
  level->envelope[envelope_nr].ysize = getFile8Bit(file);

  ReadUnusedBytesFromFile(file, LEVEL_CHUNK_CNT3_UNUSED);

  chunk_size_expected = LEVEL_CHUNK_CNT3_SIZE(envelope_len);
  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size - LEVEL_CHUNK_CNT3_HEADER);
    return chunk_size_expected;
  }

  for (i = 0; i < envelope_len; i++)
    level->envelope[envelope_nr].text[i] = getFile8Bit(file);

  return chunk_size;
}

static int LoadLevel_CUS1(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int num_changed_custom_elements = getFile16BitBE(file);
  int chunk_size_expected = 2 + num_changed_custom_elements * 6;
  int i;

  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size - 2);
    return chunk_size_expected;
  }

  for (i = 0; i < num_changed_custom_elements; i++)
  {
    int element = getMappedElement(getFile16BitBE(file));
    int properties = getFile32BitBE(file);

    if (IS_CUSTOM_ELEMENT(element))
      element_info[element].properties[EP_BITFIELD_BASE_NR] = properties;
    else
      Error(ERR_WARN, "invalid custom element number %d", element);

    /* older game versions that wrote level files with CUS1 chunks used
       different default push delay values (not yet stored in level file) */
    element_info[element].push_delay_fixed = 2;
    element_info[element].push_delay_random = 8;
  }

  return chunk_size;
}

static int LoadLevel_CUS2(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int num_changed_custom_elements = getFile16BitBE(file);
  int chunk_size_expected = 2 + num_changed_custom_elements * 4;
  int i;

  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size - 2);
    return chunk_size_expected;
  }

  for (i = 0; i < num_changed_custom_elements; i++)
  {
    int element = getMappedElement(getFile16BitBE(file));
    int custom_target_element = getMappedElement(getFile16BitBE(file));

    if (IS_CUSTOM_ELEMENT(element))
      element_info[element].change->target_element = custom_target_element;
    else
      Error(ERR_WARN, "invalid custom element number %d", element);
  }

  return chunk_size;
}

static int LoadLevel_CUS3(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int num_changed_custom_elements = getFile16BitBE(file);
  int chunk_size_expected = LEVEL_CHUNK_CUS3_SIZE(num_changed_custom_elements);
  int i, j, x, y;

  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size - 2);
    return chunk_size_expected;
  }

  for (i = 0; i < num_changed_custom_elements; i++)
  {
    int element = getMappedElement(getFile16BitBE(file));
    struct ElementInfo *ei = &element_info[element];
    unsigned int event_bits;

    if (!IS_CUSTOM_ELEMENT(element))
    {
      Error(ERR_WARN, "invalid custom element number %d", element);

      element = EL_INTERNAL_DUMMY;
    }

    for (j = 0; j < MAX_ELEMENT_NAME_LEN; j++)
      ei->description[j] = getFile8Bit(file);
    ei->description[MAX_ELEMENT_NAME_LEN] = 0;

    ei->properties[EP_BITFIELD_BASE_NR] = getFile32BitBE(file);

    /* some free bytes for future properties and padding */
    ReadUnusedBytesFromFile(file, 7);

    ei->use_gfx_element = getFile8Bit(file);
    ei->gfx_element_initial = getMappedElement(getFile16BitBE(file));

    ei->collect_score_initial = getFile8Bit(file);
    ei->collect_count_initial = getFile8Bit(file);

    ei->push_delay_fixed = getFile16BitBE(file);
    ei->push_delay_random = getFile16BitBE(file);
    ei->move_delay_fixed = getFile16BitBE(file);
    ei->move_delay_random = getFile16BitBE(file);

    ei->move_pattern = getFile16BitBE(file);
    ei->move_direction_initial = getFile8Bit(file);
    ei->move_stepsize = getFile8Bit(file);

    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	ei->content.e[x][y] = getMappedElement(getFile16BitBE(file));

    event_bits = getFile32BitBE(file);
    for (j = 0; j < NUM_CHANGE_EVENTS; j++)
      if (event_bits & (1 << j))
	ei->change->has_event[j] = TRUE;

    ei->change->target_element = getMappedElement(getFile16BitBE(file));

    ei->change->delay_fixed = getFile16BitBE(file);
    ei->change->delay_random = getFile16BitBE(file);
    ei->change->delay_frames = getFile16BitBE(file);

    ei->change->initial_trigger_element= getMappedElement(getFile16BitBE(file));

    ei->change->explode = getFile8Bit(file);
    ei->change->use_target_content = getFile8Bit(file);
    ei->change->only_if_complete = getFile8Bit(file);
    ei->change->use_random_replace = getFile8Bit(file);

    ei->change->random_percentage = getFile8Bit(file);
    ei->change->replace_when = getFile8Bit(file);

    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	ei->change->target_content.e[x][y] =
	  getMappedElement(getFile16BitBE(file));

    ei->slippery_type = getFile8Bit(file);

    /* some free bytes for future properties and padding */
    ReadUnusedBytesFromFile(file, LEVEL_CPART_CUS3_UNUSED);

    /* mark that this custom element has been modified */
    ei->modified_settings = TRUE;
  }

  return chunk_size;
}

static int LoadLevel_CUS4(FILE *file, int chunk_size, struct LevelInfo *level)
{
  struct ElementInfo *ei;
  int chunk_size_expected;
  int element;
  int i, j, x, y;

  /* ---------- custom element base property values (96 bytes) ------------- */

  element = getMappedElement(getFile16BitBE(file));

  if (!IS_CUSTOM_ELEMENT(element))
  {
    Error(ERR_WARN, "invalid custom element number %d", element);

    ReadUnusedBytesFromFile(file, chunk_size - 2);
    return chunk_size;
  }

  ei = &element_info[element];

  for (i = 0; i < MAX_ELEMENT_NAME_LEN; i++)
    ei->description[i] = getFile8Bit(file);
  ei->description[MAX_ELEMENT_NAME_LEN] = 0;

  ei->properties[EP_BITFIELD_BASE_NR] = getFile32BitBE(file);

  ReadUnusedBytesFromFile(file, 4);	/* reserved for more base properties */

  ei->num_change_pages = getFile8Bit(file);

  chunk_size_expected = LEVEL_CHUNK_CUS4_SIZE(ei->num_change_pages);
  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size - 43);
    return chunk_size_expected;
  }

  ei->ce_value_fixed_initial = getFile16BitBE(file);
  ei->ce_value_random_initial = getFile16BitBE(file);
  ei->use_last_ce_value = getFile8Bit(file);

  ei->use_gfx_element = getFile8Bit(file);
  ei->gfx_element_initial = getMappedElement(getFile16BitBE(file));

  ei->collect_score_initial = getFile8Bit(file);
  ei->collect_count_initial = getFile8Bit(file);

  ei->drop_delay_fixed = getFile8Bit(file);
  ei->push_delay_fixed = getFile8Bit(file);
  ei->drop_delay_random = getFile8Bit(file);
  ei->push_delay_random = getFile8Bit(file);
  ei->move_delay_fixed = getFile16BitBE(file);
  ei->move_delay_random = getFile16BitBE(file);

  /* bits 0 - 15 of "move_pattern" ... */
  ei->move_pattern = getFile16BitBE(file);
  ei->move_direction_initial = getFile8Bit(file);
  ei->move_stepsize = getFile8Bit(file);

  ei->slippery_type = getFile8Bit(file);

  for (y = 0; y < 3; y++)
    for (x = 0; x < 3; x++)
      ei->content.e[x][y] = getMappedElement(getFile16BitBE(file));

  ei->move_enter_element = getMappedElement(getFile16BitBE(file));
  ei->move_leave_element = getMappedElement(getFile16BitBE(file));
  ei->move_leave_type = getFile8Bit(file);

  /* ... bits 16 - 31 of "move_pattern" (not nice, but downward compatible) */
  ei->move_pattern |= (getFile16BitBE(file) << 16);

  ei->access_direction = getFile8Bit(file);

  ei->explosion_delay = getFile8Bit(file);
  ei->ignition_delay = getFile8Bit(file);
  ei->explosion_type = getFile8Bit(file);

  /* some free bytes for future custom property values and padding */
  ReadUnusedBytesFromFile(file, 1);

  /* ---------- change page property values (48 bytes) --------------------- */

  setElementChangePages(ei, ei->num_change_pages);

  for (i = 0; i < ei->num_change_pages; i++)
  {
    struct ElementChangeInfo *change = &ei->change_page[i];
    unsigned int event_bits;

    /* always start with reliable default values */
    setElementChangeInfoToDefaults(change);

    /* bits 0 - 31 of "has_event[]" ... */
    event_bits = getFile32BitBE(file);
    for (j = 0; j < MIN(NUM_CHANGE_EVENTS, 32); j++)
      if (event_bits & (1 << j))
	change->has_event[j] = TRUE;

    change->target_element = getMappedElement(getFile16BitBE(file));

    change->delay_fixed = getFile16BitBE(file);
    change->delay_random = getFile16BitBE(file);
    change->delay_frames = getFile16BitBE(file);

    change->initial_trigger_element = getMappedElement(getFile16BitBE(file));

    change->explode = getFile8Bit(file);
    change->use_target_content = getFile8Bit(file);
    change->only_if_complete = getFile8Bit(file);
    change->use_random_replace = getFile8Bit(file);

    change->random_percentage = getFile8Bit(file);
    change->replace_when = getFile8Bit(file);

    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	change->target_content.e[x][y]= getMappedElement(getFile16BitBE(file));

    change->can_change = getFile8Bit(file);

    change->trigger_side = getFile8Bit(file);

    change->trigger_player = getFile8Bit(file);
    change->trigger_page = getFile8Bit(file);

    change->trigger_page = (change->trigger_page == CH_PAGE_ANY_FILE ?
			    CH_PAGE_ANY : (1 << change->trigger_page));

    change->has_action = getFile8Bit(file);
    change->action_type = getFile8Bit(file);
    change->action_mode = getFile8Bit(file);
    change->action_arg = getFile16BitBE(file);

    /* ... bits 32 - 39 of "has_event[]" (not nice, but downward compatible) */
    event_bits = getFile8Bit(file);
    for (j = 32; j < NUM_CHANGE_EVENTS; j++)
      if (event_bits & (1 << (j - 32)))
	change->has_event[j] = TRUE;
  }

  /* mark this custom element as modified */
  ei->modified_settings = TRUE;

  return chunk_size;
}

static int LoadLevel_GRP1(FILE *file, int chunk_size, struct LevelInfo *level)
{
  struct ElementInfo *ei;
  struct ElementGroupInfo *group;
  int element;
  int i;

  element = getMappedElement(getFile16BitBE(file));

  if (!IS_GROUP_ELEMENT(element))
  {
    Error(ERR_WARN, "invalid group element number %d", element);

    ReadUnusedBytesFromFile(file, chunk_size - 2);
    return chunk_size;
  }

  ei = &element_info[element];

  for (i = 0; i < MAX_ELEMENT_NAME_LEN; i++)
    ei->description[i] = getFile8Bit(file);
  ei->description[MAX_ELEMENT_NAME_LEN] = 0;

  group = element_info[element].group;

  group->num_elements = getFile8Bit(file);

  ei->use_gfx_element = getFile8Bit(file);
  ei->gfx_element_initial = getMappedElement(getFile16BitBE(file));

  group->choice_mode = getFile8Bit(file);

  /* some free bytes for future values and padding */
  ReadUnusedBytesFromFile(file, 3);

  for (i = 0; i < MAX_ELEMENTS_IN_GROUP; i++)
    group->element[i] = getMappedElement(getFile16BitBE(file));

  /* mark this group element as modified */
  element_info[element].modified_settings = TRUE;

  return chunk_size;
}

static int LoadLevel_MicroChunk(FILE *file, struct LevelFileConfigInfo *conf,
				int element, int real_element)
{
  int micro_chunk_size = 0;
  int conf_type = getFile8Bit(file);
  int byte_mask = conf_type & CONF_MASK_BYTES;
  boolean element_found = FALSE;
  int i;

  micro_chunk_size += 1;

  if (byte_mask == CONF_MASK_MULTI_BYTES)
  {
    int num_bytes = getFile16BitBE(file);
    byte *buffer = checked_malloc(num_bytes);

    ReadBytesFromFile(file, buffer, num_bytes);

    for (i = 0; conf[i].data_type != -1; i++)
    {
      if (conf[i].element == element &&
	  conf[i].conf_type == conf_type)
      {
	int data_type = conf[i].data_type;
	int num_entities = num_bytes / CONF_ENTITY_NUM_BYTES(data_type);
	int max_num_entities = conf[i].max_num_entities;

	if (num_entities > max_num_entities)
	{
	  Error(ERR_WARN,
		"truncating number of entities for element %d from %d to %d",
		element, num_entities, max_num_entities);

	  num_entities = max_num_entities;
	}

	if (num_entities == 0 && (data_type == TYPE_ELEMENT_LIST ||
				  data_type == TYPE_CONTENT_LIST))
	{
	  /* for element and content lists, zero entities are not allowed */
	  Error(ERR_WARN, "found empty list of entities for element %d",
		element);

	  /* do not set "num_entities" here to prevent reading behind buffer */

	  *(int *)(conf[i].num_entities) = 1;	/* at least one is required */
	}
	else
	{
	  *(int *)(conf[i].num_entities) = num_entities;
	}

	element_found = TRUE;

	if (data_type == TYPE_STRING)
	{
	  char *string = (char *)(conf[i].value);
	  int j;

	  for (j = 0; j < max_num_entities; j++)
	    string[j] = (j < num_entities ? buffer[j] : '\0');
	}
	else if (data_type == TYPE_ELEMENT_LIST)
	{
	  int *element_array = (int *)(conf[i].value);
	  int j;

	  for (j = 0; j < num_entities; j++)
	    element_array[j] =
	      getMappedElement(CONF_ELEMENTS_ELEMENT(buffer, j));
	}
	else if (data_type == TYPE_CONTENT_LIST)
	{
	  struct Content *content= (struct Content *)(conf[i].value);
	  int c, x, y;

	  for (c = 0; c < num_entities; c++)
	    for (y = 0; y < 3; y++)
	      for (x = 0; x < 3; x++)
		content[c].e[x][y] =
		  getMappedElement(CONF_CONTENTS_ELEMENT(buffer, c, x, y));
	}
	else
	  element_found = FALSE;

	break;
      }
    }

    checked_free(buffer);

    micro_chunk_size += 2 + num_bytes;
  }
  else		/* constant size configuration data (1, 2 or 4 bytes) */
  {
    int value = (byte_mask == CONF_MASK_1_BYTE ? getFile8Bit   (file) :
		 byte_mask == CONF_MASK_2_BYTE ? getFile16BitBE(file) :
		 byte_mask == CONF_MASK_4_BYTE ? getFile32BitBE(file) : 0);

    for (i = 0; conf[i].data_type != -1; i++)
    {
      if (conf[i].element == element &&
	  conf[i].conf_type == conf_type)
      {
	int data_type = conf[i].data_type;

	if (data_type == TYPE_ELEMENT)
	  value = getMappedElement(value);

	if (data_type == TYPE_BOOLEAN)
	  *(boolean *)(conf[i].value) = value;
	else
	  *(int *)    (conf[i].value) = value;

	element_found = TRUE;

	break;
      }
    }

    micro_chunk_size += CONF_VALUE_NUM_BYTES(byte_mask);
  }

  if (!element_found)
  {
    char *error_conf_chunk_bytes =
      (byte_mask == CONF_MASK_1_BYTE ? "CONF_VALUE_8_BIT" :
       byte_mask == CONF_MASK_2_BYTE ? "CONF_VALUE_16_BIT" :
       byte_mask == CONF_MASK_4_BYTE ? "CONF_VALUE_32_BIT" :"CONF_VALUE_BYTES");
    int error_conf_chunk_token = conf_type & CONF_MASK_TOKEN;
    int error_element = real_element;

    Error(ERR_WARN, "cannot load micro chunk '%s(%d)' value for element %d ['%s']",
	  error_conf_chunk_bytes, error_conf_chunk_token,
	  error_element, EL_NAME(error_element));
  }

  return micro_chunk_size;
}

static int LoadLevel_INFO(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int real_chunk_size = 0;

  li = *level;		/* copy level data into temporary buffer */

  while (!feof(file))
  {
    real_chunk_size += LoadLevel_MicroChunk(file, chunk_config_INFO, -1, -1);

    if (real_chunk_size >= chunk_size)
      break;
  }

  *level = li;		/* copy temporary buffer back to level data */

  return real_chunk_size;
}

static int LoadLevel_CONF(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int real_chunk_size = 0;

  li = *level;		/* copy level data into temporary buffer */

  while (!feof(file))
  {
    int element = getMappedElement(getFile16BitBE(file));

    real_chunk_size += 2;
    real_chunk_size += LoadLevel_MicroChunk(file, chunk_config_CONF,
					    element, element);
    if (real_chunk_size >= chunk_size)
      break;
  }

  *level = li;		/* copy temporary buffer back to level data */

  return real_chunk_size;
}

static int LoadLevel_ELEM(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int real_chunk_size = 0;

  li = *level;		/* copy level data into temporary buffer */

  while (!feof(file))
  {
    int element = getMappedElement(getFile16BitBE(file));

    real_chunk_size += 2;
    real_chunk_size += LoadLevel_MicroChunk(file, chunk_config_ELEM,
					    element, element);
    if (real_chunk_size >= chunk_size)
      break;
  }

  *level = li;		/* copy temporary buffer back to level data */

  return real_chunk_size;
}

static int LoadLevel_NOTE(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int element = getMappedElement(getFile16BitBE(file));
  int envelope_nr = element - EL_ENVELOPE_1;
  int real_chunk_size = 2;

  while (!feof(file))
  {
    real_chunk_size += LoadLevel_MicroChunk(file, chunk_config_NOTE,
					    -1, element);

    if (real_chunk_size >= chunk_size)
      break;
  }

  level->envelope[envelope_nr] = xx_envelope;

  return real_chunk_size;
}

static int LoadLevel_CUSX(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int element = getMappedElement(getFile16BitBE(file));
  int real_chunk_size = 2;
  struct ElementInfo *ei = &element_info[element];
  int i;

  xx_ei = *ei;		/* copy element data into temporary buffer */

  xx_ei.num_change_pages = -1;

  while (!feof(file))
  {
    real_chunk_size += LoadLevel_MicroChunk(file, chunk_config_CUSX_base,
					    -1, element);
    if (xx_ei.num_change_pages != -1)
      break;

    if (real_chunk_size >= chunk_size)
      break;
  }

  *ei = xx_ei;

  if (ei->num_change_pages == -1)
  {
    Error(ERR_WARN, "LoadLevel_CUSX(): missing 'num_change_pages' for '%s'",
	  EL_NAME(element));

    ei->num_change_pages = 1;

    setElementChangePages(ei, 1);
    setElementChangeInfoToDefaults(ei->change);

    return real_chunk_size;
  }

  /* initialize number of change pages stored for this custom element */
  setElementChangePages(ei, ei->num_change_pages);
  for (i = 0; i < ei->num_change_pages; i++)
    setElementChangeInfoToDefaults(&ei->change_page[i]);

  /* start with reading properties for the first change page */
  xx_current_change_page = 0;

  while (!feof(file))
  {
    struct ElementChangeInfo *change = &ei->change_page[xx_current_change_page];

    xx_change = *change;	/* copy change data into temporary buffer */

    resetEventBits();		/* reset bits; change page might have changed */

    real_chunk_size += LoadLevel_MicroChunk(file, chunk_config_CUSX_change,
					    -1, element);

    *change = xx_change;

    setEventFlagsFromEventBits(change);

    if (real_chunk_size >= chunk_size)
      break;
  }

  return real_chunk_size;
}

static int LoadLevel_GRPX(FILE *file, int chunk_size, struct LevelInfo *level)
{
  int element = getMappedElement(getFile16BitBE(file));
  int real_chunk_size = 2;
  struct ElementInfo *ei = &element_info[element];
  struct ElementGroupInfo *group = ei->group;

  xx_ei = *ei;		/* copy element data into temporary buffer */
  xx_group = *group;	/* copy group data into temporary buffer */

  while (!feof(file))
  {
    real_chunk_size += LoadLevel_MicroChunk(file, chunk_config_GRPX,
					    -1, element);

    if (real_chunk_size >= chunk_size)
      break;
  }

  *ei = xx_ei;
  *group = xx_group;

  return real_chunk_size;
}

static void LoadLevelFromFileInfo_RND(struct LevelInfo *level,
				      struct LevelFileInfo *level_file_info)
{
  char *filename = level_file_info->filename;
  char cookie[MAX_LINE_LEN];
  char chunk_name[CHUNK_ID_LEN + 1];
  int chunk_size;
  FILE *file;

  if (!(file = fopen(filename, MODE_READ)))
  {
    level->no_valid_file = TRUE;

#if 1
    Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);
#else
    if (level != &level_template)
      Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);
#endif

    return;
  }

  getFileChunkBE(file, chunk_name, NULL);
  if (strEqual(chunk_name, "RND1"))
  {
    getFile32BitBE(file);		/* not used */

    getFileChunkBE(file, chunk_name, NULL);
    if (!strEqual(chunk_name, "CAVE"))
    {
      level->no_valid_file = TRUE;

      Error(ERR_WARN, "unknown format of level file '%s'", filename);
      fclose(file);
      return;
    }
  }
  else	/* check for pre-2.0 file format with cookie string */
  {
    strcpy(cookie, chunk_name);
    fgets(&cookie[4], MAX_LINE_LEN - 4, file);
    if (strlen(cookie) > 0 && cookie[strlen(cookie) - 1] == '\n')
      cookie[strlen(cookie) - 1] = '\0';

    if (!checkCookieString(cookie, LEVEL_COOKIE_TMPL))
    {
      level->no_valid_file = TRUE;

      Error(ERR_WARN, "unknown format of level file '%s'", filename);
      fclose(file);
      return;
    }

    if ((level->file_version = getFileVersionFromCookieString(cookie)) == -1)
    {
      level->no_valid_file = TRUE;

      Error(ERR_WARN, "unsupported version of level file '%s'", filename);
      fclose(file);
      return;
    }

    /* pre-2.0 level files have no game version, so use file version here */
    level->game_version = level->file_version;
  }

  if (level->file_version < FILE_VERSION_1_2)
  {
    /* level files from versions before 1.2.0 without chunk structure */
    LoadLevel_HEAD(file, LEVEL_CHUNK_HEAD_SIZE,         level);
    LoadLevel_BODY(file, level->fieldx * level->fieldy, level);
  }
  else
  {
    static struct
    {
      char *name;
      int size;
      int (*loader)(FILE *, int, struct LevelInfo *);
    }
    chunk_info[] =
    {
      { "VERS", LEVEL_CHUNK_VERS_SIZE,	LoadLevel_VERS },
      { "DATE", LEVEL_CHUNK_DATE_SIZE,	LoadLevel_DATE },
      { "HEAD", LEVEL_CHUNK_HEAD_SIZE,	LoadLevel_HEAD },
      { "NAME", LEVEL_CHUNK_NAME_SIZE,	LoadLevel_NAME },
      { "AUTH", LEVEL_CHUNK_AUTH_SIZE,	LoadLevel_AUTH },
      { "INFO", -1,			LoadLevel_INFO },
      { "BODY", -1,			LoadLevel_BODY },
      { "CONT", -1,			LoadLevel_CONT },
      { "CNT2", LEVEL_CHUNK_CNT2_SIZE,	LoadLevel_CNT2 },
      { "CNT3", -1,			LoadLevel_CNT3 },
      { "CUS1", -1,			LoadLevel_CUS1 },
      { "CUS2", -1,			LoadLevel_CUS2 },
      { "CUS3", -1,			LoadLevel_CUS3 },
      { "CUS4", -1,			LoadLevel_CUS4 },
      { "GRP1", -1,			LoadLevel_GRP1 },
      { "CONF", -1,			LoadLevel_CONF },
      { "ELEM", -1,			LoadLevel_ELEM },
      { "NOTE", -1,			LoadLevel_NOTE },
      { "CUSX", -1,			LoadLevel_CUSX },
      { "GRPX", -1,			LoadLevel_GRPX },

      {  NULL,  0,			NULL }
    };

    while (getFileChunkBE(file, chunk_name, &chunk_size))
    {
      int i = 0;

      while (chunk_info[i].name != NULL &&
	     !strEqual(chunk_name, chunk_info[i].name))
	i++;

      if (chunk_info[i].name == NULL)
      {
	Error(ERR_WARN, "unknown chunk '%s' in level file '%s'",
	      chunk_name, filename);
	ReadUnusedBytesFromFile(file, chunk_size);
      }
      else if (chunk_info[i].size != -1 &&
	       chunk_info[i].size != chunk_size)
      {
	Error(ERR_WARN, "wrong size (%d) of chunk '%s' in level file '%s'",
	      chunk_size, chunk_name, filename);
	ReadUnusedBytesFromFile(file, chunk_size);
      }
      else
      {
	/* call function to load this level chunk */
	int chunk_size_expected =
	  (chunk_info[i].loader)(file, chunk_size, level);

	/* the size of some chunks cannot be checked before reading other
	   chunks first (like "HEAD" and "BODY") that contain some header
	   information, so check them here */
	if (chunk_size_expected != chunk_size)
	{
	  Error(ERR_WARN, "wrong size (%d) of chunk '%s' in level file '%s'",
		chunk_size, chunk_name, filename);
	}
      }
    }
  }

  fclose(file);
}

/* ------------------------------------------------------------------------- */
/* functions for loading EM level                                            */
/* ------------------------------------------------------------------------- */

#if 0

static int map_em_element_yam(int element)
{
  switch (element)
  {
    case 0x00:	return EL_EMPTY;
    case 0x01:	return EL_EMERALD;
    case 0x02:	return EL_DIAMOND;
    case 0x03:	return EL_ROCK;
    case 0x04:	return EL_ROBOT;
    case 0x05:	return EL_SPACESHIP_UP;
    case 0x06:	return EL_BOMB;
    case 0x07:	return EL_BUG_UP;
    case 0x08:	return EL_AMOEBA_DROP;
    case 0x09:	return EL_NUT;
    case 0x0a:	return EL_YAMYAM;
    case 0x0b:	return EL_QUICKSAND_FULL;
    case 0x0c:	return EL_SAND;
    case 0x0d:	return EL_WALL_SLIPPERY;
    case 0x0e:	return EL_STEELWALL;
    case 0x0f:	return EL_WALL;
    case 0x10:	return EL_EM_KEY_1;
    case 0x11:	return EL_EM_KEY_2;
    case 0x12:	return EL_EM_KEY_4;
    case 0x13:	return EL_EM_KEY_3;
    case 0x14:	return EL_MAGIC_WALL;
    case 0x15:	return EL_ROBOT_WHEEL;
    case 0x16:	return EL_DYNAMITE;

    case 0x17:	return EL_EM_KEY_1;			/* EMC */
    case 0x18:	return EL_BUG_UP;			/* EMC */
    case 0x1a:	return EL_DIAMOND;			/* EMC */
    case 0x1b:	return EL_EMERALD;			/* EMC */
    case 0x25:	return EL_NUT;				/* EMC */
    case 0x80:	return EL_EMPTY;			/* EMC */
    case 0x85:	return EL_EM_KEY_1;			/* EMC */
    case 0x86:	return EL_EM_KEY_2;			/* EMC */
    case 0x87:	return EL_EM_KEY_4;			/* EMC */
    case 0x88:	return EL_EM_KEY_3;			/* EMC */
    case 0x94:	return EL_QUICKSAND_EMPTY;		/* EMC */
    case 0x9a:	return EL_AMOEBA_WET;			/* EMC */
    case 0xaf:	return EL_DYNAMITE;			/* EMC */
    case 0xbd:	return EL_SAND;				/* EMC */

    default:
      Error(ERR_WARN, "invalid level element %d", element);
      return EL_UNKNOWN;
  }
}

static int map_em_element_field(int element)
{
  if (element >= 0xc8 && element <= 0xe1)
    return EL_CHAR_A + (element - 0xc8);
  else if (element >= 0xe2 && element <= 0xeb)
    return EL_CHAR_0 + (element - 0xe2);

  switch (element)
  {
    case 0x00:	return EL_ROCK;
    case 0x01:	return EL_ROCK;				/* EMC */
    case 0x02:	return EL_DIAMOND;
    case 0x03:	return EL_DIAMOND;
    case 0x04:	return EL_ROBOT;
    case 0x05:	return EL_ROBOT;			/* EMC */
    case 0x06:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x07:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x08:	return EL_SPACESHIP_UP;
    case 0x09:	return EL_SPACESHIP_RIGHT;
    case 0x0a:	return EL_SPACESHIP_DOWN;
    case 0x0b:	return EL_SPACESHIP_LEFT;
    case 0x0c:	return EL_SPACESHIP_UP;
    case 0x0d:	return EL_SPACESHIP_RIGHT;
    case 0x0e:	return EL_SPACESHIP_DOWN;
    case 0x0f:	return EL_SPACESHIP_LEFT;

    case 0x10:	return EL_BOMB;
    case 0x11:	return EL_BOMB;				/* EMC */
    case 0x12:	return EL_EMERALD;
    case 0x13:	return EL_EMERALD;
    case 0x14:	return EL_BUG_UP;
    case 0x15:	return EL_BUG_RIGHT;
    case 0x16:	return EL_BUG_DOWN;
    case 0x17:	return EL_BUG_LEFT;
    case 0x18:	return EL_BUG_UP;
    case 0x19:	return EL_BUG_RIGHT;
    case 0x1a:	return EL_BUG_DOWN;
    case 0x1b:	return EL_BUG_LEFT;
    case 0x1c:	return EL_AMOEBA_DROP;
    case 0x1d:	return EL_AMOEBA_DROP;			/* EMC */
    case 0x1e:	return EL_AMOEBA_DROP;			/* EMC */
    case 0x1f:	return EL_AMOEBA_DROP;			/* EMC */

    case 0x20:	return EL_ROCK;
    case 0x21:	return EL_BOMB;				/* EMC */
    case 0x22:	return EL_DIAMOND;			/* EMC */
    case 0x23:	return EL_EMERALD;			/* EMC */
    case 0x24:	return EL_MAGIC_WALL;
    case 0x25:	return EL_NUT;
    case 0x26:	return EL_NUT;				/* EMC */
    case 0x27:	return EL_NUT;				/* EMC */

      /* looks like magic wheel, but is _always_ activated */
    case 0x28:	return EL_ROBOT_WHEEL;			/* EMC */

    case 0x29:	return EL_YAMYAM;	/* up */
    case 0x2a:	return EL_YAMYAM;	/* down */
    case 0x2b:	return EL_YAMYAM;	/* left */	/* EMC */
    case 0x2c:	return EL_YAMYAM;	/* right */	/* EMC */
    case 0x2d:	return EL_QUICKSAND_FULL;
    case 0x2e:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x2f:	return EL_EMPTY_SPACE;			/* EMC */

    case 0x30:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x31:	return EL_SAND;				/* EMC */
    case 0x32:	return EL_SAND;				/* EMC */
    case 0x33:	return EL_SAND;				/* EMC */
    case 0x34:	return EL_QUICKSAND_FULL;		/* EMC */
    case 0x35:	return EL_QUICKSAND_FULL;		/* EMC */
    case 0x36:	return EL_QUICKSAND_FULL;		/* EMC */
    case 0x37:	return EL_SAND;				/* EMC */
    case 0x38:	return EL_ROCK;				/* EMC */
    case 0x39:	return EL_EXPANDABLE_WALL_HORIZONTAL;	/* EMC */
    case 0x3a:	return EL_EXPANDABLE_WALL_VERTICAL;	/* EMC */
    case 0x3b:	return EL_DYNAMITE_ACTIVE;	/* 1 */
    case 0x3c:	return EL_DYNAMITE_ACTIVE;	/* 2 */
    case 0x3d:	return EL_DYNAMITE_ACTIVE;	/* 3 */
    case 0x3e:	return EL_DYNAMITE_ACTIVE;	/* 4 */
    case 0x3f:	return EL_ACID_POOL_BOTTOM;

    case 0x40:	return EL_EXIT_OPEN;	/* 1 */
    case 0x41:	return EL_EXIT_OPEN;	/* 2 */
    case 0x42:	return EL_EXIT_OPEN;	/* 3 */
    case 0x43:	return EL_BALLOON;			/* EMC */
    case 0x44:	return EL_UNKNOWN;			/* EMC ("plant") */
    case 0x45:	return EL_SPRING;			/* EMC */
    case 0x46:	return EL_SPRING;	/* falling */	/* EMC */
    case 0x47:	return EL_SPRING;	/* left */	/* EMC */
    case 0x48:	return EL_SPRING;	/* right */	/* EMC */
    case 0x49:	return EL_UNKNOWN;			/* EMC ("ball 1") */
    case 0x4a:	return EL_UNKNOWN;			/* EMC ("ball 2") */
    case 0x4b:	return EL_UNKNOWN;			/* EMC ("android") */
    case 0x4c:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x4d:	return EL_UNKNOWN;			/* EMC ("android") */
    case 0x4e:	return EL_INVISIBLE_WALL;		/* EMC (? "android") */
    case 0x4f:	return EL_UNKNOWN;			/* EMC ("android") */

    case 0x50:	return EL_UNKNOWN;			/* EMC ("android") */
    case 0x51:	return EL_UNKNOWN;			/* EMC ("android") */
    case 0x52:	return EL_UNKNOWN;			/* EMC ("android") */
    case 0x53:	return EL_UNKNOWN;			/* EMC ("android") */
    case 0x54:	return EL_UNKNOWN;			/* EMC ("android") */
    case 0x55:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x56:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x57:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x58:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x59:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x5a:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x5b:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x5c:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x5d:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x5e:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x5f:	return EL_EMPTY_SPACE;			/* EMC */

    case 0x60:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x61:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x62:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x63:	return EL_SPRING;	/* left */	/* EMC */
    case 0x64:	return EL_SPRING;	/* right */	/* EMC */
    case 0x65:	return EL_ACID;		/* 1 */		/* EMC */
    case 0x66:	return EL_ACID;		/* 2 */		/* EMC */
    case 0x67:	return EL_ACID;		/* 3 */		/* EMC */
    case 0x68:	return EL_ACID;		/* 4 */		/* EMC */
    case 0x69:	return EL_ACID;		/* 5 */		/* EMC */
    case 0x6a:	return EL_ACID;		/* 6 */		/* EMC */
    case 0x6b:	return EL_ACID;		/* 7 */		/* EMC */
    case 0x6c:	return EL_ACID;		/* 8 */		/* EMC */
    case 0x6d:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x6e:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x6f:	return EL_EMPTY_SPACE;			/* EMC */

    case 0x70:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x71:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x72:	return EL_NUT;		/* left */	/* EMC */
    case 0x73:	return EL_SAND;				/* EMC (? "nut") */
    case 0x74:	return EL_STEELWALL;
    case 0x75:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x76:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x77:	return EL_BOMB;		/* left */	/* EMC */
    case 0x78:	return EL_BOMB;		/* right */	/* EMC */
    case 0x79:	return EL_ROCK;		/* left */	/* EMC */
    case 0x7a:	return EL_ROCK;		/* right */	/* EMC */
    case 0x7b:	return EL_ACID;				/* (? EMC "blank") */
    case 0x7c:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x7d:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x7e:	return EL_EMPTY_SPACE;			/* EMC */
    case 0x7f:	return EL_EMPTY_SPACE;			/* EMC */

    case 0x80:	return EL_EMPTY;
    case 0x81:	return EL_WALL_SLIPPERY;
    case 0x82:	return EL_SAND;
    case 0x83:	return EL_STEELWALL;
    case 0x84:	return EL_WALL;
    case 0x85:	return EL_EM_KEY_1;
    case 0x86:	return EL_EM_KEY_2;
    case 0x87:	return EL_EM_KEY_4;
    case 0x88:	return EL_EM_KEY_3;
    case 0x89:	return EL_EM_GATE_1;
    case 0x8a:	return EL_EM_GATE_2;
    case 0x8b:	return EL_EM_GATE_4;
    case 0x8c:	return EL_EM_GATE_3;
    case 0x8d:	return EL_INVISIBLE_WALL;		/* EMC (? "dripper") */
    case 0x8e:	return EL_EM_GATE_1_GRAY;
    case 0x8f:	return EL_EM_GATE_2_GRAY;

    case 0x90:	return EL_EM_GATE_4_GRAY;
    case 0x91:	return EL_EM_GATE_3_GRAY;
    case 0x92:	return EL_MAGIC_WALL;
    case 0x93:	return EL_ROBOT_WHEEL;
    case 0x94:	return EL_QUICKSAND_EMPTY;		/* (? EMC "sand") */
    case 0x95:	return EL_ACID_POOL_TOPLEFT;
    case 0x96:	return EL_ACID_POOL_TOPRIGHT;
    case 0x97:	return EL_ACID_POOL_BOTTOMLEFT;
    case 0x98:	return EL_ACID_POOL_BOTTOMRIGHT;
    case 0x99:	return EL_ACID;			/* (? EMC "fake blank") */
    case 0x9a:	return EL_AMOEBA_DEAD;		/* 1 */
    case 0x9b:	return EL_AMOEBA_DEAD;		/* 2 */
    case 0x9c:	return EL_AMOEBA_DEAD;		/* 3 */
    case 0x9d:	return EL_AMOEBA_DEAD;		/* 4 */
    case 0x9e:	return EL_EXIT_CLOSED;
    case 0x9f:	return EL_CHAR_LESS;		/* arrow left */

      /* looks like normal sand, but behaves like wall */
    case 0xa0:	return EL_UNKNOWN;		/* EMC ("fake grass") */
    case 0xa1:	return EL_UNKNOWN;		/* EMC ("lenses") */
    case 0xa2:	return EL_UNKNOWN;		/* EMC ("magnify") */
    case 0xa3:	return EL_UNKNOWN;		/* EMC ("fake blank") */
    case 0xa4:	return EL_UNKNOWN;		/* EMC ("fake grass") */
    case 0xa5:	return EL_UNKNOWN;		/* EMC ("switch") */
    case 0xa6:	return EL_UNKNOWN;		/* EMC ("switch") */
    case 0xa7:	return EL_EMPTY_SPACE;			/* EMC */
    case 0xa8:	return EL_EMC_WALL_1;			/* EMC ("decor 8") */
    case 0xa9:	return EL_EMC_WALL_2;			/* EMC ("decor 9") */
    case 0xaa:	return EL_EMC_WALL_3;			/* EMC ("decor 10") */
    case 0xab:	return EL_EMC_WALL_7;			/* EMC ("decor 5") */
    case 0xac:	return EL_CHAR_COMMA;			/* EMC */
    case 0xad:	return EL_CHAR_QUOTEDBL;		/* EMC */
    case 0xae:	return EL_CHAR_MINUS;			/* EMC */
    case 0xaf:	return EL_DYNAMITE;

    case 0xb0:	return EL_EMC_STEELWALL_1;		/* EMC ("steel 3") */
    case 0xb1:	return EL_EMC_WALL_8;			/* EMC ("decor 6") */
    case 0xb2:	return EL_UNKNOWN;			/* EMC ("decor 7") */
    case 0xb3:	return EL_STEELWALL;		/* 2 */	/* EMC */
    case 0xb4:	return EL_WALL_SLIPPERY;	/* 2 */	/* EMC */
    case 0xb5:	return EL_EMC_WALL_6;			/* EMC ("decor 2") */
    case 0xb6:	return EL_EMC_WALL_5;			/* EMC ("decor 4") */
    case 0xb7:	return EL_EMC_WALL_4;			/* EMC ("decor 3") */
    case 0xb8:	return EL_BALLOON_SWITCH_ANY;		/* EMC */
    case 0xb9:	return EL_BALLOON_SWITCH_RIGHT;		/* EMC */
    case 0xba:	return EL_BALLOON_SWITCH_DOWN;		/* EMC */
    case 0xbb:	return EL_BALLOON_SWITCH_LEFT;		/* EMC */
    case 0xbc:	return EL_BALLOON_SWITCH_UP;		/* EMC */
    case 0xbd:	return EL_SAND;				/* EMC ("dirt") */
    case 0xbe:	return EL_UNKNOWN;			/* EMC ("plant") */
    case 0xbf:	return EL_UNKNOWN;			/* EMC ("key 5") */

    case 0xc0:	return EL_UNKNOWN;			/* EMC ("key 6") */
    case 0xc1:	return EL_UNKNOWN;			/* EMC ("key 7") */
    case 0xc2:	return EL_UNKNOWN;			/* EMC ("key 8") */
    case 0xc3:	return EL_UNKNOWN;			/* EMC ("door 5") */
    case 0xc4:	return EL_UNKNOWN;			/* EMC ("door 6") */
    case 0xc5:	return EL_UNKNOWN;			/* EMC ("door 7") */
    case 0xc6:	return EL_UNKNOWN;			/* EMC ("door 8") */
    case 0xc7:	return EL_UNKNOWN;			/* EMC ("bumper") */

      /* characters: see above */

    case 0xec:	return EL_CHAR_PERIOD;
    case 0xed:	return EL_CHAR_EXCLAM;
    case 0xee:	return EL_CHAR_COLON;
    case 0xef:	return EL_CHAR_QUESTION;

    case 0xf0:	return EL_CHAR_GREATER;			/* arrow right */
    case 0xf1:	return EL_CHAR_COPYRIGHT;		/* EMC: "decor 1" */
    case 0xf2:	return EL_UNKNOWN;		/* EMC ("fake door 5") */
    case 0xf3:	return EL_UNKNOWN;		/* EMC ("fake door 6") */
    case 0xf4:	return EL_UNKNOWN;		/* EMC ("fake door 7") */
    case 0xf5:	return EL_UNKNOWN;		/* EMC ("fake door 8") */
    case 0xf6:	return EL_EMPTY_SPACE;			/* EMC */
    case 0xf7:	return EL_EMPTY_SPACE;			/* EMC */

    case 0xf8:	return EL_EMPTY_SPACE;			/* EMC */
    case 0xf9:	return EL_EMPTY_SPACE;			/* EMC */
    case 0xfa:	return EL_EMPTY_SPACE;			/* EMC */
    case 0xfb:	return EL_EMPTY_SPACE;			/* EMC */
    case 0xfc:	return EL_EMPTY_SPACE;			/* EMC */
    case 0xfd:	return EL_EMPTY_SPACE;			/* EMC */

    case 0xfe:	return EL_PLAYER_1;			/* EMC: "blank" */
    case 0xff:	return EL_PLAYER_2;			/* EMC: "blank" */

    default:
      /* should never happen (all 8-bit value cases should be handled) */
      Error(ERR_WARN, "invalid level element %d", element);
      return EL_UNKNOWN;
  }
}

#define EM_LEVEL_SIZE			2106
#define EM_LEVEL_XSIZE			64
#define EM_LEVEL_YSIZE			32

static void OLD_LoadLevelFromFileInfo_EM(struct LevelInfo *level,
					 struct LevelFileInfo *level_file_info)
{
  char *filename = level_file_info->filename;
  FILE *file;
  unsigned char leveldata[EM_LEVEL_SIZE];
  unsigned char *header = &leveldata[EM_LEVEL_XSIZE * EM_LEVEL_YSIZE];
  int nr = level_file_info->nr;
  int i, x, y;

  if (!(file = fopen(filename, MODE_READ)))
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);

    return;
  }

  for (i = 0; i < EM_LEVEL_SIZE; i++)
    leveldata[i] = fgetc(file);

  fclose(file);

  /* check if level data is crypted by testing against known starting bytes
     of the few existing crypted level files (from Emerald Mine 1 + 2) */

  if ((leveldata[0] == 0xf1 ||
       leveldata[0] == 0xf5) && leveldata[2] == 0xe7 && leveldata[3] == 0xee)
  {
    unsigned char code0 = 0x65;
    unsigned char code1 = 0x11;

    if (leveldata[0] == 0xf5)	/* error in crypted Emerald Mine 2 levels */
      leveldata[0] = 0xf1;

    /* decode crypted level data */

    for (i = 0; i < EM_LEVEL_SIZE; i++)
    {
      leveldata[i] ^= code0;
      leveldata[i] -= code1;

      code0 = (code0 + 7) & 0xff;
    }
  }

  level->fieldx	= EM_LEVEL_XSIZE;
  level->fieldy	= EM_LEVEL_YSIZE;

  level->time		= header[46] * 10;
  level->gems_needed	= header[47];

  /* The original Emerald Mine levels have their level number stored
     at the second byte of the level file...
     Do not trust this information at other level files, e.g. EMC,
     but correct it anyway (normally the first row is completely
     steel wall, so the correction does not hurt anyway). */

  if (leveldata[1] == nr)
    leveldata[1] = leveldata[2];	/* correct level number field */

  sprintf(level->name, "Level %d", nr);		/* set level name */

  level->score[SC_EMERALD]	= header[36];
  level->score[SC_DIAMOND]	= header[37];
  level->score[SC_ROBOT]	= header[38];
  level->score[SC_SPACESHIP]	= header[39];
  level->score[SC_BUG]		= header[40];
  level->score[SC_YAMYAM]	= header[41];
  level->score[SC_NUT]		= header[42];
  level->score[SC_DYNAMITE]	= header[43];
  level->score[SC_TIME_BONUS]	= header[44];

  level->num_yamyam_contents = 4;

  for (i = 0; i < level->num_yamyam_contents; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	level->yamyam_content[i].e[x][y] =
	  map_em_element_yam(header[i * 9 + y * 3 + x]);

  level->amoeba_speed		= (header[52] * 256 + header[53]) % 256;
  level->time_magic_wall	= (header[54] * 256 + header[55]) * 16 / 100;
  level->time_wheel		= (header[56] * 256 + header[57]) * 16 / 100;
  level->amoeba_content		= EL_DIAMOND;

  for (y = 0; y < level->fieldy; y++) for (x = 0; x < level->fieldx; x++)
  {
    int new_element = map_em_element_field(leveldata[y * EM_LEVEL_XSIZE + x]);

    if (new_element == EL_AMOEBA_DEAD && level->amoeba_speed)
      new_element = EL_AMOEBA_WET;

    level->field[x][y] = new_element;
  }

  x = (header[48] * 256 + header[49]) % EM_LEVEL_XSIZE;
  y = (header[48] * 256 + header[49]) / EM_LEVEL_XSIZE;
  level->field[x][y] = EL_PLAYER_1;

  x = (header[50] * 256 + header[51]) % EM_LEVEL_XSIZE;
  y = (header[50] * 256 + header[51]) / EM_LEVEL_XSIZE;
  level->field[x][y] = EL_PLAYER_2;
}

#endif

void CopyNativeLevel_RND_to_EM(struct LevelInfo *level)
{
  static int ball_xy[8][2] =
  {
    { 0, 0 },
    { 1, 0 },
    { 2, 0 },
    { 0, 1 },
    { 2, 1 },
    { 0, 2 },
    { 1, 2 },
    { 2, 2 },
  };
  struct LevelInfo_EM *level_em = level->native_em_level;
  struct LEVEL *lev = level_em->lev;
  struct PLAYER **ply = level_em->ply;
  int i, j, x, y;

  lev->width  = MIN(level->fieldx, EM_MAX_CAVE_WIDTH);
  lev->height = MIN(level->fieldy, EM_MAX_CAVE_HEIGHT);

  lev->time_seconds     = level->time;
  lev->required_initial = level->gems_needed;

  lev->emerald_score	= level->score[SC_EMERALD];
  lev->diamond_score	= level->score[SC_DIAMOND];
  lev->alien_score	= level->score[SC_ROBOT];
  lev->tank_score	= level->score[SC_SPACESHIP];
  lev->bug_score	= level->score[SC_BUG];
  lev->eater_score	= level->score[SC_YAMYAM];
  lev->nut_score	= level->score[SC_NUT];
  lev->dynamite_score	= level->score[SC_DYNAMITE];
  lev->key_score	= level->score[SC_KEY];
  lev->exit_score	= level->score[SC_TIME_BONUS];

  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	lev->eater_array[i][y * 3 + x] =
	  map_element_RND_to_EM(level->yamyam_content[i].e[x][y]);

  lev->amoeba_time		= level->amoeba_speed;
  lev->wonderwall_time_initial	= level->time_magic_wall;
  lev->wheel_time		= level->time_wheel;

  lev->android_move_time	= level->android_move_time;
  lev->android_clone_time	= level->android_clone_time;
  lev->ball_random		= level->ball_random;
  lev->ball_state_initial	= level->ball_state_initial;
  lev->ball_time		= level->ball_time;
  lev->num_ball_arrays		= level->num_ball_contents;

  lev->lenses_score		= level->lenses_score;
  lev->magnify_score		= level->magnify_score;
  lev->slurp_score		= level->slurp_score;

  lev->lenses_time		= level->lenses_time;
  lev->magnify_time		= level->magnify_time;

  lev->wind_direction_initial =
    map_direction_RND_to_EM(level->wind_direction_initial);
  lev->wind_cnt_initial = (level->wind_direction_initial != MV_NONE ?
			   lev->wind_time : 0);

  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (j = 0; j < 8; j++)
      lev->ball_array[i][j] =
	map_element_RND_to_EM(level->
			      ball_content[i].e[ball_xy[j][0]][ball_xy[j][1]]);

  map_android_clone_elements_RND_to_EM(level);

  /* first fill the complete playfield with the default border element */
  for (y = 0; y < EM_MAX_CAVE_HEIGHT; y++)
    for (x = 0; x < EM_MAX_CAVE_WIDTH; x++)
      level_em->cave[x][y] = ZBORDER;

  if (BorderElement == EL_STEELWALL)
  {
    for (y = 0; y < lev->height + 2; y++)
      for (x = 0; x < lev->width + 2; x++)
	level_em->cave[x + 1][y + 1] = map_element_RND_to_EM(EL_STEELWALL);
  }

  /* then copy the real level contents from level file into the playfield */
  for (y = 0; y < lev->height; y++) for (x = 0; x < lev->width; x++)
  {
    int new_element = map_element_RND_to_EM(level->field[x][y]);
    int offset = (BorderElement == EL_STEELWALL ? 1 : 0);
    int xx = x + 1 + offset;
    int yy = y + 1 + offset;

    if (level->field[x][y] == EL_AMOEBA_DEAD)
      new_element = map_element_RND_to_EM(EL_AMOEBA_WET);

    level_em->cave[xx][yy] = new_element;
  }

  for (i = 0; i < MAX_PLAYERS; i++)
  {
    ply[i]->x_initial = 0;
    ply[i]->y_initial = 0;
  }

  /* initialize player positions and delete players from the playfield */
  for (y = 0; y < lev->height; y++) for (x = 0; x < lev->width; x++)
  {
    if (ELEM_IS_PLAYER(level->field[x][y]))
    {
      int player_nr = GET_PLAYER_NR(level->field[x][y]);
      int offset = (BorderElement == EL_STEELWALL ? 1 : 0);
      int xx = x + 1 + offset;
      int yy = y + 1 + offset;

      ply[player_nr]->x_initial = xx;
      ply[player_nr]->y_initial = yy;

      level_em->cave[xx][yy] = map_element_RND_to_EM(EL_EMPTY);
    }
  }

  if (BorderElement == EL_STEELWALL)
  {
    lev->width  += 2;
    lev->height += 2;
  }
}

void CopyNativeLevel_EM_to_RND(struct LevelInfo *level)
{
  static int ball_xy[8][2] =
  {
    { 0, 0 },
    { 1, 0 },
    { 2, 0 },
    { 0, 1 },
    { 2, 1 },
    { 0, 2 },
    { 1, 2 },
    { 2, 2 },
  };
  struct LevelInfo_EM *level_em = level->native_em_level;
  struct LEVEL *lev = level_em->lev;
  struct PLAYER **ply = level_em->ply;
  int i, j, x, y;

  level->fieldx = MIN(lev->width,  MAX_LEV_FIELDX);
  level->fieldy = MIN(lev->height, MAX_LEV_FIELDY);

  level->time        = lev->time_seconds;
  level->gems_needed = lev->required_initial;

  sprintf(level->name, "Level %d", level->file_info.nr);

  level->score[SC_EMERALD]	= lev->emerald_score;
  level->score[SC_DIAMOND]	= lev->diamond_score;
  level->score[SC_ROBOT]	= lev->alien_score;
  level->score[SC_SPACESHIP]	= lev->tank_score;
  level->score[SC_BUG]		= lev->bug_score;
  level->score[SC_YAMYAM]	= lev->eater_score;
  level->score[SC_NUT]		= lev->nut_score;
  level->score[SC_DYNAMITE]	= lev->dynamite_score;
  level->score[SC_KEY]		= lev->key_score;
  level->score[SC_TIME_BONUS]	= lev->exit_score;

  level->num_yamyam_contents = MAX_ELEMENT_CONTENTS;

  for (i = 0; i < level->num_yamyam_contents; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	level->yamyam_content[i].e[x][y] =
	  map_element_EM_to_RND(lev->eater_array[i][y * 3 + x]);

  level->amoeba_speed		= lev->amoeba_time;
  level->time_magic_wall	= lev->wonderwall_time_initial;
  level->time_wheel		= lev->wheel_time;

  level->android_move_time	= lev->android_move_time;
  level->android_clone_time	= lev->android_clone_time;
  level->ball_random		= lev->ball_random;
  level->ball_state_initial	= lev->ball_state_initial;
  level->ball_time		= lev->ball_time;
  level->num_ball_contents	= lev->num_ball_arrays;

  level->lenses_score		= lev->lenses_score;
  level->magnify_score		= lev->magnify_score;
  level->slurp_score		= lev->slurp_score;

  level->lenses_time		= lev->lenses_time;
  level->magnify_time		= lev->magnify_time;

  level->wind_direction_initial =
    map_direction_EM_to_RND(lev->wind_direction_initial);

  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (j = 0; j < 8; j++)
      level->ball_content[i].e[ball_xy[j][0]][ball_xy[j][1]] =
	map_element_EM_to_RND(lev->ball_array[i][j]);

  map_android_clone_elements_EM_to_RND(level);

  /* convert the playfield (some elements need special treatment) */
  for (y = 0; y < level->fieldy; y++) for (x = 0; x < level->fieldx; x++)
  {
    int new_element = map_element_EM_to_RND(level_em->cave[x + 1][y + 1]);

    if (new_element == EL_AMOEBA_WET && level->amoeba_speed == 0)
      new_element = EL_AMOEBA_DEAD;

    level->field[x][y] = new_element;
  }

  for (i = 0; i < MAX_PLAYERS; i++)
  {
    /* in case of all players set to the same field, use the first player */
    int nr = MAX_PLAYERS - i - 1;
    int jx = ply[nr]->x_initial - 1;
    int jy = ply[nr]->y_initial - 1;

    if (jx != -1 && jy != -1)
      level->field[jx][jy] = EL_PLAYER_1 + nr;
  }
}


/* ------------------------------------------------------------------------- */
/* functions for loading SP level                                            */
/* ------------------------------------------------------------------------- */

#if 0

#define NUM_SUPAPLEX_LEVELS_PER_PACKAGE	111
#define SP_LEVEL_SIZE			1536
#define SP_LEVEL_XSIZE			60
#define SP_LEVEL_YSIZE			24
#define SP_LEVEL_NAME_LEN		23

static void LoadLevelFromFileStream_SP(FILE *file, struct LevelInfo *level,
				       int nr)
{
  int initial_player_gravity;
  int num_special_ports;
  int i, x, y;

  /* for details of the Supaplex level format, see Herman Perk's Supaplex
     documentation file "SPFIX63.DOC" from his Supaplex "SpeedFix" package */

  /* read level body (width * height == 60 * 24 tiles == 1440 bytes) */
  for (y = 0; y < SP_LEVEL_YSIZE; y++)
  {
    for (x = 0; x < SP_LEVEL_XSIZE; x++)
    {
      int element_old = fgetc(file);
      int element_new;

      if (element_old <= 0x27)
	element_new = getMappedElement(EL_SP_START + element_old);
      else if (element_old == 0x28)
	element_new = EL_INVISIBLE_WALL;
      else
      {
	Error(ERR_WARN, "in level %d, at position %d, %d:", nr, x, y);
	Error(ERR_WARN, "invalid level element %d", element_old);

	element_new = EL_UNKNOWN;
      }

      level->field[x][y] = element_new;
    }
  }

  ReadUnusedBytesFromFile(file, 4);	/* (not used by Supaplex engine) */

  /* initial gravity: 1 == "on", anything else (0) == "off" */
  initial_player_gravity = (fgetc(file) == 1 ? TRUE : FALSE);

  for (i = 0; i < MAX_PLAYERS; i++)
    level->initial_player_gravity[i] = initial_player_gravity;

  ReadUnusedBytesFromFile(file, 1);	/* (not used by Supaplex engine) */

  /* level title in uppercase letters, padded with dashes ("-") (23 bytes) */
  for (i = 0; i < SP_LEVEL_NAME_LEN; i++)
    level->name[i] = fgetc(file);
  level->name[SP_LEVEL_NAME_LEN] = '\0';

  /* initial "freeze zonks": 2 == "on", anything else (0, 1) == "off" */
  ReadUnusedBytesFromFile(file, 1);	/* (not used by R'n'D engine) */

  /* number of infotrons needed; 0 means that Supaplex will count the total
     amount of infotrons in the level and use the low byte of that number
     (a multiple of 256 infotrons will result in "0 infotrons needed"!) */
  level->gems_needed = fgetc(file);

  /* number of special ("gravity") port entries below (maximum 10 allowed) */
  num_special_ports = fgetc(file);

  /* database of properties of up to 10 special ports (6 bytes per port) */
  for (i = 0; i < 10; i++)
  {
    int port_location, port_x, port_y, port_element;
    int gravity;

    /* high and low byte of the location of a special port; if (x, y) are the
       coordinates of a port in the field and (0, 0) is the top-left corner,
       the 16 bit value here calculates as 2 * (x + (y * 60)) (this is twice
       of what may be expected: Supaplex works with a game field in memory
       which is 2 bytes per tile) */
    port_location = getFile16BitBE(file);

    /* change gravity: 1 == "turn on", anything else (0) == "turn off" */
    gravity = fgetc(file);

    /* "freeze zonks": 2 == "turn on", anything else (0, 1) == "turn off" */
    ReadUnusedBytesFromFile(file, 1);	/* (not used by R'n'D engine) */

    /* "freeze enemies": 1 == "turn on", anything else (0) == "turn off" */
    ReadUnusedBytesFromFile(file, 1);	/* (not used by R'n'D engine) */

    ReadUnusedBytesFromFile(file, 1);	/* (not used by Supaplex engine) */

    if (i >= num_special_ports)
      continue;

    port_x = (port_location / 2) % SP_LEVEL_XSIZE;
    port_y = (port_location / 2) / SP_LEVEL_XSIZE;

    if (port_x < 0 || port_x >= SP_LEVEL_XSIZE ||
	port_y < 0 || port_y >= SP_LEVEL_YSIZE)
    {
      Error(ERR_WARN, "special port position (%d, %d) out of bounds",
	    port_x, port_y);

      continue;
    }

    port_element = level->field[port_x][port_y];

    if (port_element < EL_SP_GRAVITY_PORT_RIGHT ||
	port_element > EL_SP_GRAVITY_PORT_UP)
    {
      Error(ERR_WARN, "no special port at position (%d, %d)", port_x, port_y);

      continue;
    }

    /* change previous (wrong) gravity inverting special port to either
       gravity enabling special port or gravity disabling special port */
    level->field[port_x][port_y] +=
      (gravity == 1 ? EL_SP_GRAVITY_ON_PORT_RIGHT :
       EL_SP_GRAVITY_OFF_PORT_RIGHT) - EL_SP_GRAVITY_PORT_RIGHT;
  }

  ReadUnusedBytesFromFile(file, 4);	/* (not used by Supaplex engine) */

  /* change special gravity ports without database entries to normal ports */
  for (y = 0; y < SP_LEVEL_YSIZE; y++)
    for (x = 0; x < SP_LEVEL_XSIZE; x++)
      if (level->field[x][y] >= EL_SP_GRAVITY_PORT_RIGHT &&
	  level->field[x][y] <= EL_SP_GRAVITY_PORT_UP)
	level->field[x][y] += EL_SP_PORT_RIGHT - EL_SP_GRAVITY_PORT_RIGHT;

  /* auto-determine number of infotrons if it was stored as "0" -- see above */
  if (level->gems_needed == 0)
  {
    for (y = 0; y < SP_LEVEL_YSIZE; y++)
      for (x = 0; x < SP_LEVEL_XSIZE; x++)
	if (level->field[x][y] == EL_SP_INFOTRON)
	  level->gems_needed++;

    level->gems_needed &= 0xff;		/* only use low byte -- see above */
  }

  level->fieldx = SP_LEVEL_XSIZE;
  level->fieldy = SP_LEVEL_YSIZE;

  level->time = 0;			/* no time limit */
  level->amoeba_speed = 0;
  level->time_magic_wall = 0;
  level->time_wheel = 0;
  level->amoeba_content = EL_EMPTY;

#if 1
  /* original Supaplex does not use score values -- use default values */
#else
  for (i = 0; i < LEVEL_SCORE_ELEMENTS; i++)
    level->score[i] = 0;
#endif

  /* there are no yamyams in supaplex levels */
  for (i = 0; i < level->num_yamyam_contents; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	level->yamyam_content[i].e[x][y] = EL_EMPTY;
}

static void LoadLevelFromFileInfo_SP(struct LevelInfo *level,
				     struct LevelFileInfo *level_file_info)
{
  char *filename = level_file_info->filename;
  FILE *file;
  int nr = level_file_info->nr - leveldir_current->first_level;
  int i, l, x, y;
  char name_first, name_last;
  struct LevelInfo multipart_level;
  int multipart_xpos, multipart_ypos;
  boolean is_multipart_level;
  boolean is_first_part;
  boolean reading_multipart_level = FALSE;
  boolean use_empty_level = FALSE;

  if (!(file = fopen(filename, MODE_READ)))
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);

    return;
  }

  /* position file stream to the requested level inside the level package */
  if (level_file_info->packed &&
      fseek(file, nr * SP_LEVEL_SIZE, SEEK_SET) != 0)
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot fseek in file '%s' -- using empty level", filename);

    return;
  }

  /* there exist Supaplex level package files with multi-part levels which
     can be detected as follows: instead of leading and trailing dashes ('-')
     to pad the level name, they have leading and trailing numbers which are
     the x and y coordinations of the current part of the multi-part level;
     if there are '?' characters instead of numbers on the left or right side
     of the level name, the multi-part level consists of only horizontal or
     vertical parts */

  for (l = nr; l < NUM_SUPAPLEX_LEVELS_PER_PACKAGE; l++)
  {
    LoadLevelFromFileStream_SP(file, level, l);

    /* check if this level is a part of a bigger multi-part level */

    name_first = level->name[0];
    name_last  = level->name[SP_LEVEL_NAME_LEN - 1];

    is_multipart_level =
      ((name_first == '?' || (name_first >= '0' && name_first <= '9')) &&
       (name_last  == '?' || (name_last  >= '0' && name_last  <= '9')));

    is_first_part =
      ((name_first == '?' || name_first == '1') &&
       (name_last  == '?' || name_last  == '1'));

    /* correct leading multipart level meta information in level name */
    for (i = 0; i < SP_LEVEL_NAME_LEN && level->name[i] == name_first; i++)
      level->name[i] = '-';

    /* correct trailing multipart level meta information in level name */
    for (i = SP_LEVEL_NAME_LEN - 1; i >= 0 && level->name[i] == name_last; i--)
      level->name[i] = '-';

    /* ---------- check for normal single level ---------- */

    if (!reading_multipart_level && !is_multipart_level)
    {
      /* the current level is simply a normal single-part level, and we are
	 not reading a multi-part level yet, so return the level as it is */

      break;
    }

    /* ---------- check for empty level (unused multi-part) ---------- */

    if (!reading_multipart_level && is_multipart_level && !is_first_part)
    {
      /* this is a part of a multi-part level, but not the first part
	 (and we are not already reading parts of a multi-part level);
	 in this case, use an empty level instead of the single part */

      use_empty_level = TRUE;

      break;
    }

    /* ---------- check for finished multi-part level ---------- */

    if (reading_multipart_level &&
	(!is_multipart_level ||
	 !strEqual(level->name, multipart_level.name)))
    {
      /* we are already reading parts of a multi-part level, but this level is
	 either not a multi-part level, or a part of a different multi-part
	 level; in both cases, the multi-part level seems to be complete */

      break;
    }

    /* ---------- here we have one part of a multi-part level ---------- */

    reading_multipart_level = TRUE;

    if (is_first_part)	/* start with first part of new multi-part level */
    {
      /* copy level info structure from first part */
      multipart_level = *level;

      /* clear playfield of new multi-part level */
      for (y = 0; y < MAX_LEV_FIELDY; y++)
	for (x = 0; x < MAX_LEV_FIELDX; x++)
	  multipart_level.field[x][y] = EL_EMPTY;
    }

    if (name_first == '?')
      name_first = '1';
    if (name_last == '?')
      name_last = '1';

    multipart_xpos = (int)(name_first - '0');
    multipart_ypos = (int)(name_last  - '0');

#if 0
    printf("----------> part (%d/%d) of multi-part level '%s'\n",
	   multipart_xpos, multipart_ypos, multipart_level.name);
#endif

    if (multipart_xpos * SP_LEVEL_XSIZE > MAX_LEV_FIELDX ||
	multipart_ypos * SP_LEVEL_YSIZE > MAX_LEV_FIELDY)
    {
      Error(ERR_WARN, "multi-part level is too big -- ignoring part of it");

      break;
    }

    multipart_level.fieldx = MAX(multipart_level.fieldx,
				 multipart_xpos * SP_LEVEL_XSIZE);
    multipart_level.fieldy = MAX(multipart_level.fieldy,
				 multipart_ypos * SP_LEVEL_YSIZE);

    /* copy level part at the right position of multi-part level */
    for (y = 0; y < SP_LEVEL_YSIZE; y++)
    {
      for (x = 0; x < SP_LEVEL_XSIZE; x++)
      {
	int start_x = (multipart_xpos - 1) * SP_LEVEL_XSIZE;
	int start_y = (multipart_ypos - 1) * SP_LEVEL_YSIZE;

	multipart_level.field[start_x + x][start_y + y] = level->field[x][y];
      }
    }
  }

  fclose(file);

  if (use_empty_level)
  {
    setLevelInfoToDefaults(level);

    level->fieldx = SP_LEVEL_XSIZE;
    level->fieldy = SP_LEVEL_YSIZE;

    for (y = 0; y < SP_LEVEL_YSIZE; y++)
      for (x = 0; x < SP_LEVEL_XSIZE; x++)
	level->field[x][y] = EL_EMPTY;

    strcpy(level->name, "-------- EMPTY --------");

    Error(ERR_WARN, "single part of multi-part level -- using empty level");
  }

  if (reading_multipart_level)
    *level = multipart_level;
}

#endif

void CopyNativeLevel_RND_to_SP(struct LevelInfo *level)
{
  struct LevelInfo_SP *level_sp = level->native_sp_level;
  LevelInfoType *header = &level_sp->header;
  int i, x, y;

  level_sp->width  = level->fieldx;
  level_sp->height = level->fieldy;

  for (x = 0; x < level->fieldx; x++)
    for (y = 0; y < level->fieldy; y++)
      level_sp->playfield[x][y] = map_element_RND_to_SP(level->field[x][y]);

  header->InitialGravity = (level->initial_player_gravity[0] ? 1 : 0);

  for (i = 0; i < SP_LEVEL_NAME_LEN; i++)
    header->LevelTitle[i] = level->name[i];
  /* !!! NO STRING TERMINATION IN SUPAPLEX VB CODE YET -- FIX THIS !!! */

  header->InfotronsNeeded = level->gems_needed;

  header->SpecialPortCount = 0;

  for (x = 0; x < level->fieldx; x++) for (y = 0; y < level->fieldy; y++)
  {
    boolean gravity_port_found = FALSE;
    boolean gravity_port_valid = FALSE;
    int gravity_port_flag;
    int gravity_port_base_element;
    int element = level->field[x][y];

    if (element >= EL_SP_GRAVITY_ON_PORT_RIGHT &&
	element <= EL_SP_GRAVITY_ON_PORT_UP)
    {
      gravity_port_found = TRUE;
      gravity_port_valid = TRUE;
      gravity_port_flag = 1;
      gravity_port_base_element = EL_SP_GRAVITY_ON_PORT_RIGHT;
    }
    else if (element >= EL_SP_GRAVITY_OFF_PORT_RIGHT &&
	     element <= EL_SP_GRAVITY_OFF_PORT_UP)
    {
      gravity_port_found = TRUE;
      gravity_port_valid = TRUE;
      gravity_port_flag = 0;
      gravity_port_base_element = EL_SP_GRAVITY_OFF_PORT_RIGHT;
    }
    else if (element >= EL_SP_GRAVITY_PORT_RIGHT &&
	     element <= EL_SP_GRAVITY_PORT_UP)
    {
      /* change R'n'D style gravity inverting special port to normal port
	 (there are no gravity inverting ports in native Supaplex engine) */

      gravity_port_found = TRUE;
      gravity_port_valid = FALSE;
      gravity_port_base_element = EL_SP_GRAVITY_PORT_RIGHT;
    }

    if (gravity_port_found)
    {
      if (gravity_port_valid &&
	  header->SpecialPortCount < SP_MAX_SPECIAL_PORTS)
      {
	SpecialPortType *port = &header->SpecialPort[header->SpecialPortCount];

	port->PortLocation = (y * level->fieldx + x) * 2;
	port->Gravity = gravity_port_flag;

	element += EL_SP_GRAVITY_PORT_RIGHT - gravity_port_base_element;

	header->SpecialPortCount++;
      }
      else
      {
	/* change special gravity port to normal port */

	element += EL_SP_PORT_RIGHT - gravity_port_base_element;
      }

      level_sp->playfield[x][y] = element - EL_SP_START;
    }
  }
}

void CopyNativeLevel_SP_to_RND(struct LevelInfo *level)
{
  struct LevelInfo_SP *level_sp = level->native_sp_level;
  LevelInfoType *header = &level_sp->header;
  int i, x, y;

  level->fieldx = level_sp->width;
  level->fieldy = level_sp->height;

  for (x = 0; x < level->fieldx; x++)
  {
    for (y = 0; y < level->fieldy; y++)
    {
      int element_old = level_sp->playfield[x][y];
      int element_new = getMappedElement(map_element_SP_to_RND(element_old));

      if (element_new == EL_UNKNOWN)
	Error(ERR_WARN, "invalid element %d at position %d, %d",
	      element_old, x, y);

      level->field[x][y] = element_new;
    }
  }

  for (i = 0; i < MAX_PLAYERS; i++)
    level->initial_player_gravity[i] =
      (header->InitialGravity == 1 ? TRUE : FALSE);

  for (i = 0; i < SP_LEVEL_NAME_LEN; i++)
    level->name[i] = header->LevelTitle[i];
  level->name[SP_LEVEL_NAME_LEN] = '\0';

  level->gems_needed = header->InfotronsNeeded;

  for (i = 0; i < header->SpecialPortCount; i++)
  {
    SpecialPortType *port = &header->SpecialPort[i];
    int port_location = port->PortLocation;
    int gravity = port->Gravity;
    int port_x, port_y, port_element;

    port_x = (port_location / 2) % level->fieldx;
    port_y = (port_location / 2) / level->fieldx;

    if (port_x < 0 || port_x >= level->fieldx ||
	port_y < 0 || port_y >= level->fieldy)
    {
      Error(ERR_WARN, "special port position (%d, %d) out of bounds",
	    port_x, port_y);

      continue;
    }

    port_element = level->field[port_x][port_y];

    if (port_element < EL_SP_GRAVITY_PORT_RIGHT ||
	port_element > EL_SP_GRAVITY_PORT_UP)
    {
      Error(ERR_WARN, "no special port at position (%d, %d)", port_x, port_y);

      continue;
    }

    /* change previous (wrong) gravity inverting special port to either
       gravity enabling special port or gravity disabling special port */
    level->field[port_x][port_y] +=
      (gravity == 1 ? EL_SP_GRAVITY_ON_PORT_RIGHT :
       EL_SP_GRAVITY_OFF_PORT_RIGHT) - EL_SP_GRAVITY_PORT_RIGHT;
  }

  /* change special gravity ports without database entries to normal ports */
  for (x = 0; x < level->fieldx; x++)
    for (y = 0; y < level->fieldy; y++)
      if (level->field[x][y] >= EL_SP_GRAVITY_PORT_RIGHT &&
	  level->field[x][y] <= EL_SP_GRAVITY_PORT_UP)
	level->field[x][y] += EL_SP_PORT_RIGHT - EL_SP_GRAVITY_PORT_RIGHT;

  level->time = 0;			/* no time limit */
  level->amoeba_speed = 0;
  level->time_magic_wall = 0;
  level->time_wheel = 0;
  level->amoeba_content = EL_EMPTY;

#if 1
  /* original Supaplex does not use score values -- use default values */
#else
  for (i = 0; i < LEVEL_SCORE_ELEMENTS; i++)
    level->score[i] = 0;
#endif

  /* there are no yamyams in supaplex levels */
  for (i = 0; i < level->num_yamyam_contents; i++)
    for (x = 0; x < 3; x++)
      for (y = 0; y < 3; y++)
	level->yamyam_content[i].e[x][y] = EL_EMPTY;
}

static void CopyNativeTape_RND_to_SP(struct LevelInfo *level)
{
  struct LevelInfo_SP *level_sp = level->native_sp_level;
  struct DemoInfo_SP *demo = &level_sp->demo;
  int i, j;

  /* always start with reliable default values */
  demo->is_available = FALSE;
  demo->length = 0;

  if (TAPE_IS_EMPTY(tape))
    return;

  demo->level_nr = tape.level_nr;	/* (currently not used) */

  level_sp->header.DemoRandomSeed = tape.random_seed;

  demo->length = 0;
  for (i = 0; i < tape.length; i++)
  {
    int demo_action = map_key_RND_to_SP(tape.pos[i].action[0]);
    int demo_repeat = tape.pos[i].delay;

    for (j = 0; j < demo_repeat / 16; j++)
      demo->data[demo->length++] = 0xf0 | demo_action;

    if (demo_repeat % 16)
      demo->data[demo->length++] = ((demo_repeat % 16 - 1) << 4) | demo_action;
  }

  demo->data[demo->length++] = 0xff;

  demo->is_available = TRUE;
}

static void setTapeInfoToDefaults();

static void CopyNativeTape_SP_to_RND(struct LevelInfo *level)
{
  struct LevelInfo_SP *level_sp = level->native_sp_level;
  struct DemoInfo_SP *demo = &level_sp->demo;
  char *filename = level->file_info.filename;
  int i;

  /* always start with reliable default values */
  setTapeInfoToDefaults();

  if (!demo->is_available)
    return;

  tape.level_nr = demo->level_nr;	/* (currently not used) */
  tape.length = demo->length - 1;	/* without "end of demo" byte */
  tape.random_seed = level_sp->header.DemoRandomSeed;

  TapeSetDateFromEpochSeconds(getFileTimestampEpochSeconds(filename));

  for (i = 0; i < demo->length - 1; i++)
  {
    int demo_action = demo->data[i] & 0x0f;
    int demo_repeat = (demo->data[i] & 0xf0) >> 4;

    tape.pos[i].action[0] = map_key_SP_to_RND(demo_action);
    tape.pos[i].delay = demo_repeat + 1;
  }

  tape.length_seconds = GetTapeLength();
}


/* ------------------------------------------------------------------------- */
/* functions for loading DC level                                            */
/* ------------------------------------------------------------------------- */

#define DC_LEVEL_HEADER_SIZE		344

unsigned short getDecodedWord_DC(unsigned short data_encoded, boolean init)
{
  static int last_data_encoded;
  static int offset1;
  static int offset2;
  int diff;
  int diff_hi, diff_lo;
  int data_hi, data_lo;
  unsigned short data_decoded;

  if (init)
  {
    last_data_encoded = 0;
    offset1 = -1;
    offset2 = 0;

    return 0;
  }

  diff = data_encoded - last_data_encoded;
  diff_hi = diff & ~0xff;
  diff_lo = diff &  0xff;

  offset2 += diff_lo;

  data_hi = diff_hi - (offset1 << 8) + (offset2 & 0xff00);
  data_lo = (diff_lo + (data_hi >> 16)) & 0x00ff;
  data_hi = data_hi & 0xff00;

  data_decoded = data_hi | data_lo;

  last_data_encoded = data_encoded;

  offset1 = (offset1 + 1) % 31;
  offset2 = offset2 & 0xff;

  return data_decoded;
}

int getMappedElement_DC(int element)
{
  switch (element)
  {
    case 0x0000:
      element = EL_ROCK;
      break;

      /* 0x0117 - 0x036e: (?) */
      /* EL_DIAMOND */

      /* 0x042d - 0x0684: (?) */
      /* EL_EMERALD */

    case 0x06f1:
      element = EL_NUT;
      break;

    case 0x074c:
      element = EL_BOMB;
      break;

    case 0x07a4:
      element = EL_PEARL;
      break;

    case 0x0823:
      element = EL_CRYSTAL;
      break;

    case 0x0e77:	/* quicksand (boulder) */
      element = EL_QUICKSAND_FAST_FULL;
      break;

    case 0x0e99:	/* slow quicksand (boulder) */
      element = EL_QUICKSAND_FULL;
      break;

    case 0x0ed2:
      element = EL_EM_EXIT_OPEN;
      break;

    case 0x0ee3:
      element = EL_EM_EXIT_CLOSED;
      break;

    case 0x0eeb:
      element = EL_EM_STEEL_EXIT_OPEN;
      break;

    case 0x0efc:
      element = EL_EM_STEEL_EXIT_CLOSED;
      break;

    case 0x0f4f:	/* dynamite (lit 1) */
      element = EL_EM_DYNAMITE_ACTIVE;
      break;

    case 0x0f57:	/* dynamite (lit 2) */
      element = EL_EM_DYNAMITE_ACTIVE;
      break;

    case 0x0f5f:	/* dynamite (lit 3) */
      element = EL_EM_DYNAMITE_ACTIVE;
      break;

    case 0x0f67:	/* dynamite (lit 4) */
      element = EL_EM_DYNAMITE_ACTIVE;
      break;

    case 0x0f81:
    case 0x0f82:
    case 0x0f83:
    case 0x0f84:
      element = EL_AMOEBA_WET;
      break;

    case 0x0f85:
      element = EL_AMOEBA_DROP;
      break;

    case 0x0fb9:
      element = EL_DC_MAGIC_WALL;
      break;

    case 0x0fd0:
      element = EL_SPACESHIP_UP;
      break;

    case 0x0fd9:
      element = EL_SPACESHIP_DOWN;
      break;

    case 0x0ff1:
      element = EL_SPACESHIP_LEFT;
      break;

    case 0x0ff9:
      element = EL_SPACESHIP_RIGHT;
      break;

    case 0x1057:
      element = EL_BUG_UP;
      break;

    case 0x1060:
      element = EL_BUG_DOWN;
      break;

    case 0x1078:
      element = EL_BUG_LEFT;
      break;

    case 0x1080:
      element = EL_BUG_RIGHT;
      break;

    case 0x10de:
      element = EL_MOLE_UP;
      break;

    case 0x10e7:
      element = EL_MOLE_DOWN;
      break;

    case 0x10ff:
      element = EL_MOLE_LEFT;
      break;

    case 0x1107:
      element = EL_MOLE_RIGHT;
      break;

    case 0x11c0:
      element = EL_ROBOT;
      break;

    case 0x13f5:
      element = EL_YAMYAM;
      break;

    case 0x1425:
      element = EL_SWITCHGATE_OPEN;
      break;

    case 0x1426:
      element = EL_SWITCHGATE_CLOSED;
      break;

    case 0x1437:
      element = EL_DC_SWITCHGATE_SWITCH_UP;
      break;

    case 0x143a:
      element = EL_TIMEGATE_CLOSED;
      break;

    case 0x144c:	/* conveyor belt switch (green) */
      element = EL_CONVEYOR_BELT_3_SWITCH_MIDDLE;
      break;

    case 0x144f:	/* conveyor belt switch (red) */
      element = EL_CONVEYOR_BELT_1_SWITCH_MIDDLE;
      break;

    case 0x1452:	/* conveyor belt switch (blue) */
      element = EL_CONVEYOR_BELT_4_SWITCH_MIDDLE;
      break;

    case 0x145b:
      element = EL_CONVEYOR_BELT_3_MIDDLE;
      break;

    case 0x1463:
      element = EL_CONVEYOR_BELT_3_LEFT;
      break;

    case 0x146b:
      element = EL_CONVEYOR_BELT_3_RIGHT;
      break;

    case 0x1473:
      element = EL_CONVEYOR_BELT_1_MIDDLE;
      break;

    case 0x147b:
      element = EL_CONVEYOR_BELT_1_LEFT;
      break;

    case 0x1483:
      element = EL_CONVEYOR_BELT_1_RIGHT;
      break;

    case 0x148b:
      element = EL_CONVEYOR_BELT_4_MIDDLE;
      break;

    case 0x1493:
      element = EL_CONVEYOR_BELT_4_LEFT;
      break;

    case 0x149b:
      element = EL_CONVEYOR_BELT_4_RIGHT;
      break;

    case 0x14ac:
      element = EL_EXPANDABLE_WALL_HORIZONTAL;
      break;

    case 0x14bd:
      element = EL_EXPANDABLE_WALL_VERTICAL;
      break;

    case 0x14c6:
      element = EL_EXPANDABLE_WALL_ANY;
      break;

    case 0x14ce:	/* growing steel wall (left/right) */
      element = EL_EXPANDABLE_STEELWALL_HORIZONTAL;
      break;

    case 0x14df:	/* growing steel wall (up/down) */
      element = EL_EXPANDABLE_STEELWALL_VERTICAL;
      break;

    case 0x14e8:	/* growing steel wall (up/down/left/right) */
      element = EL_EXPANDABLE_STEELWALL_ANY;
      break;

    case 0x14e9:
      element = EL_SHIELD_DEADLY;
      break;

    case 0x1501:
      element = EL_EXTRA_TIME;
      break;

    case 0x154f:
      element = EL_ACID;
      break;

    case 0x1577:
      element = EL_EMPTY_SPACE;
      break;

    case 0x1578:	/* quicksand (empty) */
      element = EL_QUICKSAND_FAST_EMPTY;
      break;

    case 0x1579:	/* slow quicksand (empty) */
      element = EL_QUICKSAND_EMPTY;
      break;

      /* 0x157c - 0x158b: */
      /* EL_SAND */

      /* 0x1590 - 0x159f: */
      /* EL_DC_LANDMINE */

    case 0x15a0:
      element = EL_EM_DYNAMITE;
      break;

    case 0x15a1:	/* key (red) */
      element = EL_EM_KEY_1;
      break;

    case 0x15a2:	/* key (yellow) */
      element = EL_EM_KEY_2;
      break;

    case 0x15a3:	/* key (blue) */
      element = EL_EM_KEY_4;
      break;

    case 0x15a4:	/* key (green) */
      element = EL_EM_KEY_3;
      break;

    case 0x15a5:	/* key (white) */
      element = EL_DC_KEY_WHITE;
      break;

    case 0x15a6:
      element = EL_WALL_SLIPPERY;
      break;

    case 0x15a7:
      element = EL_WALL;
      break;

    case 0x15a8:	/* wall (not round) */
      element = EL_WALL;
      break;

    case 0x15a9:	/* (blue) */
      element = EL_CHAR_A;
      break;

    case 0x15aa:	/* (blue) */
      element = EL_CHAR_B;
      break;

    case 0x15ab:	/* (blue) */
      element = EL_CHAR_C;
      break;

    case 0x15ac:	/* (blue) */
      element = EL_CHAR_D;
      break;

    case 0x15ad:	/* (blue) */
      element = EL_CHAR_E;
      break;

    case 0x15ae:	/* (blue) */
      element = EL_CHAR_F;
      break;

    case 0x15af:	/* (blue) */
      element = EL_CHAR_G;
      break;

    case 0x15b0:	/* (blue) */
      element = EL_CHAR_H;
      break;

    case 0x15b1:	/* (blue) */
      element = EL_CHAR_I;
      break;

    case 0x15b2:	/* (blue) */
      element = EL_CHAR_J;
      break;

    case 0x15b3:	/* (blue) */
      element = EL_CHAR_K;
      break;

    case 0x15b4:	/* (blue) */
      element = EL_CHAR_L;
      break;

    case 0x15b5:	/* (blue) */
      element = EL_CHAR_M;
      break;

    case 0x15b6:	/* (blue) */
      element = EL_CHAR_N;
      break;

    case 0x15b7:	/* (blue) */
      element = EL_CHAR_O;
      break;

    case 0x15b8:	/* (blue) */
      element = EL_CHAR_P;
      break;

    case 0x15b9:	/* (blue) */
      element = EL_CHAR_Q;
      break;

    case 0x15ba:	/* (blue) */
      element = EL_CHAR_R;
      break;

    case 0x15bb:	/* (blue) */
      element = EL_CHAR_S;
      break;

    case 0x15bc:	/* (blue) */
      element = EL_CHAR_T;
      break;

    case 0x15bd:	/* (blue) */
      element = EL_CHAR_U;
      break;

    case 0x15be:	/* (blue) */
      element = EL_CHAR_V;
      break;

    case 0x15bf:	/* (blue) */
      element = EL_CHAR_W;
      break;

    case 0x15c0:	/* (blue) */
      element = EL_CHAR_X;
      break;

    case 0x15c1:	/* (blue) */
      element = EL_CHAR_Y;
      break;

    case 0x15c2:	/* (blue) */
      element = EL_CHAR_Z;
      break;

    case 0x15c3:	/* (blue) */
      element = EL_CHAR_AUMLAUT;
      break;

    case 0x15c4:	/* (blue) */
      element = EL_CHAR_OUMLAUT;
      break;

    case 0x15c5:	/* (blue) */
      element = EL_CHAR_UUMLAUT;
      break;

    case 0x15c6:	/* (blue) */
      element = EL_CHAR_0;
      break;

    case 0x15c7:	/* (blue) */
      element = EL_CHAR_1;
      break;

    case 0x15c8:	/* (blue) */
      element = EL_CHAR_2;
      break;

    case 0x15c9:	/* (blue) */
      element = EL_CHAR_3;
      break;

    case 0x15ca:	/* (blue) */
      element = EL_CHAR_4;
      break;

    case 0x15cb:	/* (blue) */
      element = EL_CHAR_5;
      break;

    case 0x15cc:	/* (blue) */
      element = EL_CHAR_6;
      break;

    case 0x15cd:	/* (blue) */
      element = EL_CHAR_7;
      break;

    case 0x15ce:	/* (blue) */
      element = EL_CHAR_8;
      break;

    case 0x15cf:	/* (blue) */
      element = EL_CHAR_9;
      break;

    case 0x15d0:	/* (blue) */
      element = EL_CHAR_PERIOD;
      break;

    case 0x15d1:	/* (blue) */
      element = EL_CHAR_EXCLAM;
      break;

    case 0x15d2:	/* (blue) */
      element = EL_CHAR_COLON;
      break;

    case 0x15d3:	/* (blue) */
      element = EL_CHAR_LESS;
      break;

    case 0x15d4:	/* (blue) */
      element = EL_CHAR_GREATER;
      break;

    case 0x15d5:	/* (blue) */
      element = EL_CHAR_QUESTION;
      break;

    case 0x15d6:	/* (blue) */
      element = EL_CHAR_COPYRIGHT;
      break;

    case 0x15d7:	/* (blue) */
      element = EL_CHAR_UP;
      break;

    case 0x15d8:	/* (blue) */
      element = EL_CHAR_DOWN;
      break;

    case 0x15d9:	/* (blue) */
      element = EL_CHAR_BUTTON;
      break;

    case 0x15da:	/* (blue) */
      element = EL_CHAR_PLUS;
      break;

    case 0x15db:	/* (blue) */
      element = EL_CHAR_MINUS;
      break;

    case 0x15dc:	/* (blue) */
      element = EL_CHAR_APOSTROPHE;
      break;

    case 0x15dd:	/* (blue) */
      element = EL_CHAR_PARENLEFT;
      break;

    case 0x15de:	/* (blue) */
      element = EL_CHAR_PARENRIGHT;
      break;

    case 0x15df:	/* (green) */
      element = EL_CHAR_A;
      break;

    case 0x15e0:	/* (green) */
      element = EL_CHAR_B;
      break;

    case 0x15e1:	/* (green) */
      element = EL_CHAR_C;
      break;

    case 0x15e2:	/* (green) */
      element = EL_CHAR_D;
      break;

    case 0x15e3:	/* (green) */
      element = EL_CHAR_E;
      break;

    case 0x15e4:	/* (green) */
      element = EL_CHAR_F;
      break;

    case 0x15e5:	/* (green) */
      element = EL_CHAR_G;
      break;

    case 0x15e6:	/* (green) */
      element = EL_CHAR_H;
      break;

    case 0x15e7:	/* (green) */
      element = EL_CHAR_I;
      break;

    case 0x15e8:	/* (green) */
      element = EL_CHAR_J;
      break;

    case 0x15e9:	/* (green) */
      element = EL_CHAR_K;
      break;

    case 0x15ea:	/* (green) */
      element = EL_CHAR_L;
      break;

    case 0x15eb:	/* (green) */
      element = EL_CHAR_M;
      break;

    case 0x15ec:	/* (green) */
      element = EL_CHAR_N;
      break;

    case 0x15ed:	/* (green) */
      element = EL_CHAR_O;
      break;

    case 0x15ee:	/* (green) */
      element = EL_CHAR_P;
      break;

    case 0x15ef:	/* (green) */
      element = EL_CHAR_Q;
      break;

    case 0x15f0:	/* (green) */
      element = EL_CHAR_R;
      break;

    case 0x15f1:	/* (green) */
      element = EL_CHAR_S;
      break;

    case 0x15f2:	/* (green) */
      element = EL_CHAR_T;
      break;

    case 0x15f3:	/* (green) */
      element = EL_CHAR_U;
      break;

    case 0x15f4:	/* (green) */
      element = EL_CHAR_V;
      break;

    case 0x15f5:	/* (green) */
      element = EL_CHAR_W;
      break;

    case 0x15f6:	/* (green) */
      element = EL_CHAR_X;
      break;

    case 0x15f7:	/* (green) */
      element = EL_CHAR_Y;
      break;

    case 0x15f8:	/* (green) */
      element = EL_CHAR_Z;
      break;

    case 0x15f9:	/* (green) */
      element = EL_CHAR_AUMLAUT;
      break;

    case 0x15fa:	/* (green) */
      element = EL_CHAR_OUMLAUT;
      break;

    case 0x15fb:	/* (green) */
      element = EL_CHAR_UUMLAUT;
      break;

    case 0x15fc:	/* (green) */
      element = EL_CHAR_0;
      break;

    case 0x15fd:	/* (green) */
      element = EL_CHAR_1;
      break;

    case 0x15fe:	/* (green) */
      element = EL_CHAR_2;
      break;

    case 0x15ff:	/* (green) */
      element = EL_CHAR_3;
      break;

    case 0x1600:	/* (green) */
      element = EL_CHAR_4;
      break;

    case 0x1601:	/* (green) */
      element = EL_CHAR_5;
      break;

    case 0x1602:	/* (green) */
      element = EL_CHAR_6;
      break;

    case 0x1603:	/* (green) */
      element = EL_CHAR_7;
      break;

    case 0x1604:	/* (green) */
      element = EL_CHAR_8;
      break;

    case 0x1605:	/* (green) */
      element = EL_CHAR_9;
      break;

    case 0x1606:	/* (green) */
      element = EL_CHAR_PERIOD;
      break;

    case 0x1607:	/* (green) */
      element = EL_CHAR_EXCLAM;
      break;

    case 0x1608:	/* (green) */
      element = EL_CHAR_COLON;
      break;

    case 0x1609:	/* (green) */
      element = EL_CHAR_LESS;
      break;

    case 0x160a:	/* (green) */
      element = EL_CHAR_GREATER;
      break;

    case 0x160b:	/* (green) */
      element = EL_CHAR_QUESTION;
      break;

    case 0x160c:	/* (green) */
      element = EL_CHAR_COPYRIGHT;
      break;

    case 0x160d:	/* (green) */
      element = EL_CHAR_UP;
      break;

    case 0x160e:	/* (green) */
      element = EL_CHAR_DOWN;
      break;

    case 0x160f:	/* (green) */
      element = EL_CHAR_BUTTON;
      break;

    case 0x1610:	/* (green) */
      element = EL_CHAR_PLUS;
      break;

    case 0x1611:	/* (green) */
      element = EL_CHAR_MINUS;
      break;

    case 0x1612:	/* (green) */
      element = EL_CHAR_APOSTROPHE;
      break;

    case 0x1613:	/* (green) */
      element = EL_CHAR_PARENLEFT;
      break;

    case 0x1614:	/* (green) */
      element = EL_CHAR_PARENRIGHT;
      break;

    case 0x1615:	/* (blue steel) */
      element = EL_STEEL_CHAR_A;
      break;

    case 0x1616:	/* (blue steel) */
      element = EL_STEEL_CHAR_B;
      break;

    case 0x1617:	/* (blue steel) */
      element = EL_STEEL_CHAR_C;
      break;

    case 0x1618:	/* (blue steel) */
      element = EL_STEEL_CHAR_D;
      break;

    case 0x1619:	/* (blue steel) */
      element = EL_STEEL_CHAR_E;
      break;

    case 0x161a:	/* (blue steel) */
      element = EL_STEEL_CHAR_F;
      break;

    case 0x161b:	/* (blue steel) */
      element = EL_STEEL_CHAR_G;
      break;

    case 0x161c:	/* (blue steel) */
      element = EL_STEEL_CHAR_H;
      break;

    case 0x161d:	/* (blue steel) */
      element = EL_STEEL_CHAR_I;
      break;

    case 0x161e:	/* (blue steel) */
      element = EL_STEEL_CHAR_J;
      break;

    case 0x161f:	/* (blue steel) */
      element = EL_STEEL_CHAR_K;
      break;

    case 0x1620:	/* (blue steel) */
      element = EL_STEEL_CHAR_L;
      break;

    case 0x1621:	/* (blue steel) */
      element = EL_STEEL_CHAR_M;
      break;

    case 0x1622:	/* (blue steel) */
      element = EL_STEEL_CHAR_N;
      break;

    case 0x1623:	/* (blue steel) */
      element = EL_STEEL_CHAR_O;
      break;

    case 0x1624:	/* (blue steel) */
      element = EL_STEEL_CHAR_P;
      break;

    case 0x1625:	/* (blue steel) */
      element = EL_STEEL_CHAR_Q;
      break;

    case 0x1626:	/* (blue steel) */
      element = EL_STEEL_CHAR_R;
      break;

    case 0x1627:	/* (blue steel) */
      element = EL_STEEL_CHAR_S;
      break;

    case 0x1628:	/* (blue steel) */
      element = EL_STEEL_CHAR_T;
      break;

    case 0x1629:	/* (blue steel) */
      element = EL_STEEL_CHAR_U;
      break;

    case 0x162a:	/* (blue steel) */
      element = EL_STEEL_CHAR_V;
      break;

    case 0x162b:	/* (blue steel) */
      element = EL_STEEL_CHAR_W;
      break;

    case 0x162c:	/* (blue steel) */
      element = EL_STEEL_CHAR_X;
      break;

    case 0x162d:	/* (blue steel) */
      element = EL_STEEL_CHAR_Y;
      break;

    case 0x162e:	/* (blue steel) */
      element = EL_STEEL_CHAR_Z;
      break;

    case 0x162f:	/* (blue steel) */
      element = EL_STEEL_CHAR_AUMLAUT;
      break;

    case 0x1630:	/* (blue steel) */
      element = EL_STEEL_CHAR_OUMLAUT;
      break;

    case 0x1631:	/* (blue steel) */
      element = EL_STEEL_CHAR_UUMLAUT;
      break;

    case 0x1632:	/* (blue steel) */
      element = EL_STEEL_CHAR_0;
      break;

    case 0x1633:	/* (blue steel) */
      element = EL_STEEL_CHAR_1;
      break;

    case 0x1634:	/* (blue steel) */
      element = EL_STEEL_CHAR_2;
      break;

    case 0x1635:	/* (blue steel) */
      element = EL_STEEL_CHAR_3;
      break;

    case 0x1636:	/* (blue steel) */
      element = EL_STEEL_CHAR_4;
      break;

    case 0x1637:	/* (blue steel) */
      element = EL_STEEL_CHAR_5;
      break;

    case 0x1638:	/* (blue steel) */
      element = EL_STEEL_CHAR_6;
      break;

    case 0x1639:	/* (blue steel) */
      element = EL_STEEL_CHAR_7;
      break;

    case 0x163a:	/* (blue steel) */
      element = EL_STEEL_CHAR_8;
      break;

    case 0x163b:	/* (blue steel) */
      element = EL_STEEL_CHAR_9;
      break;

    case 0x163c:	/* (blue steel) */
      element = EL_STEEL_CHAR_PERIOD;
      break;

    case 0x163d:	/* (blue steel) */
      element = EL_STEEL_CHAR_EXCLAM;
      break;

    case 0x163e:	/* (blue steel) */
      element = EL_STEEL_CHAR_COLON;
      break;

    case 0x163f:	/* (blue steel) */
      element = EL_STEEL_CHAR_LESS;
      break;

    case 0x1640:	/* (blue steel) */
      element = EL_STEEL_CHAR_GREATER;
      break;

    case 0x1641:	/* (blue steel) */
      element = EL_STEEL_CHAR_QUESTION;
      break;

    case 0x1642:	/* (blue steel) */
      element = EL_STEEL_CHAR_COPYRIGHT;
      break;

    case 0x1643:	/* (blue steel) */
      element = EL_STEEL_CHAR_UP;
      break;

    case 0x1644:	/* (blue steel) */
      element = EL_STEEL_CHAR_DOWN;
      break;

    case 0x1645:	/* (blue steel) */
      element = EL_STEEL_CHAR_BUTTON;
      break;

    case 0x1646:	/* (blue steel) */
      element = EL_STEEL_CHAR_PLUS;
      break;

    case 0x1647:	/* (blue steel) */
      element = EL_STEEL_CHAR_MINUS;
      break;

    case 0x1648:	/* (blue steel) */
      element = EL_STEEL_CHAR_APOSTROPHE;
      break;

    case 0x1649:	/* (blue steel) */
      element = EL_STEEL_CHAR_PARENLEFT;
      break;

    case 0x164a:	/* (blue steel) */
      element = EL_STEEL_CHAR_PARENRIGHT;
      break;

    case 0x164b:	/* (green steel) */
      element = EL_STEEL_CHAR_A;
      break;

    case 0x164c:	/* (green steel) */
      element = EL_STEEL_CHAR_B;
      break;

    case 0x164d:	/* (green steel) */
      element = EL_STEEL_CHAR_C;
      break;

    case 0x164e:	/* (green steel) */
      element = EL_STEEL_CHAR_D;
      break;

    case 0x164f:	/* (green steel) */
      element = EL_STEEL_CHAR_E;
      break;

    case 0x1650:	/* (green steel) */
      element = EL_STEEL_CHAR_F;
      break;

    case 0x1651:	/* (green steel) */
      element = EL_STEEL_CHAR_G;
      break;

    case 0x1652:	/* (green steel) */
      element = EL_STEEL_CHAR_H;
      break;

    case 0x1653:	/* (green steel) */
      element = EL_STEEL_CHAR_I;
      break;

    case 0x1654:	/* (green steel) */
      element = EL_STEEL_CHAR_J;
      break;

    case 0x1655:	/* (green steel) */
      element = EL_STEEL_CHAR_K;
      break;

    case 0x1656:	/* (green steel) */
      element = EL_STEEL_CHAR_L;
      break;

    case 0x1657:	/* (green steel) */
      element = EL_STEEL_CHAR_M;
      break;

    case 0x1658:	/* (green steel) */
      element = EL_STEEL_CHAR_N;
      break;

    case 0x1659:	/* (green steel) */
      element = EL_STEEL_CHAR_O;
      break;

    case 0x165a:	/* (green steel) */
      element = EL_STEEL_CHAR_P;
      break;

    case 0x165b:	/* (green steel) */
      element = EL_STEEL_CHAR_Q;
      break;

    case 0x165c:	/* (green steel) */
      element = EL_STEEL_CHAR_R;
      break;

    case 0x165d:	/* (green steel) */
      element = EL_STEEL_CHAR_S;
      break;

    case 0x165e:	/* (green steel) */
      element = EL_STEEL_CHAR_T;
      break;

    case 0x165f:	/* (green steel) */
      element = EL_STEEL_CHAR_U;
      break;

    case 0x1660:	/* (green steel) */
      element = EL_STEEL_CHAR_V;
      break;

    case 0x1661:	/* (green steel) */
      element = EL_STEEL_CHAR_W;
      break;

    case 0x1662:	/* (green steel) */
      element = EL_STEEL_CHAR_X;
      break;

    case 0x1663:	/* (green steel) */
      element = EL_STEEL_CHAR_Y;
      break;

    case 0x1664:	/* (green steel) */
      element = EL_STEEL_CHAR_Z;
      break;

    case 0x1665:	/* (green steel) */
      element = EL_STEEL_CHAR_AUMLAUT;
      break;

    case 0x1666:	/* (green steel) */
      element = EL_STEEL_CHAR_OUMLAUT;
      break;

    case 0x1667:	/* (green steel) */
      element = EL_STEEL_CHAR_UUMLAUT;
      break;

    case 0x1668:	/* (green steel) */
      element = EL_STEEL_CHAR_0;
      break;

    case 0x1669:	/* (green steel) */
      element = EL_STEEL_CHAR_1;
      break;

    case 0x166a:	/* (green steel) */
      element = EL_STEEL_CHAR_2;
      break;

    case 0x166b:	/* (green steel) */
      element = EL_STEEL_CHAR_3;
      break;

    case 0x166c:	/* (green steel) */
      element = EL_STEEL_CHAR_4;
      break;

    case 0x166d:	/* (green steel) */
      element = EL_STEEL_CHAR_5;
      break;

    case 0x166e:	/* (green steel) */
      element = EL_STEEL_CHAR_6;
      break;

    case 0x166f:	/* (green steel) */
      element = EL_STEEL_CHAR_7;
      break;

    case 0x1670:	/* (green steel) */
      element = EL_STEEL_CHAR_8;
      break;

    case 0x1671:	/* (green steel) */
      element = EL_STEEL_CHAR_9;
      break;

    case 0x1672:	/* (green steel) */
      element = EL_STEEL_CHAR_PERIOD;
      break;

    case 0x1673:	/* (green steel) */
      element = EL_STEEL_CHAR_EXCLAM;
      break;

    case 0x1674:	/* (green steel) */
      element = EL_STEEL_CHAR_COLON;
      break;

    case 0x1675:	/* (green steel) */
      element = EL_STEEL_CHAR_LESS;
      break;

    case 0x1676:	/* (green steel) */
      element = EL_STEEL_CHAR_GREATER;
      break;

    case 0x1677:	/* (green steel) */
      element = EL_STEEL_CHAR_QUESTION;
      break;

    case 0x1678:	/* (green steel) */
      element = EL_STEEL_CHAR_COPYRIGHT;
      break;

    case 0x1679:	/* (green steel) */
      element = EL_STEEL_CHAR_UP;
      break;

    case 0x167a:	/* (green steel) */
      element = EL_STEEL_CHAR_DOWN;
      break;

    case 0x167b:	/* (green steel) */
      element = EL_STEEL_CHAR_BUTTON;
      break;

    case 0x167c:	/* (green steel) */
      element = EL_STEEL_CHAR_PLUS;
      break;

    case 0x167d:	/* (green steel) */
      element = EL_STEEL_CHAR_MINUS;
      break;

    case 0x167e:	/* (green steel) */
      element = EL_STEEL_CHAR_APOSTROPHE;
      break;

    case 0x167f:	/* (green steel) */
      element = EL_STEEL_CHAR_PARENLEFT;
      break;

    case 0x1680:	/* (green steel) */
      element = EL_STEEL_CHAR_PARENRIGHT;
      break;

    case 0x1681:	/* gate (red) */
      element = EL_EM_GATE_1;
      break;

    case 0x1682:	/* secret gate (red) */
      element = EL_GATE_1_GRAY;
      break;

    case 0x1683:	/* gate (yellow) */
      element = EL_EM_GATE_2;
      break;

    case 0x1684:	/* secret gate (yellow) */
      element = EL_GATE_2_GRAY;
      break;

    case 0x1685:	/* gate (blue) */
      element = EL_EM_GATE_4;
      break;

    case 0x1686:	/* secret gate (blue) */
      element = EL_GATE_4_GRAY;
      break;

    case 0x1687:	/* gate (green) */
      element = EL_EM_GATE_3;
      break;

    case 0x1688:	/* secret gate (green) */
      element = EL_GATE_3_GRAY;
      break;

    case 0x1689:	/* gate (white) */
      element = EL_DC_GATE_WHITE;
      break;

    case 0x168a:	/* secret gate (white) */
      element = EL_DC_GATE_WHITE_GRAY;
      break;

    case 0x168b:	/* secret gate (no key) */
      element = EL_DC_GATE_FAKE_GRAY;
      break;

    case 0x168c:
      element = EL_ROBOT_WHEEL;
      break;

    case 0x168d:
      element = EL_DC_TIMEGATE_SWITCH;
      break;

    case 0x168e:
      element = EL_ACID_POOL_BOTTOM;
      break;

    case 0x168f:
      element = EL_ACID_POOL_TOPLEFT;
      break;

    case 0x1690:
      element = EL_ACID_POOL_TOPRIGHT;
      break;

    case 0x1691:
      element = EL_ACID_POOL_BOTTOMLEFT;
      break;

    case 0x1692:
      element = EL_ACID_POOL_BOTTOMRIGHT;
      break;

    case 0x1693:
      element = EL_STEELWALL;
      break;

    case 0x1694:
      element = EL_STEELWALL_SLIPPERY;
      break;

    case 0x1695:	/* steel wall (not round) */
      element = EL_STEELWALL;
      break;

    case 0x1696:	/* steel wall (left) */
      element = EL_DC_STEELWALL_1_LEFT;
      break;

    case 0x1697:	/* steel wall (bottom) */
      element = EL_DC_STEELWALL_1_BOTTOM;
      break;

    case 0x1698:	/* steel wall (right) */
      element = EL_DC_STEELWALL_1_RIGHT;
      break;

    case 0x1699:	/* steel wall (top) */
      element = EL_DC_STEELWALL_1_TOP;
      break;

    case 0x169a:	/* steel wall (left/bottom) */
      element = EL_DC_STEELWALL_1_BOTTOMLEFT;
      break;

    case 0x169b:	/* steel wall (right/bottom) */
      element = EL_DC_STEELWALL_1_BOTTOMRIGHT;
      break;

    case 0x169c:	/* steel wall (right/top) */
      element = EL_DC_STEELWALL_1_TOPRIGHT;
      break;

    case 0x169d:	/* steel wall (left/top) */
      element = EL_DC_STEELWALL_1_TOPLEFT;
      break;

    case 0x169e:	/* steel wall (right/bottom small) */
      element = EL_DC_STEELWALL_1_BOTTOMRIGHT_2;
      break;

    case 0x169f:	/* steel wall (left/bottom small) */
      element = EL_DC_STEELWALL_1_BOTTOMLEFT_2;
      break;

    case 0x16a0:	/* steel wall (right/top small) */
      element = EL_DC_STEELWALL_1_TOPRIGHT_2;
      break;

    case 0x16a1:	/* steel wall (left/top small) */
      element = EL_DC_STEELWALL_1_TOPLEFT_2;
      break;

    case 0x16a2:	/* steel wall (left/right) */
      element = EL_DC_STEELWALL_1_VERTICAL;
      break;

    case 0x16a3:	/* steel wall (top/bottom) */
      element = EL_DC_STEELWALL_1_HORIZONTAL;
      break;

    case 0x16a4:	/* steel wall 2 (left end) */
      element = EL_DC_STEELWALL_2_LEFT;
      break;

    case 0x16a5:	/* steel wall 2 (right end) */
      element = EL_DC_STEELWALL_2_RIGHT;
      break;

    case 0x16a6:	/* steel wall 2 (top end) */
      element = EL_DC_STEELWALL_2_TOP;
      break;

    case 0x16a7:	/* steel wall 2 (bottom end) */
      element = EL_DC_STEELWALL_2_BOTTOM;
      break;

    case 0x16a8:	/* steel wall 2 (left/right) */
      element = EL_DC_STEELWALL_2_HORIZONTAL;
      break;

    case 0x16a9:	/* steel wall 2 (up/down) */
      element = EL_DC_STEELWALL_2_VERTICAL;
      break;

    case 0x16aa:	/* steel wall 2 (mid) */
      element = EL_DC_STEELWALL_2_MIDDLE;
      break;

    case 0x16ab:
      element = EL_SIGN_EXCLAMATION;
      break;

    case 0x16ac:
      element = EL_SIGN_RADIOACTIVITY;
      break;

    case 0x16ad:
      element = EL_SIGN_STOP;
      break;

    case 0x16ae:
      element = EL_SIGN_WHEELCHAIR;
      break;

    case 0x16af:
      element = EL_SIGN_PARKING;
      break;

    case 0x16b0:
      element = EL_SIGN_NO_ENTRY;
      break;

    case 0x16b1:
      element = EL_SIGN_HEART;
      break;

    case 0x16b2:
      element = EL_SIGN_GIVE_WAY;
      break;

    case 0x16b3:
      element = EL_SIGN_ENTRY_FORBIDDEN;
      break;

    case 0x16b4:
      element = EL_SIGN_EMERGENCY_EXIT;
      break;

    case 0x16b5:
      element = EL_SIGN_YIN_YANG;
      break;

    case 0x16b6:
      element = EL_WALL_EMERALD;
      break;

    case 0x16b7:
      element = EL_WALL_DIAMOND;
      break;

    case 0x16b8:
      element = EL_WALL_PEARL;
      break;

    case 0x16b9:
      element = EL_WALL_CRYSTAL;
      break;

    case 0x16ba:
      element = EL_INVISIBLE_WALL;
      break;

    case 0x16bb:
      element = EL_INVISIBLE_STEELWALL;
      break;

      /* 0x16bc - 0x16cb: */
      /* EL_INVISIBLE_SAND */

    case 0x16cc:
      element = EL_LIGHT_SWITCH;
      break;

    case 0x16cd:
      element = EL_ENVELOPE_1;
      break;

    default:
      if (element >= 0x0117 && element <= 0x036e)	/* (?) */
	element = EL_DIAMOND;
      else if (element >= 0x042d && element <= 0x0684)	/* (?) */
	element = EL_EMERALD;
      else if (element >= 0x157c && element <= 0x158b)
	element = EL_SAND;
      else if (element >= 0x1590 && element <= 0x159f)
	element = EL_DC_LANDMINE;
      else if (element >= 0x16bc && element <= 0x16cb)
	element = EL_INVISIBLE_SAND;
      else
      {
	Error(ERR_WARN, "unknown Diamond Caves element 0x%04x", element);
	element = EL_UNKNOWN;
      }
      break;
  }

  return getMappedElement(element);
}

#if 1

static void LoadLevelFromFileStream_DC(FILE *file, struct LevelInfo *level,
				       int nr)
{
  byte header[DC_LEVEL_HEADER_SIZE];
  int envelope_size;
  int envelope_header_pos = 62;
  int envelope_content_pos = 94;
  int level_name_pos = 251;
  int level_author_pos = 292;
  int envelope_header_len;
  int envelope_content_len;
  int level_name_len;
  int level_author_len;
  int fieldx, fieldy;
  int num_yamyam_contents;
  int i, x, y;

  getDecodedWord_DC(0, TRUE);		/* initialize DC2 decoding engine */

  for (i = 0; i < DC_LEVEL_HEADER_SIZE / 2; i++)
  {
    unsigned short header_word = getDecodedWord_DC(getFile16BitBE(file), FALSE);

    header[i * 2 + 0] = header_word >> 8;
    header[i * 2 + 1] = header_word & 0xff;
  }

  /* read some values from level header to check level decoding integrity */
  fieldx = header[6] | (header[7] << 8);
  fieldy = header[8] | (header[9] << 8);
  num_yamyam_contents = header[60] | (header[61] << 8);

  /* do some simple sanity checks to ensure that level was correctly decoded */
  if (fieldx < 1 || fieldx > 256 ||
      fieldy < 1 || fieldy > 256 ||
      num_yamyam_contents < 1 || num_yamyam_contents > 8)
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot decode level from stream -- using empty level");

    return;
  }

  /* maximum envelope header size is 31 bytes */
  envelope_header_len	= header[envelope_header_pos];
  /* maximum envelope content size is 110 (156?) bytes */
  envelope_content_len	= header[envelope_content_pos];

  /* maximum level title size is 40 bytes */
  level_name_len	= MIN(header[level_name_pos],   MAX_LEVEL_NAME_LEN);
  /* maximum level author size is 30 (51?) bytes */
  level_author_len	= MIN(header[level_author_pos], MAX_LEVEL_AUTHOR_LEN);

  envelope_size = 0;

  for (i = 0; i < envelope_header_len; i++)
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] =
	header[envelope_header_pos + 1 + i];

  if (envelope_header_len > 0 && envelope_content_len > 0)
  {
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] = '\n';
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] = '\n';
  }

  for (i = 0; i < envelope_content_len; i++)
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] =
	header[envelope_content_pos + 1 + i];

  level->envelope[0].text[envelope_size] = '\0';

  level->envelope[0].xsize = MAX_ENVELOPE_XSIZE;
  level->envelope[0].ysize = 10;
  level->envelope[0].autowrap = TRUE;
  level->envelope[0].centered = TRUE;

  for (i = 0; i < level_name_len; i++)
    level->name[i] = header[level_name_pos + 1 + i];
  level->name[level_name_len] = '\0';

  for (i = 0; i < level_author_len; i++)
    level->author[i] = header[level_author_pos + 1 + i];
  level->author[level_author_len] = '\0';

  num_yamyam_contents = header[60] | (header[61] << 8);
  level->num_yamyam_contents =
    MIN(MAX(MIN_ELEMENT_CONTENTS, num_yamyam_contents), MAX_ELEMENT_CONTENTS);

  for (i = 0; i < num_yamyam_contents; i++)
  {
    for (y = 0; y < 3; y++) for (x = 0; x < 3; x++)
    {
      unsigned short word = getDecodedWord_DC(getFile16BitBE(file), FALSE);
#if 1
      int element_dc = ((word & 0xff) << 8) | ((word >> 8) & 0xff);
#else
      int element_dc = word;
#endif

      if (i < MAX_ELEMENT_CONTENTS)
	level->yamyam_content[i].e[x][y] = getMappedElement_DC(element_dc);
    }
  }

  fieldx = header[6] | (header[7] << 8);
  fieldy = header[8] | (header[9] << 8);
  level->fieldx = MIN(MAX(MIN_LEV_FIELDX, fieldx), MAX_LEV_FIELDX);
  level->fieldy = MIN(MAX(MIN_LEV_FIELDY, fieldy), MAX_LEV_FIELDY);

  for (y = 0; y < fieldy; y++) for (x = 0; x < fieldx; x++)
  {
    unsigned short word = getDecodedWord_DC(getFile16BitBE(file), FALSE);
#if 1
    int element_dc = ((word & 0xff) << 8) | ((word >> 8) & 0xff);
#else
    int element_dc = word;
#endif

    if (x < MAX_LEV_FIELDX && y < MAX_LEV_FIELDY)
      level->field[x][y] = getMappedElement_DC(element_dc);
  }

  x = MIN(MAX(0, (header[10] | (header[11] << 8)) - 1), MAX_LEV_FIELDX - 1);
  y = MIN(MAX(0, (header[12] | (header[13] << 8)) - 1), MAX_LEV_FIELDY - 1);
  level->field[x][y] = EL_PLAYER_1;

  x = MIN(MAX(0, (header[14] | (header[15] << 8)) - 1), MAX_LEV_FIELDX - 1);
  y = MIN(MAX(0, (header[16] | (header[17] << 8)) - 1), MAX_LEV_FIELDY - 1);
  level->field[x][y] = EL_PLAYER_2;

  level->gems_needed		= header[18] | (header[19] << 8);

  level->score[SC_EMERALD]	= header[20] | (header[21] << 8);
  level->score[SC_DIAMOND]	= header[22] | (header[23] << 8);
  level->score[SC_PEARL]	= header[24] | (header[25] << 8);
  level->score[SC_CRYSTAL]	= header[26] | (header[27] << 8);
  level->score[SC_NUT]		= header[28] | (header[29] << 8);
  level->score[SC_ROBOT]	= header[30] | (header[31] << 8);
  level->score[SC_SPACESHIP]	= header[32] | (header[33] << 8);
  level->score[SC_BUG]		= header[34] | (header[35] << 8);
  level->score[SC_YAMYAM]	= header[36] | (header[37] << 8);
  level->score[SC_DYNAMITE]	= header[38] | (header[39] << 8);
  level->score[SC_KEY]		= header[40] | (header[41] << 8);
  level->score[SC_TIME_BONUS]	= header[42] | (header[43] << 8);

  level->time			= header[44] | (header[45] << 8);

  level->amoeba_speed		= header[46] | (header[47] << 8);
  level->time_light		= header[48] | (header[49] << 8);
  level->time_timegate		= header[50] | (header[51] << 8);
  level->time_wheel		= header[52] | (header[53] << 8);
  level->time_magic_wall	= header[54] | (header[55] << 8);
  level->extra_time		= header[56] | (header[57] << 8);
  level->shield_normal_time	= header[58] | (header[59] << 8);

  /* Diamond Caves has the same (strange) behaviour as Emerald Mine that gems
     can slip down from flat walls, like normal walls and steel walls */
  level->em_slippery_gems = TRUE;

#if 0
  /* Diamond Caves II levels are always surrounded by indestructible wall, but
     not necessarily in a rectangular way -- fill with invisible steel wall */

  /* !!! not always true !!! keep level and set BorderElement instead !!! */

  for (y = 0; y < level->fieldy; y++) for (x = 0; x < level->fieldx; x++)
  {
#if 1
    if ((x == 0 || x == level->fieldx - 1 ||
	 y == 0 || y == level->fieldy - 1) &&
	level->field[x][y] == EL_EMPTY)
      level->field[x][y] = EL_INVISIBLE_STEELWALL;
#else
    if ((x == 0 || x == level->fieldx - 1 ||
	 y == 0 || y == level->fieldy - 1) &&
	level->field[x][y] == EL_EMPTY)
      FloodFillLevel(x, y, EL_INVISIBLE_STEELWALL,
		     level->field, level->fieldx, level->fieldy);
#endif
  }
#endif
}

static void LoadLevelFromFileInfo_DC(struct LevelInfo *level,
				     struct LevelFileInfo *level_file_info)
{
  char *filename = level_file_info->filename;
  FILE *file;
  int num_magic_bytes = 8;
  char magic_bytes[num_magic_bytes + 1];
  int num_levels_to_skip = level_file_info->nr - leveldir_current->first_level;

  if (!(file = fopen(filename, MODE_READ)))
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);

    return;
  }

  // fseek(file, 0x0000, SEEK_SET);

  if (level_file_info->packed)
  {
    /* read "magic bytes" from start of file */
    fgets(magic_bytes, num_magic_bytes + 1, file);

    /* check "magic bytes" for correct file format */
    if (!strPrefix(magic_bytes, "DC2"))
    {
      level->no_valid_file = TRUE;

      Error(ERR_WARN, "unknown DC level file '%s' -- using empty level",
	    filename);

      return;
    }

    if (strPrefix(magic_bytes, "DC2Win95") ||
	strPrefix(magic_bytes, "DC2Win98"))
    {
      int position_first_level = 0x00fa;
      int extra_bytes = 4;
      int skip_bytes;

      /* advance file stream to first level inside the level package */
      skip_bytes = position_first_level - num_magic_bytes - extra_bytes;

      /* each block of level data is followed by block of non-level data */
      num_levels_to_skip *= 2;

      /* at least skip header bytes, therefore use ">= 0" instead of "> 0" */
      while (num_levels_to_skip >= 0)
      {
	/* advance file stream to next level inside the level package */
	if (fseek(file, skip_bytes, SEEK_CUR) != 0)
	{
	  level->no_valid_file = TRUE;

	  Error(ERR_WARN, "cannot fseek in file '%s' -- using empty level",
		filename);

	  return;
	}

	/* skip apparently unused extra bytes following each level */
	ReadUnusedBytesFromFile(file, extra_bytes);

	/* read size of next level in level package */
	skip_bytes = getFile32BitLE(file);

	num_levels_to_skip--;
      }
    }
    else
    {
      level->no_valid_file = TRUE;

      Error(ERR_WARN, "unknown DC2 level file '%s' -- using empty level",
	    filename);

      return;
    }
  }

  LoadLevelFromFileStream_DC(file, level, level_file_info->nr);

  fclose(file);
}

#else

static void LoadLevelFromFileInfo_DC(struct LevelInfo *level,
				     struct LevelFileInfo *level_file_info)
{
  char *filename = level_file_info->filename;
  FILE *file;
#if 0
  int nr = level_file_info->nr - leveldir_current->first_level;
#endif
  byte header[DC_LEVEL_HEADER_SIZE];
  int envelope_size;
  int envelope_header_pos = 62;
  int envelope_content_pos = 94;
  int level_name_pos = 251;
  int level_author_pos = 292;
  int envelope_header_len;
  int envelope_content_len;
  int level_name_len;
  int level_author_len;
  int fieldx, fieldy;
  int num_yamyam_contents;
  int i, x, y;

  if (!(file = fopen(filename, MODE_READ)))
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);

    return;
  }

#if 0
  /* position file stream to the requested level inside the level package */
  if (fseek(file, nr * SP_LEVEL_SIZE, SEEK_SET) != 0)
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot fseek in file '%s' -- using empty level", filename);

    return;
  }
#endif

  getDecodedWord_DC(0, TRUE);		/* initialize DC2 decoding engine */

  for (i = 0; i < DC_LEVEL_HEADER_SIZE / 2; i++)
  {
    unsigned short header_word = getDecodedWord_DC(getFile16BitBE(file), FALSE);

    header[i * 2 + 0] = header_word >> 8;
    header[i * 2 + 1] = header_word & 0xff;
  }

  /* read some values from level header to check level decoding integrity */
  fieldx = header[6] | (header[7] << 8);
  fieldy = header[8] | (header[9] << 8);
  num_yamyam_contents = header[60] | (header[61] << 8);

  /* do some simple sanity checks to ensure that level was correctly decoded */
  if (fieldx < 1 || fieldx > 256 ||
      fieldy < 1 || fieldy > 256 ||
      num_yamyam_contents < 1 || num_yamyam_contents > 8)
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot read level from file '%s' -- using empty level",
	  filename);

    return;
  }

  /* maximum envelope header size is 31 bytes */
  envelope_header_len	= header[envelope_header_pos];
  /* maximum envelope content size is 110 (156?) bytes */
  envelope_content_len	= header[envelope_content_pos];

  /* maximum level title size is 40 bytes */
  level_name_len	= MIN(header[level_name_pos],   MAX_LEVEL_NAME_LEN);
  /* maximum level author size is 30 (51?) bytes */
  level_author_len	= MIN(header[level_author_pos], MAX_LEVEL_AUTHOR_LEN);

  envelope_size = 0;

  for (i = 0; i < envelope_header_len; i++)
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] =
	header[envelope_header_pos + 1 + i];

  if (envelope_header_len > 0 && envelope_content_len > 0)
  {
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] = '\n';
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] = '\n';
  }

  for (i = 0; i < envelope_content_len; i++)
    if (envelope_size < MAX_ENVELOPE_TEXT_LEN)
      level->envelope[0].text[envelope_size++] =
	header[envelope_content_pos + 1 + i];

  level->envelope[0].text[envelope_size] = '\0';

  level->envelope[0].xsize = MAX_ENVELOPE_XSIZE;
  level->envelope[0].ysize = 10;
  level->envelope[0].autowrap = TRUE;
  level->envelope[0].centered = TRUE;

  for (i = 0; i < level_name_len; i++)
    level->name[i] = header[level_name_pos + 1 + i];
  level->name[level_name_len] = '\0';

  for (i = 0; i < level_author_len; i++)
    level->author[i] = header[level_author_pos + 1 + i];
  level->author[level_author_len] = '\0';

  num_yamyam_contents = header[60] | (header[61] << 8);
  level->num_yamyam_contents =
    MIN(MAX(MIN_ELEMENT_CONTENTS, num_yamyam_contents), MAX_ELEMENT_CONTENTS);

  for (i = 0; i < num_yamyam_contents; i++)
  {
    for (y = 0; y < 3; y++) for (x = 0; x < 3; x++)
    {
      unsigned short word = getDecodedWord_DC(getFile16BitBE(file), FALSE);
#if 1
      int element_dc = ((word & 0xff) << 8) | ((word >> 8) & 0xff);
#else
      int element_dc = word;
#endif

      if (i < MAX_ELEMENT_CONTENTS)
	level->yamyam_content[i].e[x][y] = getMappedElement_DC(element_dc);
    }
  }

  fieldx = header[6] | (header[7] << 8);
  fieldy = header[8] | (header[9] << 8);
  level->fieldx = MIN(MAX(MIN_LEV_FIELDX, fieldx), MAX_LEV_FIELDX);
  level->fieldy = MIN(MAX(MIN_LEV_FIELDY, fieldy), MAX_LEV_FIELDY);

  for (y = 0; y < fieldy; y++) for (x = 0; x < fieldx; x++)
  {
    unsigned short word = getDecodedWord_DC(getFile16BitBE(file), FALSE);
#if 1
    int element_dc = ((word & 0xff) << 8) | ((word >> 8) & 0xff);
#else
    int element_dc = word;
#endif

    if (x < MAX_LEV_FIELDX && y < MAX_LEV_FIELDY)
      level->field[x][y] = getMappedElement_DC(element_dc);
  }

  x = MIN(MAX(0, (header[10] | (header[11] << 8)) - 1), MAX_LEV_FIELDX - 1);
  y = MIN(MAX(0, (header[12] | (header[13] << 8)) - 1), MAX_LEV_FIELDY - 1);
  level->field[x][y] = EL_PLAYER_1;

  x = MIN(MAX(0, (header[14] | (header[15] << 8)) - 1), MAX_LEV_FIELDX - 1);
  y = MIN(MAX(0, (header[16] | (header[17] << 8)) - 1), MAX_LEV_FIELDY - 1);
  level->field[x][y] = EL_PLAYER_2;

  level->gems_needed		= header[18] | (header[19] << 8);

  level->score[SC_EMERALD]	= header[20] | (header[21] << 8);
  level->score[SC_DIAMOND]	= header[22] | (header[23] << 8);
  level->score[SC_PEARL]	= header[24] | (header[25] << 8);
  level->score[SC_CRYSTAL]	= header[26] | (header[27] << 8);
  level->score[SC_NUT]		= header[28] | (header[29] << 8);
  level->score[SC_ROBOT]	= header[30] | (header[31] << 8);
  level->score[SC_SPACESHIP]	= header[32] | (header[33] << 8);
  level->score[SC_BUG]		= header[34] | (header[35] << 8);
  level->score[SC_YAMYAM]	= header[36] | (header[37] << 8);
  level->score[SC_DYNAMITE]	= header[38] | (header[39] << 8);
  level->score[SC_KEY]		= header[40] | (header[41] << 8);
  level->score[SC_TIME_BONUS]	= header[42] | (header[43] << 8);

  level->time			= header[44] | (header[45] << 8);

  level->amoeba_speed		= header[46] | (header[47] << 8);
  level->time_light		= header[48] | (header[49] << 8);
  level->time_timegate		= header[50] | (header[51] << 8);
  level->time_wheel		= header[52] | (header[53] << 8);
  level->time_magic_wall	= header[54] | (header[55] << 8);
  level->extra_time		= header[56] | (header[57] << 8);
  level->shield_normal_time	= header[58] | (header[59] << 8);

  fclose(file);

  /* Diamond Caves has the same (strange) behaviour as Emerald Mine that gems
     can slip down from flat walls, like normal walls and steel walls */
  level->em_slippery_gems = TRUE;

#if 0
  /* Diamond Caves II levels are always surrounded by indestructible wall, but
     not necessarily in a rectangular way -- fill with invisible steel wall */

  /* !!! not always true !!! keep level and set BorderElement instead !!! */

  for (y = 0; y < level->fieldy; y++) for (x = 0; x < level->fieldx; x++)
  {
#if 1
    if ((x == 0 || x == level->fieldx - 1 ||
	 y == 0 || y == level->fieldy - 1) &&
	level->field[x][y] == EL_EMPTY)
      level->field[x][y] = EL_INVISIBLE_STEELWALL;
#else
    if ((x == 0 || x == level->fieldx - 1 ||
	 y == 0 || y == level->fieldy - 1) &&
	level->field[x][y] == EL_EMPTY)
      FloodFillLevel(x, y, EL_INVISIBLE_STEELWALL,
		     level->field, level->fieldx, level->fieldy);
#endif
  }
#endif
}

#endif


/* ------------------------------------------------------------------------- */
/* functions for loading SB level                                            */
/* ------------------------------------------------------------------------- */

int getMappedElement_SB(int element_ascii, boolean use_ces)
{
  static struct
  {
    int ascii;
    int sb;
    int ce;
  }
  sb_element_mapping[] =
  {
    { ' ', EL_EMPTY,                EL_CUSTOM_1 },  /* floor (space) */
    { '#', EL_STEELWALL,            EL_CUSTOM_2 },  /* wall */
    { '@', EL_PLAYER_1,             EL_CUSTOM_3 },  /* player */
    { '$', EL_SOKOBAN_OBJECT,       EL_CUSTOM_4 },  /* box */
    { '.', EL_SOKOBAN_FIELD_EMPTY,  EL_CUSTOM_5 },  /* goal square */
    { '*', EL_SOKOBAN_FIELD_FULL,   EL_CUSTOM_6 },  /* box on goal square */
    { '+', EL_SOKOBAN_FIELD_PLAYER, EL_CUSTOM_7 },  /* player on goal square */
#if 0
    { '_', EL_INVISIBLE_STEELWALL,  EL_CUSTOM_8 },  /* floor beyond border */
#else
    { '_', EL_INVISIBLE_STEELWALL,  EL_FROM_LEVEL_TEMPLATE },  /* floor beyond border */
#endif

    { 0,   -1,                      -1          },
  };

  int i;

  for (i = 0; sb_element_mapping[i].ascii != 0; i++)
    if (element_ascii == sb_element_mapping[i].ascii)
      return (use_ces ? sb_element_mapping[i].ce : sb_element_mapping[i].sb);

  return EL_UNDEFINED;
}

static void LoadLevelFromFileInfo_SB(struct LevelInfo *level,
				     struct LevelFileInfo *level_file_info)
{
  char *filename = level_file_info->filename;
  char line[MAX_LINE_LEN], line_raw[MAX_LINE_LEN], previous_line[MAX_LINE_LEN];
  char last_comment[MAX_LINE_LEN];
  char level_name[MAX_LINE_LEN];
  char *line_ptr;
  FILE *file;
  int num_levels_to_skip = level_file_info->nr - leveldir_current->first_level;
  boolean read_continued_line = FALSE;
  boolean reading_playfield = FALSE;
  boolean got_valid_playfield_line = FALSE;
  boolean invalid_playfield_char = FALSE;
  boolean load_xsb_to_ces = check_special_flags("load_xsb_to_ces");
  int file_level_nr = 0;
  int line_nr = 0;
  int x = 0, y = 0;		/* initialized to make compilers happy */

#if 0
  printf("::: looking for level number %d [%d]\n",
	 level_file_info->nr, num_levels_to_skip);
#endif

  last_comment[0] = '\0';
  level_name[0] = '\0';

  if (!(file = fopen(filename, MODE_READ)))
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);

    return;
  }

  while (!feof(file))
  {
    /* level successfully read, but next level may follow here */
    if (!got_valid_playfield_line && reading_playfield)
    {
#if 0
      printf("::: read complete playfield\n");
#endif

      /* read playfield from single level file -- skip remaining file */
      if (!level_file_info->packed)
	break;

      if (file_level_nr >= num_levels_to_skip)
	break;

      file_level_nr++;

      last_comment[0] = '\0';
      level_name[0] = '\0';

      reading_playfield = FALSE;
    }

    got_valid_playfield_line = FALSE;

    /* read next line of input file */
    if (!fgets(line, MAX_LINE_LEN, file))
      break;

    /* check if line was completely read and is terminated by line break */
    if (strlen(line) > 0 && line[strlen(line) - 1] == '\n')
      line_nr++;

    /* cut trailing line break (this can be newline and/or carriage return) */
    for (line_ptr = &line[strlen(line)]; line_ptr >= line; line_ptr--)
      if ((*line_ptr == '\n' || *line_ptr == '\r') && *(line_ptr + 1) == '\0')
        *line_ptr = '\0';

    /* copy raw input line for later use (mainly debugging output) */
    strcpy(line_raw, line);

    if (read_continued_line)
    {
      /* append new line to existing line, if there is enough space */
      if (strlen(previous_line) + strlen(line_ptr) < MAX_LINE_LEN)
        strcat(previous_line, line_ptr);

      strcpy(line, previous_line);      /* copy storage buffer to line */

      read_continued_line = FALSE;
    }

    /* if the last character is '\', continue at next line */
    if (strlen(line) > 0 && line[strlen(line) - 1] == '\\')
    {
      line[strlen(line) - 1] = '\0';    /* cut off trailing backslash */
      strcpy(previous_line, line);      /* copy line to storage buffer */

      read_continued_line = TRUE;

      continue;
    }

    /* skip empty lines */
    if (line[0] == '\0')
      continue;

    /* extract comment text from comment line */
    if (line[0] == ';')
    {
      for (line_ptr = line; *line_ptr; line_ptr++)
        if (*line_ptr != ' ' && *line_ptr != '\t' && *line_ptr != ';')
          break;

      strcpy(last_comment, line_ptr);

#if 0
      printf("::: found comment '%s' in line %d\n", last_comment, line_nr);
#endif

      continue;
    }

    /* extract level title text from line containing level title */
    if (line[0] == '\'')
    {
      strcpy(level_name, &line[1]);

      if (strlen(level_name) > 0 && level_name[strlen(level_name) - 1] == '\'')
	level_name[strlen(level_name) - 1] = '\0';

#if 0
      printf("::: found level name '%s' in line %d\n", level_name, line_nr);
#endif

      continue;
    }

    /* skip lines containing only spaces (or empty lines) */
    for (line_ptr = line; *line_ptr; line_ptr++)
      if (*line_ptr != ' ')
	break;
    if (*line_ptr == '\0')
      continue;

    /* at this point, we have found a line containing part of a playfield */

#if 0
    printf("::: found playfield row in line %d\n", line_nr);
#endif

    got_valid_playfield_line = TRUE;

    if (!reading_playfield)
    {
      reading_playfield = TRUE;
      invalid_playfield_char = FALSE;

      for (x = 0; x < MAX_LEV_FIELDX; x++)
	for (y = 0; y < MAX_LEV_FIELDY; y++)
	  level->field[x][y] = getMappedElement_SB(' ', load_xsb_to_ces);

      level->fieldx = 0;
      level->fieldy = 0;

      /* start with topmost tile row */
      y = 0;
    }

    /* skip playfield line if larger row than allowed */
    if (y >= MAX_LEV_FIELDY)
      continue;

    /* start with leftmost tile column */
    x = 0;

    /* read playfield elements from line */
    for (line_ptr = line; *line_ptr; line_ptr++)
    {
      int mapped_sb_element = getMappedElement_SB(*line_ptr, load_xsb_to_ces);

      /* stop parsing playfield line if larger column than allowed */
      if (x >= MAX_LEV_FIELDX)
	break;

      if (mapped_sb_element == EL_UNDEFINED)
      {
	invalid_playfield_char = TRUE;

	break;
      }

      level->field[x][y] = mapped_sb_element;

      /* continue with next tile column */
      x++;

      level->fieldx = MAX(x, level->fieldx);
    }

    if (invalid_playfield_char)
    {
      /* if first playfield line, treat invalid lines as comment lines */
      if (y == 0)
	reading_playfield = FALSE;

      continue;
    }

    /* continue with next tile row */
    y++;
  }

  fclose(file);

  level->fieldy = y;

  level->fieldx = MIN(MAX(MIN_LEV_FIELDX, level->fieldx), MAX_LEV_FIELDX);
  level->fieldy = MIN(MAX(MIN_LEV_FIELDY, level->fieldy), MAX_LEV_FIELDY);

  if (!reading_playfield)
  {
    level->no_valid_file = TRUE;

    Error(ERR_WARN, "cannot read level '%s' -- using empty level", filename);

    return;
  }

  if (*level_name != '\0')
  {
    strncpy(level->name, level_name, MAX_LEVEL_NAME_LEN);
    level->name[MAX_LEVEL_NAME_LEN] = '\0';

#if 0
    printf(":1: level name: '%s'\n", level->name);
#endif
  }
  else if (*last_comment != '\0')
  {
    strncpy(level->name, last_comment, MAX_LEVEL_NAME_LEN);
    level->name[MAX_LEVEL_NAME_LEN] = '\0';

#if 0
    printf(":2: level name: '%s'\n", level->name);
#endif
  }
  else
  {
    sprintf(level->name, "--> Level %d <--", level_file_info->nr);
  }

  /* set all empty fields beyond the border walls to invisible steel wall */
  for (y = 0; y < level->fieldy; y++) for (x = 0; x < level->fieldx; x++)
  {
    if ((x == 0 || x == level->fieldx - 1 ||
	 y == 0 || y == level->fieldy - 1) &&
	level->field[x][y] == getMappedElement_SB(' ', load_xsb_to_ces))
      FloodFillLevel(x, y, getMappedElement_SB('_', load_xsb_to_ces),
		     level->field, level->fieldx, level->fieldy);
  }

  /* set special level settings for Sokoban levels */

  level->time = 0;
  level->use_step_counter = TRUE;

  if (load_xsb_to_ces)
  {
#if 1
    /* !!! special global settings can now be set in level template !!! */
#else
    level->initial_player_stepsize[0] = STEPSIZE_SLOW;
#endif

    /* fill smaller playfields with padding "beyond border wall" elements */
    if (level->fieldx < SCR_FIELDX ||
	level->fieldy < SCR_FIELDY)
    {
      short field[level->fieldx][level->fieldy];
      int new_fieldx = MAX(level->fieldx, SCR_FIELDX);
      int new_fieldy = MAX(level->fieldy, SCR_FIELDY);
      int pos_fieldx = (new_fieldx - level->fieldx) / 2;
      int pos_fieldy = (new_fieldy - level->fieldy) / 2;

      /* copy old playfield (which is smaller than the visible area) */
      for (y = 0; y < level->fieldy; y++) for (x = 0; x < level->fieldx; x++)
	field[x][y] = level->field[x][y];

      /* fill new, larger playfield with "beyond border wall" elements */
      for (y = 0; y < new_fieldy; y++) for (x = 0; x < new_fieldx; x++)
	level->field[x][y] = getMappedElement_SB('_', load_xsb_to_ces);

      /* copy the old playfield to the middle of the new playfield */
      for (y = 0; y < level->fieldy; y++) for (x = 0; x < level->fieldx; x++)
	level->field[pos_fieldx + x][pos_fieldy + y] = field[x][y];

      level->fieldx = new_fieldx;
      level->fieldy = new_fieldy;
    }

    level->use_custom_template = TRUE;
  }
}


/* ------------------------------------------------------------------------- */
/* functions for handling native levels                                      */
/* ------------------------------------------------------------------------- */

static void LoadLevelFromFileInfo_EM(struct LevelInfo *level,
				     struct LevelFileInfo *level_file_info)
{
  if (!LoadNativeLevel_EM(level_file_info->filename))
    level->no_valid_file = TRUE;
}

static void LoadLevelFromFileInfo_SP(struct LevelInfo *level,
				     struct LevelFileInfo *level_file_info)
{
  int pos = 0;

  /* determine position of requested level inside level package */
  if (level_file_info->packed)
    pos = level_file_info->nr - leveldir_current->first_level;

  if (!LoadNativeLevel_SP(level_file_info->filename, pos))
    level->no_valid_file = TRUE;
}

void CopyNativeLevel_RND_to_Native(struct LevelInfo *level)
{
  if (level->game_engine_type == GAME_ENGINE_TYPE_EM)
    CopyNativeLevel_RND_to_EM(level);
  else if (level->game_engine_type == GAME_ENGINE_TYPE_SP)
    CopyNativeLevel_RND_to_SP(level);
}

void CopyNativeLevel_Native_to_RND(struct LevelInfo *level)
{
  if (level->game_engine_type == GAME_ENGINE_TYPE_EM)
    CopyNativeLevel_EM_to_RND(level);
  else if (level->game_engine_type == GAME_ENGINE_TYPE_SP)
    CopyNativeLevel_SP_to_RND(level);
}

void SaveNativeLevel(struct LevelInfo *level)
{
  if (level->game_engine_type == GAME_ENGINE_TYPE_SP)
  {
    char *basename = getSingleLevelBasenameExt(level->file_info.nr, "sp");
    char *filename = getLevelFilenameFromBasename(basename);

    CopyNativeLevel_RND_to_SP(level);
    CopyNativeTape_RND_to_SP(level);

    SaveNativeLevel_SP(filename);
  }
}


/* ------------------------------------------------------------------------- */
/* functions for loading generic level                                       */
/* ------------------------------------------------------------------------- */

void LoadLevelFromFileInfo(struct LevelInfo *level,
			   struct LevelFileInfo *level_file_info)
{
  /* always start with reliable default values */
  setLevelInfoToDefaults(level);

  switch (level_file_info->type)
  {
    case LEVEL_FILE_TYPE_RND:
      LoadLevelFromFileInfo_RND(level, level_file_info);
      break;

    case LEVEL_FILE_TYPE_EM:
      LoadLevelFromFileInfo_EM(level, level_file_info);
      level->game_engine_type = GAME_ENGINE_TYPE_EM;
      break;

    case LEVEL_FILE_TYPE_SP:
      LoadLevelFromFileInfo_SP(level, level_file_info);
      level->game_engine_type = GAME_ENGINE_TYPE_SP;
      break;

    case LEVEL_FILE_TYPE_DC:
      LoadLevelFromFileInfo_DC(level, level_file_info);
      break;

    case LEVEL_FILE_TYPE_SB:
      LoadLevelFromFileInfo_SB(level, level_file_info);
      break;

    default:
      LoadLevelFromFileInfo_RND(level, level_file_info);
      break;
  }

  /* if level file is invalid, restore level structure to default values */
  if (level->no_valid_file)
    setLevelInfoToDefaults(level);

  if (level->game_engine_type == GAME_ENGINE_TYPE_UNKNOWN)
    level->game_engine_type = GAME_ENGINE_TYPE_RND;

  if (level_file_info->type != LEVEL_FILE_TYPE_RND)
    CopyNativeLevel_Native_to_RND(level);
}

void LoadLevelFromFilename(struct LevelInfo *level, char *filename)
{
  static struct LevelFileInfo level_file_info;

  /* always start with reliable default values */
  setFileInfoToDefaults(&level_file_info);

  level_file_info.nr = 0;			/* unknown level number */
  level_file_info.type = LEVEL_FILE_TYPE_RND;	/* no others supported yet */
  level_file_info.filename = filename;

  LoadLevelFromFileInfo(level, &level_file_info);
}

static void LoadLevel_InitVersion(struct LevelInfo *level, char *filename)
{
  int i, j;

  if (leveldir_current == NULL)		/* only when dumping level */
    return;

  /* all engine modifications also valid for levels which use latest engine */
  if (level->game_version < VERSION_IDENT(3,2,0,5))
  {
    /* time bonus score was given for 10 s instead of 1 s before 3.2.0-5 */
    level->score[SC_TIME_BONUS] /= 10;
  }

#if 0
  leveldir_current->latest_engine = TRUE;	/* !!! TEST ONLY !!! */
#endif

  if (leveldir_current->latest_engine)
  {
    /* ---------- use latest game engine ----------------------------------- */

    /* For all levels which are forced to use the latest game engine version
       (normally all but user contributed, private and undefined levels), set
       the game engine version to the actual version; this allows for actual
       corrections in the game engine to take effect for existing, converted
       levels (from "classic" or other existing games) to make the emulation
       of the corresponding game more accurate, while (hopefully) not breaking
       existing levels created from other players. */

    level->game_version = GAME_VERSION_ACTUAL;

    /* Set special EM style gems behaviour: EM style gems slip down from
       normal, steel and growing wall. As this is a more fundamental change,
       it seems better to set the default behaviour to "off" (as it is more
       natural) and make it configurable in the level editor (as a property
       of gem style elements). Already existing converted levels (neither
       private nor contributed levels) are changed to the new behaviour. */

    if (level->file_version < FILE_VERSION_2_0)
      level->em_slippery_gems = TRUE;

    return;
  }

  /* ---------- use game engine the level was created with ----------------- */

  /* For all levels which are not forced to use the latest game engine
     version (normally user contributed, private and undefined levels),
     use the version of the game engine the levels were created for.

     Since 2.0.1, the game engine version is now directly stored
     in the level file (chunk "VERS"), so there is no need anymore
     to set the game version from the file version (except for old,
     pre-2.0 levels, where the game version is still taken from the
     file format version used to store the level -- see above). */

  /* player was faster than enemies in 1.0.0 and before */
  if (level->file_version == FILE_VERSION_1_0)
    for (i = 0; i < MAX_PLAYERS; i++)
      level->initial_player_stepsize[i] = STEPSIZE_FAST;

  /* default behaviour for EM style gems was "slippery" only in 2.0.1 */
  if (level->game_version == VERSION_IDENT(2,0,1,0))
    level->em_slippery_gems = TRUE;

  /* springs could be pushed over pits before (pre-release version) 2.2.0 */
  if (level->game_version < VERSION_IDENT(2,2,0,0))
    level->use_spring_bug = TRUE;

  if (level->game_version < VERSION_IDENT(3,2,0,5))
  {
    /* time orb caused limited time in endless time levels before 3.2.0-5 */
    level->use_time_orb_bug = TRUE;

    /* default behaviour for snapping was "no snap delay" before 3.2.0-5 */
    level->block_snap_field = FALSE;

    /* extra time score was same value as time left score before 3.2.0-5 */
    level->extra_time_score = level->score[SC_TIME_BONUS];

#if 0
    /* time bonus score was given for 10 s instead of 1 s before 3.2.0-5 */
    level->score[SC_TIME_BONUS] /= 10;
#endif
  }

  if (level->game_version < VERSION_IDENT(3,2,0,7))
  {
    /* default behaviour for snapping was "not continuous" before 3.2.0-7 */
    level->continuous_snapping = FALSE;
  }

  /* only few elements were able to actively move into acid before 3.1.0 */
  /* trigger settings did not exist before 3.1.0; set to default "any" */
  if (level->game_version < VERSION_IDENT(3,1,0,0))
  {
    /* correct "can move into acid" settings (all zero in old levels) */

    level->can_move_into_acid_bits = 0; /* nothing can move into acid */
    level->dont_collide_with_bits = 0; /* nothing is deadly when colliding */

    setMoveIntoAcidProperty(level, EL_ROBOT,     TRUE);
    setMoveIntoAcidProperty(level, EL_SATELLITE, TRUE);
    setMoveIntoAcidProperty(level, EL_PENGUIN,   TRUE);
    setMoveIntoAcidProperty(level, EL_BALLOON,   TRUE);

    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
      SET_PROPERTY(EL_CUSTOM_START + i, EP_CAN_MOVE_INTO_ACID, TRUE);

    /* correct trigger settings (stored as zero == "none" in old levels) */

    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;
      struct ElementInfo *ei = &element_info[element];

      for (j = 0; j < ei->num_change_pages; j++)
      {
	struct ElementChangeInfo *change = &ei->change_page[j];

	change->trigger_player = CH_PLAYER_ANY;
	change->trigger_page = CH_PAGE_ANY;
      }
    }
  }

  /* try to detect and fix "Snake Bite" levels, which are broken with 3.2.0 */
  {
    int element = EL_CUSTOM_256;
    struct ElementInfo *ei = &element_info[element];
    struct ElementChangeInfo *change = &ei->change_page[0];

    /* This is needed to fix a problem that was caused by a bugfix in function
       game.c/CreateFieldExt() introduced with 3.2.0 that corrects the behaviour
       when a custom element changes to EL_SOKOBAN_FIELD_PLAYER (before, it did
       not replace walkable elements, but instead just placed the player on it,
       without placing the Sokoban field under the player). Unfortunately, this
       breaks "Snake Bite" style levels when the snake is halfway through a door
       that just closes (the snake head is still alive and can be moved in this
       case). This can be fixed by replacing the EL_SOKOBAN_FIELD_PLAYER by the
       player (without Sokoban element) which then gets killed as designed). */

    if ((strncmp(leveldir_current->identifier, "snake_bite", 10) == 0 ||
	 strncmp(ei->description, "pause b4 death", 14) == 0) &&
	change->target_element == EL_SOKOBAN_FIELD_PLAYER)
      change->target_element = EL_PLAYER_1;
  }

#if 1
  /* try to detect and fix "Zelda" style levels, which are broken with 3.2.5 */
  if (level->game_version < VERSION_IDENT(3,2,5,0))
  {
    /* This is needed to fix a problem that was caused by a bugfix in function
       game.c/CheckTriggeredElementChangeExt() introduced with 3.2.5 that
       corrects the behaviour when a custom element changes to another custom
       element with a higher element number that has change actions defined.
       Normally, only one change per frame is allowed for custom elements.
       Therefore, it is checked if a custom element already changed in the
       current frame; if it did, subsequent changes are suppressed.
       Unfortunately, this is only checked for element changes, but not for
       change actions, which are still executed. As the function above loops
       through all custom elements from lower to higher, an element change
       resulting in a lower CE number won't be checked again, while a target
       element with a higher number will also be checked, and potential change
       actions will get executed for this CE, too (which is wrong), while
       further changes are ignored (which is correct). As this bugfix breaks
       Zelda II (and introduces graphical bugs to Zelda I, and also breaks a
       few other levels like Alan Bond's "FMV"), allow the previous, incorrect
       behaviour for existing levels and tapes that make use of this bug */

    level->use_action_after_change_bug = TRUE;
  }
#else
  /* !!! THIS DOES NOT FIX "Zelda I" (GRAPHICALLY) AND "Alan's FMV" LEVELS */
  /* try to detect and fix "Zelda II" levels, which are broken with 3.2.5 */
  {
    int element = EL_CUSTOM_16;
    struct ElementInfo *ei = &element_info[element];

    /* This is needed to fix a problem that was caused by a bugfix in function
       game.c/CheckTriggeredElementChangeExt() introduced with 3.2.5 that
       corrects the behaviour when a custom element changes to another custom
       element with a higher element number that has change actions defined.
       Normally, only one change per frame is allowed for custom elements.
       Therefore, it is checked if a custom element already changed in the
       current frame; if it did, subsequent changes are suppressed.
       Unfortunately, this is only checked for element changes, but not for
       change actions, which are still executed. As the function above loops
       through all custom elements from lower to higher, an element change
       resulting in a lower CE number won't be checked again, while a target
       element with a higher number will also be checked, and potential change
       actions will get executed for this CE, too (which is wrong), while
       further changes are ignored (which is correct). As this bugfix breaks
       Zelda II (but no other levels), allow the previous, incorrect behaviour
       for this outstanding level set to not break the game or existing tapes */

    if (strncmp(leveldir_current->identifier, "zelda2", 6) == 0 ||
	strncmp(ei->description, "scanline - row 1", 16) == 0)
      level->use_action_after_change_bug = TRUE;
  }
#endif

  /* not centering level after relocating player was default only in 3.2.3 */
  if (level->game_version == VERSION_IDENT(3,2,3,0))	/* (no pre-releases) */
    level->shifted_relocation = TRUE;

  /* EM style elements always chain-exploded in R'n'D engine before 3.2.6 */
  if (level->game_version < VERSION_IDENT(3,2,6,0))
    level->em_explodes_by_fire = TRUE;
}

static void LoadLevel_InitElements(struct LevelInfo *level, char *filename)
{
  int i, j, x, y;

  /* map custom element change events that have changed in newer versions
     (these following values were accidentally changed in version 3.0.1)
     (this seems to be needed only for 'ab_levelset3' and 'ab_levelset4') */
  if (level->game_version <= VERSION_IDENT(3,0,0,0))
  {
    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;

      /* order of checking and copying events to be mapped is important */
      /* (do not change the start and end value -- they are constant) */
      for (j = CE_BY_OTHER_ACTION; j >= CE_VALUE_GETS_ZERO; j--)
      {
	if (HAS_CHANGE_EVENT(element, j - 2))
	{
	  SET_CHANGE_EVENT(element, j - 2, FALSE);
	  SET_CHANGE_EVENT(element, j, TRUE);
	}
      }

      /* order of checking and copying events to be mapped is important */
      /* (do not change the start and end value -- they are constant) */
      for (j = CE_PLAYER_COLLECTS_X; j >= CE_HITTING_SOMETHING; j--)
      {
	if (HAS_CHANGE_EVENT(element, j - 1))
	{
	  SET_CHANGE_EVENT(element, j - 1, FALSE);
	  SET_CHANGE_EVENT(element, j, TRUE);
	}
      }
    }
  }

  /* initialize "can_change" field for old levels with only one change page */
  if (level->game_version <= VERSION_IDENT(3,0,2,0))
  {
    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;

      if (CAN_CHANGE(element))
	element_info[element].change->can_change = TRUE;
    }
  }

  /* correct custom element values (for old levels without these options) */
  if (level->game_version < VERSION_IDENT(3,1,1,0))
  {
    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;
      struct ElementInfo *ei = &element_info[element];

      if (ei->access_direction == MV_NO_DIRECTION)
	ei->access_direction = MV_ALL_DIRECTIONS;
    }
  }

  /* correct custom element values (fix invalid values for all versions) */
  if (1)
  {
    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;
      struct ElementInfo *ei = &element_info[element];

      for (j = 0; j < ei->num_change_pages; j++)
      {
	struct ElementChangeInfo *change = &ei->change_page[j];

	if (change->trigger_player == CH_PLAYER_NONE)
	  change->trigger_player = CH_PLAYER_ANY;

	if (change->trigger_side == CH_SIDE_NONE)
	  change->trigger_side = CH_SIDE_ANY;
      }
    }
  }

  /* initialize "can_explode" field for old levels which did not store this */
  /* !!! CHECK THIS -- "<= 3,1,0,0" IS PROBABLY WRONG !!! */
  if (level->game_version <= VERSION_IDENT(3,1,0,0))
  {
    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;

      if (EXPLODES_1X1_OLD(element))
	element_info[element].explosion_type = EXPLODES_1X1;

      SET_PROPERTY(element, EP_CAN_EXPLODE, (EXPLODES_BY_FIRE(element) ||
					     EXPLODES_SMASHED(element) ||
					     EXPLODES_IMPACT(element)));
    }
  }

  /* correct previously hard-coded move delay values for maze runner style */
  if (level->game_version < VERSION_IDENT(3,1,1,0))
  {
    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;

      if (element_info[element].move_pattern & MV_MAZE_RUNNER_STYLE)
      {
	/* previously hard-coded and therefore ignored */
	element_info[element].move_delay_fixed = 9;
	element_info[element].move_delay_random = 0;
      }
    }
  }

  /* map elements that have changed in newer versions */
  level->amoeba_content = getMappedElementByVersion(level->amoeba_content,
						    level->game_version);
  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (x = 0; x < 3; x++)
      for (y = 0; y < 3; y++)
	level->yamyam_content[i].e[x][y] =
	  getMappedElementByVersion(level->yamyam_content[i].e[x][y],
				    level->game_version);

  /* initialize element properties for level editor etc. */
  InitElementPropertiesEngine(level->game_version);
  InitElementPropertiesAfterLoading(level->game_version);
  InitElementPropertiesGfxElement();
}

static void LoadLevel_InitPlayfield(struct LevelInfo *level, char *filename)
{
  int x, y;

  /* map elements that have changed in newer versions */
  for (y = 0; y < level->fieldy; y++)
    for (x = 0; x < level->fieldx; x++)
      level->field[x][y] = getMappedElementByVersion(level->field[x][y],
						     level->game_version);

  /* copy elements to runtime playfield array */
  for (x = 0; x < MAX_LEV_FIELDX; x++)
    for (y = 0; y < MAX_LEV_FIELDY; y++)
      Feld[x][y] = level->field[x][y];

  /* initialize level size variables for faster access */
  lev_fieldx = level->fieldx;
  lev_fieldy = level->fieldy;

  /* determine border element for this level */
  if (level->file_info.type == LEVEL_FILE_TYPE_DC)
    BorderElement = EL_EMPTY;	/* (in editor, SetBorderElement() is used) */
  else
    SetBorderElement();
}

static void LoadLevel_InitNativeEngines(struct LevelInfo *level,char *filename)
{
  struct LevelFileInfo *level_file_info = &level->file_info;

  if (level_file_info->type == LEVEL_FILE_TYPE_RND)
    CopyNativeLevel_RND_to_Native(level);
}

void LoadLevelTemplate(int nr)
{
  char *filename;

  setLevelFileInfo(&level_template.file_info, nr);
  filename = level_template.file_info.filename;

  LoadLevelFromFileInfo(&level_template, &level_template.file_info);

  LoadLevel_InitVersion(&level_template, filename);
  LoadLevel_InitElements(&level_template, filename);

  ActivateLevelTemplate();
}

void LoadLevel(int nr)
{
  char *filename;

  setLevelFileInfo(&level.file_info, nr);
  filename = level.file_info.filename;

  LoadLevelFromFileInfo(&level, &level.file_info);

  if (level.use_custom_template)
    LoadLevelTemplate(-1);

  LoadLevel_InitVersion(&level, filename);
  LoadLevel_InitElements(&level, filename);
  LoadLevel_InitPlayfield(&level, filename);

  LoadLevel_InitNativeEngines(&level, filename);
}

static int SaveLevel_VERS(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;

  chunk_size += putFileVersion(file, level->file_version);
  chunk_size += putFileVersion(file, level->game_version);

  return chunk_size;
}

static int SaveLevel_DATE(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;

  chunk_size += putFile16BitBE(file, level->creation_date.year);
  chunk_size += putFile8Bit(file,    level->creation_date.month);
  chunk_size += putFile8Bit(file,    level->creation_date.day);

  return chunk_size;
}

#if 0
static void SaveLevel_HEAD(FILE *file, struct LevelInfo *level)
{
  int i, x, y;

  putFile8Bit(file, level->fieldx);
  putFile8Bit(file, level->fieldy);

  putFile16BitBE(file, level->time);
  putFile16BitBE(file, level->gems_needed);

  for (i = 0; i < MAX_LEVEL_NAME_LEN; i++)
    putFile8Bit(file, level->name[i]);

  for (i = 0; i < LEVEL_SCORE_ELEMENTS; i++)
    putFile8Bit(file, level->score[i]);

  for (i = 0; i < STD_ELEMENT_CONTENTS; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	putFile8Bit(file, (level->encoding_16bit_yamyam ? EL_EMPTY :
			   level->yamyam_content[i].e[x][y]));
  putFile8Bit(file, level->amoeba_speed);
  putFile8Bit(file, level->time_magic_wall);
  putFile8Bit(file, level->time_wheel);
  putFile8Bit(file, (level->encoding_16bit_amoeba ? EL_EMPTY :
		     level->amoeba_content));
  putFile8Bit(file, (level->initial_player_stepsize == STEPSIZE_FAST ? 1 : 0));
  putFile8Bit(file, (level->initial_gravity ? 1 : 0));
  putFile8Bit(file, (level->encoding_16bit_field ? 1 : 0));
  putFile8Bit(file, (level->em_slippery_gems ? 1 : 0));

  putFile8Bit(file, (level->use_custom_template ? 1 : 0));

  putFile8Bit(file, (level->block_last_field ? 1 : 0));
  putFile8Bit(file, (level->sp_block_last_field ? 1 : 0));
  putFile32BitBE(file, level->can_move_into_acid_bits);
  putFile8Bit(file, level->dont_collide_with_bits);

  putFile8Bit(file, (level->use_spring_bug ? 1 : 0));
  putFile8Bit(file, (level->use_step_counter ? 1 : 0));

  putFile8Bit(file, (level->instant_relocation ? 1 : 0));
  putFile8Bit(file, (level->can_pass_to_walkable ? 1 : 0));
  putFile8Bit(file, (level->grow_into_diggable ? 1 : 0));

  putFile8Bit(file, level->game_engine_type);

  WriteUnusedBytesToFile(file, LEVEL_CHUNK_HEAD_UNUSED);
}
#endif

static int SaveLevel_NAME(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;
  int i;

  for (i = 0; i < MAX_LEVEL_NAME_LEN; i++)
    chunk_size += putFile8Bit(file, level->name[i]);

  return chunk_size;
}

static int SaveLevel_AUTH(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;
  int i;

  for (i = 0; i < MAX_LEVEL_AUTHOR_LEN; i++)
    chunk_size += putFile8Bit(file, level->author[i]);

  return chunk_size;
}

#if 0
static int SaveLevel_BODY(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;
  int x, y;

  for (y = 0; y < level->fieldy; y++) 
    for (x = 0; x < level->fieldx; x++) 
      if (level->encoding_16bit_field)
	chunk_size += putFile16BitBE(file, level->field[x][y]);
      else
	chunk_size += putFile8Bit(file, level->field[x][y]);

  return chunk_size;
}
#endif

static int SaveLevel_BODY(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;
  int x, y;

  for (y = 0; y < level->fieldy; y++) 
    for (x = 0; x < level->fieldx; x++) 
      chunk_size += putFile16BitBE(file, level->field[x][y]);

  return chunk_size;
}

#if 0
static void SaveLevel_CONT(FILE *file, struct LevelInfo *level)
{
  int i, x, y;

  putFile8Bit(file, EL_YAMYAM);
  putFile8Bit(file, level->num_yamyam_contents);
  putFile8Bit(file, 0);
  putFile8Bit(file, 0);

  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	if (level->encoding_16bit_field)
	  putFile16BitBE(file, level->yamyam_content[i].e[x][y]);
	else
	  putFile8Bit(file, level->yamyam_content[i].e[x][y]);
}
#endif

#if 0
static void SaveLevel_CNT2(FILE *file, struct LevelInfo *level, int element)
{
  int i, x, y;
  int num_contents, content_xsize, content_ysize;
  int content_array[MAX_ELEMENT_CONTENTS][3][3];

  if (element == EL_YAMYAM)
  {
    num_contents = level->num_yamyam_contents;
    content_xsize = 3;
    content_ysize = 3;

    for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
      for (y = 0; y < 3; y++)
	for (x = 0; x < 3; x++)
	  content_array[i][x][y] = level->yamyam_content[i].e[x][y];
  }
  else if (element == EL_BD_AMOEBA)
  {
    num_contents = 1;
    content_xsize = 1;
    content_ysize = 1;

    for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
      for (y = 0; y < 3; y++)
	for (x = 0; x < 3; x++)
	  content_array[i][x][y] = EL_EMPTY;
    content_array[0][0][0] = level->amoeba_content;
  }
  else
  {
    /* chunk header already written -- write empty chunk data */
    WriteUnusedBytesToFile(file, LEVEL_CHUNK_CNT2_SIZE);

    Error(ERR_WARN, "cannot save content for element '%d'", element);
    return;
  }

  putFile16BitBE(file, element);
  putFile8Bit(file, num_contents);
  putFile8Bit(file, content_xsize);
  putFile8Bit(file, content_ysize);

  WriteUnusedBytesToFile(file, LEVEL_CHUNK_CNT2_UNUSED);

  for (i = 0; i < MAX_ELEMENT_CONTENTS; i++)
    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	putFile16BitBE(file, content_array[i][x][y]);
}
#endif

#if 0
static int SaveLevel_CNT3(FILE *file, struct LevelInfo *level, int element)
{
  int envelope_nr = element - EL_ENVELOPE_1;
  int envelope_len = strlen(level->envelope_text[envelope_nr]) + 1;
  int chunk_size = 0;
  int i;

  chunk_size += putFile16BitBE(file, element);
  chunk_size += putFile16BitBE(file, envelope_len);
  chunk_size += putFile8Bit(file, level->envelope_xsize[envelope_nr]);
  chunk_size += putFile8Bit(file, level->envelope_ysize[envelope_nr]);

  WriteUnusedBytesToFile(file, LEVEL_CHUNK_CNT3_UNUSED);
  chunk_size += LEVEL_CHUNK_CNT3_UNUSED;

  for (i = 0; i < envelope_len; i++)
    chunk_size += putFile8Bit(file, level->envelope_text[envelope_nr][i]);

  return chunk_size;
}
#endif

#if 0
static void SaveLevel_CUS1(FILE *file, struct LevelInfo *level,
			   int num_changed_custom_elements)
{
  int i, check = 0;

  putFile16BitBE(file, num_changed_custom_elements);

  for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
  {
    int element = EL_CUSTOM_START + i;

    struct ElementInfo *ei = &element_info[element];

    if (ei->properties[EP_BITFIELD_BASE_NR] != EP_BITMASK_DEFAULT)
    {
      if (check < num_changed_custom_elements)
      {
	putFile16BitBE(file, element);
	putFile32BitBE(file, ei->properties[EP_BITFIELD_BASE_NR]);
      }

      check++;
    }
  }

  if (check != num_changed_custom_elements)	/* should not happen */
    Error(ERR_WARN, "inconsistent number of custom element properties");
}
#endif

#if 0
static void SaveLevel_CUS2(FILE *file, struct LevelInfo *level,
			   int num_changed_custom_elements)
{
  int i, check = 0;

  putFile16BitBE(file, num_changed_custom_elements);

  for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
  {
    int element = EL_CUSTOM_START + i;

    if (element_info[element].change->target_element != EL_EMPTY_SPACE)
    {
      if (check < num_changed_custom_elements)
      {
	putFile16BitBE(file, element);
	putFile16BitBE(file, element_info[element].change->target_element);
      }

      check++;
    }
  }

  if (check != num_changed_custom_elements)	/* should not happen */
    Error(ERR_WARN, "inconsistent number of custom target elements");
}
#endif

#if 0
static void SaveLevel_CUS3(FILE *file, struct LevelInfo *level,
			   int num_changed_custom_elements)
{
  int i, j, x, y, check = 0;

  putFile16BitBE(file, num_changed_custom_elements);

  for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
  {
    int element = EL_CUSTOM_START + i;
    struct ElementInfo *ei = &element_info[element];

    if (ei->modified_settings)
    {
      if (check < num_changed_custom_elements)
      {
	putFile16BitBE(file, element);

	for (j = 0; j < MAX_ELEMENT_NAME_LEN; j++)
	  putFile8Bit(file, ei->description[j]);

	putFile32BitBE(file, ei->properties[EP_BITFIELD_BASE_NR]);

	/* some free bytes for future properties and padding */
	WriteUnusedBytesToFile(file, 7);

	putFile8Bit(file, ei->use_gfx_element);
	putFile16BitBE(file, ei->gfx_element_initial);

	putFile8Bit(file, ei->collect_score_initial);
	putFile8Bit(file, ei->collect_count_initial);

	putFile16BitBE(file, ei->push_delay_fixed);
	putFile16BitBE(file, ei->push_delay_random);
	putFile16BitBE(file, ei->move_delay_fixed);
	putFile16BitBE(file, ei->move_delay_random);

	putFile16BitBE(file, ei->move_pattern);
	putFile8Bit(file, ei->move_direction_initial);
	putFile8Bit(file, ei->move_stepsize);

	for (y = 0; y < 3; y++)
	  for (x = 0; x < 3; x++)
	    putFile16BitBE(file, ei->content.e[x][y]);

	putFile32BitBE(file, ei->change->events);

	putFile16BitBE(file, ei->change->target_element);

	putFile16BitBE(file, ei->change->delay_fixed);
	putFile16BitBE(file, ei->change->delay_random);
	putFile16BitBE(file, ei->change->delay_frames);

	putFile16BitBE(file, ei->change->initial_trigger_element);

	putFile8Bit(file, ei->change->explode);
	putFile8Bit(file, ei->change->use_target_content);
	putFile8Bit(file, ei->change->only_if_complete);
	putFile8Bit(file, ei->change->use_random_replace);

	putFile8Bit(file, ei->change->random_percentage);
	putFile8Bit(file, ei->change->replace_when);

	for (y = 0; y < 3; y++)
	  for (x = 0; x < 3; x++)
	    putFile16BitBE(file, ei->change->content.e[x][y]);

	putFile8Bit(file, ei->slippery_type);

	/* some free bytes for future properties and padding */
	WriteUnusedBytesToFile(file, LEVEL_CPART_CUS3_UNUSED);
      }

      check++;
    }
  }

  if (check != num_changed_custom_elements)	/* should not happen */
    Error(ERR_WARN, "inconsistent number of custom element properties");
}
#endif

#if 0
static void SaveLevel_CUS4(FILE *file, struct LevelInfo *level, int element)
{
  struct ElementInfo *ei = &element_info[element];
  int i, j, x, y;

  /* ---------- custom element base property values (96 bytes) ------------- */

  putFile16BitBE(file, element);

  for (i = 0; i < MAX_ELEMENT_NAME_LEN; i++)
    putFile8Bit(file, ei->description[i]);

  putFile32BitBE(file, ei->properties[EP_BITFIELD_BASE_NR]);

  WriteUnusedBytesToFile(file, 4);	/* reserved for more base properties */

  putFile8Bit(file, ei->num_change_pages);

  putFile16BitBE(file, ei->ce_value_fixed_initial);
  putFile16BitBE(file, ei->ce_value_random_initial);
  putFile8Bit(file, ei->use_last_ce_value);

  putFile8Bit(file, ei->use_gfx_element);
  putFile16BitBE(file, ei->gfx_element_initial);

  putFile8Bit(file, ei->collect_score_initial);
  putFile8Bit(file, ei->collect_count_initial);

  putFile8Bit(file, ei->drop_delay_fixed);
  putFile8Bit(file, ei->push_delay_fixed);
  putFile8Bit(file, ei->drop_delay_random);
  putFile8Bit(file, ei->push_delay_random);
  putFile16BitBE(file, ei->move_delay_fixed);
  putFile16BitBE(file, ei->move_delay_random);

  /* bits 0 - 15 of "move_pattern" ... */
  putFile16BitBE(file, ei->move_pattern & 0xffff);
  putFile8Bit(file, ei->move_direction_initial);
  putFile8Bit(file, ei->move_stepsize);

  putFile8Bit(file, ei->slippery_type);

  for (y = 0; y < 3; y++)
    for (x = 0; x < 3; x++)
      putFile16BitBE(file, ei->content.e[x][y]);

  putFile16BitBE(file, ei->move_enter_element);
  putFile16BitBE(file, ei->move_leave_element);
  putFile8Bit(file, ei->move_leave_type);

  /* ... bits 16 - 31 of "move_pattern" (not nice, but downward compatible) */
  putFile16BitBE(file, (ei->move_pattern >> 16) & 0xffff);

  putFile8Bit(file, ei->access_direction);

  putFile8Bit(file, ei->explosion_delay);
  putFile8Bit(file, ei->ignition_delay);
  putFile8Bit(file, ei->explosion_type);

  /* some free bytes for future custom property values and padding */
  WriteUnusedBytesToFile(file, 1);

  /* ---------- change page property values (48 bytes) --------------------- */

  for (i = 0; i < ei->num_change_pages; i++)
  {
    struct ElementChangeInfo *change = &ei->change_page[i];
    unsigned int event_bits;

    /* bits 0 - 31 of "has_event[]" ... */
    event_bits = 0;
    for (j = 0; j < MIN(NUM_CHANGE_EVENTS, 32); j++)
      if (change->has_event[j])
	event_bits |= (1 << j);
    putFile32BitBE(file, event_bits);

    putFile16BitBE(file, change->target_element);

    putFile16BitBE(file, change->delay_fixed);
    putFile16BitBE(file, change->delay_random);
    putFile16BitBE(file, change->delay_frames);

    putFile16BitBE(file, change->initial_trigger_element);

    putFile8Bit(file, change->explode);
    putFile8Bit(file, change->use_target_content);
    putFile8Bit(file, change->only_if_complete);
    putFile8Bit(file, change->use_random_replace);

    putFile8Bit(file, change->random_percentage);
    putFile8Bit(file, change->replace_when);

    for (y = 0; y < 3; y++)
      for (x = 0; x < 3; x++)
	putFile16BitBE(file, change->target_content.e[x][y]);

    putFile8Bit(file, change->can_change);

    putFile8Bit(file, change->trigger_side);

    putFile8Bit(file, change->trigger_player);
    putFile8Bit(file, (change->trigger_page == CH_PAGE_ANY ? CH_PAGE_ANY_FILE :
		       log_2(change->trigger_page)));

    putFile8Bit(file, change->has_action);
    putFile8Bit(file, change->action_type);
    putFile8Bit(file, change->action_mode);
    putFile16BitBE(file, change->action_arg);

    /* ... bits 32 - 39 of "has_event[]" (not nice, but downward compatible) */
    event_bits = 0;
    for (j = 32; j < NUM_CHANGE_EVENTS; j++)
      if (change->has_event[j])
	event_bits |= (1 << (j - 32));
    putFile8Bit(file, event_bits);
  }
}
#endif

#if 0
static void SaveLevel_GRP1(FILE *file, struct LevelInfo *level, int element)
{
  struct ElementInfo *ei = &element_info[element];
  struct ElementGroupInfo *group = ei->group;
  int i;

  putFile16BitBE(file, element);

  for (i = 0; i < MAX_ELEMENT_NAME_LEN; i++)
    putFile8Bit(file, ei->description[i]);

  putFile8Bit(file, group->num_elements);

  putFile8Bit(file, ei->use_gfx_element);
  putFile16BitBE(file, ei->gfx_element_initial);

  putFile8Bit(file, group->choice_mode);

  /* some free bytes for future values and padding */
  WriteUnusedBytesToFile(file, 3);

  for (i = 0; i < MAX_ELEMENTS_IN_GROUP; i++)
    putFile16BitBE(file, group->element[i]);
}
#endif

static int SaveLevel_MicroChunk(FILE *file, struct LevelFileConfigInfo *entry,
				boolean write_element)
{
  int save_type = entry->save_type;
  int data_type = entry->data_type;
  int conf_type = entry->conf_type;
  int byte_mask = conf_type & CONF_MASK_BYTES;
  int element = entry->element;
  int default_value = entry->default_value;
  int num_bytes = 0;
  boolean modified = FALSE;

  if (byte_mask != CONF_MASK_MULTI_BYTES)
  {
    void *value_ptr = entry->value;
    int value = (data_type == TYPE_BOOLEAN ? *(boolean *)value_ptr :
		 *(int *)value_ptr);

    /* check if any settings have been modified before saving them */
    if (value != default_value)
      modified = TRUE;

    /* do not save if explicitly told or if unmodified default settings */
    if ((save_type == SAVE_CONF_NEVER) ||
	(save_type == SAVE_CONF_WHEN_CHANGED && !modified))
      return 0;

    if (write_element)
      num_bytes += putFile16BitBE(file, element);

    num_bytes += putFile8Bit(file, conf_type);
    num_bytes += (byte_mask == CONF_MASK_1_BYTE ? putFile8Bit   (file, value) :
		  byte_mask == CONF_MASK_2_BYTE ? putFile16BitBE(file, value) :
		  byte_mask == CONF_MASK_4_BYTE ? putFile32BitBE(file, value) :
		  0);
  }
  else if (data_type == TYPE_STRING)
  {
    char *default_string = entry->default_string;
    char *string = (char *)(entry->value);
    int string_length = strlen(string);
    int i;

    /* check if any settings have been modified before saving them */
    if (!strEqual(string, default_string))
      modified = TRUE;

    /* do not save if explicitly told or if unmodified default settings */
    if ((save_type == SAVE_CONF_NEVER) ||
	(save_type == SAVE_CONF_WHEN_CHANGED && !modified))
      return 0;

    if (write_element)
      num_bytes += putFile16BitBE(file, element);

    num_bytes += putFile8Bit(file, conf_type);
    num_bytes += putFile16BitBE(file, string_length);

    for (i = 0; i < string_length; i++)
      num_bytes += putFile8Bit(file, string[i]);
  }
  else if (data_type == TYPE_ELEMENT_LIST)
  {
    int *element_array = (int *)(entry->value);
    int num_elements = *(int *)(entry->num_entities);
    int i;

    /* check if any settings have been modified before saving them */
    for (i = 0; i < num_elements; i++)
      if (element_array[i] != default_value)
	modified = TRUE;

    /* do not save if explicitly told or if unmodified default settings */
    if ((save_type == SAVE_CONF_NEVER) ||
	(save_type == SAVE_CONF_WHEN_CHANGED && !modified))
      return 0;

    if (write_element)
      num_bytes += putFile16BitBE(file, element);

    num_bytes += putFile8Bit(file, conf_type);
    num_bytes += putFile16BitBE(file, num_elements * CONF_ELEMENT_NUM_BYTES);

    for (i = 0; i < num_elements; i++)
      num_bytes += putFile16BitBE(file, element_array[i]);
  }
  else if (data_type == TYPE_CONTENT_LIST)
  {
    struct Content *content = (struct Content *)(entry->value);
    int num_contents = *(int *)(entry->num_entities);
    int i, x, y;

    /* check if any settings have been modified before saving them */
    for (i = 0; i < num_contents; i++)
      for (y = 0; y < 3; y++)
	for (x = 0; x < 3; x++)
	  if (content[i].e[x][y] != default_value)
	    modified = TRUE;

    /* do not save if explicitly told or if unmodified default settings */
    if ((save_type == SAVE_CONF_NEVER) ||
	(save_type == SAVE_CONF_WHEN_CHANGED && !modified))
      return 0;

    if (write_element)
      num_bytes += putFile16BitBE(file, element);

    num_bytes += putFile8Bit(file, conf_type);
    num_bytes += putFile16BitBE(file, num_contents * CONF_CONTENT_NUM_BYTES);

    for (i = 0; i < num_contents; i++)
      for (y = 0; y < 3; y++)
	for (x = 0; x < 3; x++)
	  num_bytes += putFile16BitBE(file, content[i].e[x][y]);
  }

  return num_bytes;
}

static int SaveLevel_INFO(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;
  int i;

  li = *level;		/* copy level data into temporary buffer */

  for (i = 0; chunk_config_INFO[i].data_type != -1; i++)
    chunk_size += SaveLevel_MicroChunk(file, &chunk_config_INFO[i], FALSE);

  return chunk_size;
}

static int SaveLevel_ELEM(FILE *file, struct LevelInfo *level)
{
  int chunk_size = 0;
  int i;

  li = *level;		/* copy level data into temporary buffer */

  for (i = 0; chunk_config_ELEM[i].data_type != -1; i++)
    chunk_size += SaveLevel_MicroChunk(file, &chunk_config_ELEM[i], TRUE);

  return chunk_size;
}

static int SaveLevel_NOTE(FILE *file, struct LevelInfo *level, int element)
{
  int envelope_nr = element - EL_ENVELOPE_1;
  int chunk_size = 0;
  int i;

  chunk_size += putFile16BitBE(file, element);

  /* copy envelope data into temporary buffer */
  xx_envelope = level->envelope[envelope_nr];

  for (i = 0; chunk_config_NOTE[i].data_type != -1; i++)
    chunk_size += SaveLevel_MicroChunk(file, &chunk_config_NOTE[i], FALSE);

  return chunk_size;
}

static int SaveLevel_CUSX(FILE *file, struct LevelInfo *level, int element)
{
  struct ElementInfo *ei = &element_info[element];
  int chunk_size = 0;
  int i, j;

  chunk_size += putFile16BitBE(file, element);

  xx_ei = *ei;		/* copy element data into temporary buffer */

  /* set default description string for this specific element */
  strcpy(xx_default_description, getDefaultElementDescription(ei));

#if 0
  /* set (fixed) number of content areas (may be wrong by broken level file) */
  /* (this is now directly corrected for broken level files after loading) */
  xx_num_contents = 1;
#endif

  for (i = 0; chunk_config_CUSX_base[i].data_type != -1; i++)
    chunk_size += SaveLevel_MicroChunk(file, &chunk_config_CUSX_base[i], FALSE);

  for (i = 0; i < ei->num_change_pages; i++)
  {
    struct ElementChangeInfo *change = &ei->change_page[i];

    xx_current_change_page = i;

    xx_change = *change;	/* copy change data into temporary buffer */

    resetEventBits();
    setEventBitsFromEventFlags(change);

    for (j = 0; chunk_config_CUSX_change[j].data_type != -1; j++)
      chunk_size += SaveLevel_MicroChunk(file, &chunk_config_CUSX_change[j],
					 FALSE);
  }

  return chunk_size;
}

static int SaveLevel_GRPX(FILE *file, struct LevelInfo *level, int element)
{
  struct ElementInfo *ei = &element_info[element];
  struct ElementGroupInfo *group = ei->group;
  int chunk_size = 0;
  int i;

  chunk_size += putFile16BitBE(file, element);

  xx_ei = *ei;		/* copy element data into temporary buffer */
  xx_group = *group;	/* copy group data into temporary buffer */

  /* set default description string for this specific element */
  strcpy(xx_default_description, getDefaultElementDescription(ei));

  for (i = 0; chunk_config_GRPX[i].data_type != -1; i++)
    chunk_size += SaveLevel_MicroChunk(file, &chunk_config_GRPX[i], FALSE);

  return chunk_size;
}

static void SaveLevelFromFilename(struct LevelInfo *level, char *filename)
{
  int chunk_size;
  int i;
  FILE *file;

  if (!(file = fopen(filename, MODE_WRITE)))
  {
    Error(ERR_WARN, "cannot save level file '%s'", filename);
    return;
  }

  level->file_version = FILE_VERSION_ACTUAL;
  level->game_version = GAME_VERSION_ACTUAL;

  level->creation_date = getCurrentDate();

  putFileChunkBE(file, "RND1", CHUNK_SIZE_UNDEFINED);
  putFileChunkBE(file, "CAVE", CHUNK_SIZE_NONE);

  chunk_size = SaveLevel_VERS(NULL, level);
  putFileChunkBE(file, "VERS", chunk_size);
  SaveLevel_VERS(file, level);

  chunk_size = SaveLevel_DATE(NULL, level);
  putFileChunkBE(file, "DATE", chunk_size);
  SaveLevel_DATE(file, level);

  chunk_size = SaveLevel_NAME(NULL, level);
  putFileChunkBE(file, "NAME", chunk_size);
  SaveLevel_NAME(file, level);

  chunk_size = SaveLevel_AUTH(NULL, level);
  putFileChunkBE(file, "AUTH", chunk_size);
  SaveLevel_AUTH(file, level);

  chunk_size = SaveLevel_INFO(NULL, level);
  putFileChunkBE(file, "INFO", chunk_size);
  SaveLevel_INFO(file, level);

  chunk_size = SaveLevel_BODY(NULL, level);
  putFileChunkBE(file, "BODY", chunk_size);
  SaveLevel_BODY(file, level);

  chunk_size = SaveLevel_ELEM(NULL, level);
  if (chunk_size > LEVEL_CHUNK_ELEM_UNCHANGED)		/* save if changed */
  {
    putFileChunkBE(file, "ELEM", chunk_size);
    SaveLevel_ELEM(file, level);
  }

  for (i = 0; i < NUM_ENVELOPES; i++)
  {
    int element = EL_ENVELOPE_1 + i;

    chunk_size = SaveLevel_NOTE(NULL, level, element);
    if (chunk_size > LEVEL_CHUNK_NOTE_UNCHANGED)	/* save if changed */
    {
      putFileChunkBE(file, "NOTE", chunk_size);
      SaveLevel_NOTE(file, level, element);
    }
  }

  /* if not using template level, check for non-default custom/group elements */
  if (!level->use_custom_template)
  {
    for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
    {
      int element = EL_CUSTOM_START + i;

      chunk_size = SaveLevel_CUSX(NULL, level, element);
      if (chunk_size > LEVEL_CHUNK_CUSX_UNCHANGED)	/* save if changed */
      {
	putFileChunkBE(file, "CUSX", chunk_size);
	SaveLevel_CUSX(file, level, element);
      }
    }

    for (i = 0; i < NUM_GROUP_ELEMENTS; i++)
    {
      int element = EL_GROUP_START + i;

      chunk_size = SaveLevel_GRPX(NULL, level, element);
      if (chunk_size > LEVEL_CHUNK_GRPX_UNCHANGED)	/* save if changed */
      {
	putFileChunkBE(file, "GRPX", chunk_size);
	SaveLevel_GRPX(file, level, element);
      }
    }
  }

  fclose(file);

  SetFilePermissions(filename, PERMS_PRIVATE);
}

void SaveLevel(int nr)
{
  char *filename = getDefaultLevelFilename(nr);

  SaveLevelFromFilename(&level, filename);
}

void SaveLevelTemplate()
{
  char *filename = getDefaultLevelFilename(-1);

  SaveLevelFromFilename(&level, filename);
}

boolean SaveLevelChecked(int nr)
{
  char *filename = getDefaultLevelFilename(nr);
  boolean new_level = !fileExists(filename);
  boolean level_saved = FALSE;

  if (new_level || Request("Save this level and kill the old ?", REQ_ASK))
  {
    SaveLevel(nr);

    if (new_level)
      Request("Level saved !", REQ_CONFIRM);

    level_saved = TRUE;
  }

  return level_saved;
}

void DumpLevel(struct LevelInfo *level)
{
  if (level->no_valid_file)
  {
    Error(ERR_WARN, "cannot dump -- no valid level file found");

    return;
  }

  printf_line("-", 79);
  printf("Level xxx (file version %08d, game version %08d)\n",
	 level->file_version, level->game_version);
  printf_line("-", 79);

  printf("Level author: '%s'\n", level->author);
  printf("Level title:  '%s'\n", level->name);
  printf("\n");
  printf("Playfield size: %d x %d\n", level->fieldx, level->fieldy);
  printf("\n");
  printf("Level time:  %d seconds\n", level->time);
  printf("Gems needed: %d\n", level->gems_needed);
  printf("\n");
  printf("Time for magic wall: %d seconds\n", level->time_magic_wall);
  printf("Time for wheel:      %d seconds\n", level->time_wheel);
  printf("Time for light:      %d seconds\n", level->time_light);
  printf("Time for timegate:   %d seconds\n", level->time_timegate);
  printf("\n");
  printf("Amoeba speed: %d\n", level->amoeba_speed);
  printf("\n");

  printf("EM style slippery gems:      %s\n", (level->em_slippery_gems ? "yes" : "no"));
  printf("Player blocks last field:    %s\n", (level->block_last_field ? "yes" : "no"));
  printf("SP player blocks last field: %s\n", (level->sp_block_last_field ? "yes" : "no"));
  printf("use spring bug: %s\n", (level->use_spring_bug ? "yes" : "no"));
  printf("use step counter: %s\n", (level->use_step_counter ? "yes" : "no"));

  printf_line("-", 79);
}


/* ========================================================================= */
/* tape file functions                                                       */
/* ========================================================================= */

static void setTapeInfoToDefaults()
{
  int i;

  /* always start with reliable default values (empty tape) */
  TapeErase();

  /* default values (also for pre-1.2 tapes) with only the first player */
  tape.player_participates[0] = TRUE;
  for (i = 1; i < MAX_PLAYERS; i++)
    tape.player_participates[i] = FALSE;

  /* at least one (default: the first) player participates in every tape */
  tape.num_participating_players = 1;

  tape.level_nr = level_nr;
  tape.counter = 0;
  tape.changed = FALSE;

  tape.recording = FALSE;
  tape.playing = FALSE;
  tape.pausing = FALSE;

  tape.no_valid_file = FALSE;
}

static int LoadTape_VERS(FILE *file, int chunk_size, struct TapeInfo *tape)
{
  tape->file_version = getFileVersion(file);
  tape->game_version = getFileVersion(file);

  return chunk_size;
}

static int LoadTape_HEAD(FILE *file, int chunk_size, struct TapeInfo *tape)
{
  int i;

  tape->random_seed = getFile32BitBE(file);
  tape->date        = getFile32BitBE(file);
  tape->length      = getFile32BitBE(file);

  /* read header fields that are new since version 1.2 */
  if (tape->file_version >= FILE_VERSION_1_2)
  {
    byte store_participating_players = getFile8Bit(file);
    int engine_version;

    /* since version 1.2, tapes store which players participate in the tape */
    tape->num_participating_players = 0;
    for (i = 0; i < MAX_PLAYERS; i++)
    {
      tape->player_participates[i] = FALSE;

      if (store_participating_players & (1 << i))
      {
	tape->player_participates[i] = TRUE;
	tape->num_participating_players++;
      }
    }

    ReadUnusedBytesFromFile(file, TAPE_CHUNK_HEAD_UNUSED);

    engine_version = getFileVersion(file);
    if (engine_version > 0)
      tape->engine_version = engine_version;
    else
      tape->engine_version = tape->game_version;
  }

  return chunk_size;
}

static int LoadTape_INFO(FILE *file, int chunk_size, struct TapeInfo *tape)
{
  int level_identifier_size;
  int i;

  level_identifier_size = getFile16BitBE(file);

  tape->level_identifier =
    checked_realloc(tape->level_identifier, level_identifier_size);

  for (i = 0; i < level_identifier_size; i++)
    tape->level_identifier[i] = getFile8Bit(file);

  tape->level_nr = getFile16BitBE(file);

  chunk_size = 2 + level_identifier_size + 2;

  return chunk_size;
}

static int LoadTape_BODY(FILE *file, int chunk_size, struct TapeInfo *tape)
{
  int i, j;
  int chunk_size_expected =
    (tape->num_participating_players + 1) * tape->length;

  if (chunk_size_expected != chunk_size)
  {
    ReadUnusedBytesFromFile(file, chunk_size);
    return chunk_size_expected;
  }

  for (i = 0; i < tape->length; i++)
  {
    if (i >= MAX_TAPE_LEN)
      break;

    for (j = 0; j < MAX_PLAYERS; j++)
    {
      tape->pos[i].action[j] = MV_NONE;

      if (tape->player_participates[j])
	tape->pos[i].action[j] = getFile8Bit(file);
    }

    tape->pos[i].delay = getFile8Bit(file);

    if (tape->file_version == FILE_VERSION_1_0)
    {
      /* eliminate possible diagonal moves in old tapes */
      /* this is only for backward compatibility */

      byte joy_dir[4] = { JOY_LEFT, JOY_RIGHT, JOY_UP, JOY_DOWN };
      byte action = tape->pos[i].action[0];
      int k, num_moves = 0;

      for (k = 0; k<4; k++)
      {
	if (action & joy_dir[k])
	{
	  tape->pos[i + num_moves].action[0] = joy_dir[k];
	  if (num_moves > 0)
	    tape->pos[i + num_moves].delay = 0;
	  num_moves++;
	}
      }

      if (num_moves > 1)
      {
	num_moves--;
	i += num_moves;
	tape->length += num_moves;
      }
    }
    else if (tape->file_version < FILE_VERSION_2_0)
    {
      /* convert pre-2.0 tapes to new tape format */

      if (tape->pos[i].delay > 1)
      {
	/* action part */
	tape->pos[i + 1] = tape->pos[i];
	tape->pos[i + 1].delay = 1;

	/* delay part */
	for (j = 0; j < MAX_PLAYERS; j++)
	  tape->pos[i].action[j] = MV_NONE;
	tape->pos[i].delay--;

	i++;
	tape->length++;
      }
    }

    if (feof(file))
      break;
  }

  if (i != tape->length)
    chunk_size = (tape->num_participating_players + 1) * i;

  return chunk_size;
}

void LoadTape_SokobanSolution(char *filename)
{
  FILE *file;
  int move_delay = TILESIZE / level.initial_player_stepsize[0];

  if (!(file = fopen(filename, MODE_READ)))
  {
    tape.no_valid_file = TRUE;

    return;
  }

  while (!feof(file))
  {
    unsigned char c = fgetc(file);

    if (feof(file))
      break;

    switch (c)
    {
      case 'u':
      case 'U':
	tape.pos[tape.length].action[0] = MV_UP;
	tape.pos[tape.length].delay = move_delay + (c < 'a' ? 2 : 0);
	tape.length++;
	break;

      case 'd':
      case 'D':
	tape.pos[tape.length].action[0] = MV_DOWN;
	tape.pos[tape.length].delay = move_delay + (c < 'a' ? 2 : 0);
	tape.length++;
	break;

      case 'l':
      case 'L':
	tape.pos[tape.length].action[0] = MV_LEFT;
	tape.pos[tape.length].delay = move_delay + (c < 'a' ? 2 : 0);
	tape.length++;
	break;

      case 'r':
      case 'R':
	tape.pos[tape.length].action[0] = MV_RIGHT;
	tape.pos[tape.length].delay = move_delay + (c < 'a' ? 2 : 0);
	tape.length++;
	break;

      case '\n':
      case '\r':
      case '\t':
      case ' ':
	/* ignore white-space characters */
	break;

      default:
	tape.no_valid_file = TRUE;

	Error(ERR_WARN, "unsupported Sokoban solution file '%s' ['%d']", filename, c);

	break;
    }
  }

  fclose(file);

  if (tape.no_valid_file)
    return;

  tape.length_seconds = GetTapeLength();
}

void LoadTapeFromFilename(char *filename)
{
  char cookie[MAX_LINE_LEN];
  char chunk_name[CHUNK_ID_LEN + 1];
  FILE *file;
  int chunk_size;

  /* always start with reliable default values */
  setTapeInfoToDefaults();

  if (strSuffix(filename, ".sln"))
  {
    LoadTape_SokobanSolution(filename);

    return;
  }

  if (!(file = fopen(filename, MODE_READ)))
  {
    tape.no_valid_file = TRUE;

    return;
  }

  getFileChunkBE(file, chunk_name, NULL);
  if (strEqual(chunk_name, "RND1"))
  {
    getFile32BitBE(file);		/* not used */

    getFileChunkBE(file, chunk_name, NULL);
    if (!strEqual(chunk_name, "TAPE"))
    {
      tape.no_valid_file = TRUE;

      Error(ERR_WARN, "unknown format of tape file '%s'", filename);
      fclose(file);
      return;
    }
  }
  else	/* check for pre-2.0 file format with cookie string */
  {
    strcpy(cookie, chunk_name);
    fgets(&cookie[4], MAX_LINE_LEN - 4, file);
    if (strlen(cookie) > 0 && cookie[strlen(cookie) - 1] == '\n')
      cookie[strlen(cookie) - 1] = '\0';

    if (!checkCookieString(cookie, TAPE_COOKIE_TMPL))
    {
      tape.no_valid_file = TRUE;

      Error(ERR_WARN, "unknown format of tape file '%s'", filename);
      fclose(file);
      return;
    }

    if ((tape.file_version = getFileVersionFromCookieString(cookie)) == -1)
    {
      tape.no_valid_file = TRUE;

      Error(ERR_WARN, "unsupported version of tape file '%s'", filename);
      fclose(file);

      return;
    }

    /* pre-2.0 tape files have no game version, so use file version here */
    tape.game_version = tape.file_version;
  }

  if (tape.file_version < FILE_VERSION_1_2)
  {
    /* tape files from versions before 1.2.0 without chunk structure */
    LoadTape_HEAD(file, TAPE_CHUNK_HEAD_SIZE, &tape);
    LoadTape_BODY(file, 2 * tape.length,      &tape);
  }
  else
  {
    static struct
    {
      char *name;
      int size;
      int (*loader)(FILE *, int, struct TapeInfo *);
    }
    chunk_info[] =
    {
      { "VERS", TAPE_CHUNK_VERS_SIZE,	LoadTape_VERS },
      { "HEAD", TAPE_CHUNK_HEAD_SIZE,	LoadTape_HEAD },
      { "INFO", -1,			LoadTape_INFO },
      { "BODY", -1,			LoadTape_BODY },
      {  NULL,  0,			NULL }
    };

    while (getFileChunkBE(file, chunk_name, &chunk_size))
    {
      int i = 0;

      while (chunk_info[i].name != NULL &&
	     !strEqual(chunk_name, chunk_info[i].name))
	i++;

      if (chunk_info[i].name == NULL)
      {
	Error(ERR_WARN, "unknown chunk '%s' in tape file '%s'",
	      chunk_name, filename);
	ReadUnusedBytesFromFile(file, chunk_size);
      }
      else if (chunk_info[i].size != -1 &&
	       chunk_info[i].size != chunk_size)
      {
	Error(ERR_WARN, "wrong size (%d) of chunk '%s' in tape file '%s'",
	      chunk_size, chunk_name, filename);
	ReadUnusedBytesFromFile(file, chunk_size);
      }
      else
      {
	/* call function to load this tape chunk */
	int chunk_size_expected =
	  (chunk_info[i].loader)(file, chunk_size, &tape);

	/* the size of some chunks cannot be checked before reading other
	   chunks first (like "HEAD" and "BODY") that contain some header
	   information, so check them here */
	if (chunk_size_expected != chunk_size)
	{
	  Error(ERR_WARN, "wrong size (%d) of chunk '%s' in tape file '%s'",
		chunk_size, chunk_name, filename);
	}
      }
    }
  }

  fclose(file);

  tape.length_seconds = GetTapeLength();

#if 0
  printf("::: tape file version: %d\n", tape.file_version);
  printf("::: tape game version: %d\n", tape.game_version);
  printf("::: tape engine version: %d\n", tape.engine_version);
#endif
}

void LoadTape(int nr)
{
  char *filename = getTapeFilename(nr);

  LoadTapeFromFilename(filename);
}

void LoadSolutionTape(int nr)
{
  char *filename = getSolutionTapeFilename(nr);

  LoadTapeFromFilename(filename);

#if 1
  if (TAPE_IS_EMPTY(tape) &&
      level.game_engine_type == GAME_ENGINE_TYPE_SP &&
      level.native_sp_level->demo.is_available)
    CopyNativeTape_SP_to_RND(&level);
#endif
}

static void SaveTape_VERS(FILE *file, struct TapeInfo *tape)
{
  putFileVersion(file, tape->file_version);
  putFileVersion(file, tape->game_version);
}

static void SaveTape_HEAD(FILE *file, struct TapeInfo *tape)
{
  int i;
  byte store_participating_players = 0;

  /* set bits for participating players for compact storage */
  for (i = 0; i < MAX_PLAYERS; i++)
    if (tape->player_participates[i])
      store_participating_players |= (1 << i);

  putFile32BitBE(file, tape->random_seed);
  putFile32BitBE(file, tape->date);
  putFile32BitBE(file, tape->length);

  putFile8Bit(file, store_participating_players);

  /* unused bytes not at the end here for 4-byte alignment of engine_version */
  WriteUnusedBytesToFile(file, TAPE_CHUNK_HEAD_UNUSED);

  putFileVersion(file, tape->engine_version);
}

static void SaveTape_INFO(FILE *file, struct TapeInfo *tape)
{
  int level_identifier_size = strlen(tape->level_identifier) + 1;
  int i;

  putFile16BitBE(file, level_identifier_size);

  for (i = 0; i < level_identifier_size; i++)
    putFile8Bit(file, tape->level_identifier[i]);

  putFile16BitBE(file, tape->level_nr);
}

static void SaveTape_BODY(FILE *file, struct TapeInfo *tape)
{
  int i, j;

  for (i = 0; i < tape->length; i++)
  {
    for (j = 0; j < MAX_PLAYERS; j++)
      if (tape->player_participates[j])
	putFile8Bit(file, tape->pos[i].action[j]);

    putFile8Bit(file, tape->pos[i].delay);
  }
}

void SaveTape(int nr)
{
  char *filename = getTapeFilename(nr);
  FILE *file;
#if 0
  boolean new_tape = TRUE;
#endif
  int num_participating_players = 0;
  int info_chunk_size;
  int body_chunk_size;
  int i;

  InitTapeDirectory(leveldir_current->subdir);

#if 0
  /* if a tape still exists, ask to overwrite it */
  if (fileExists(filename))
  {
    new_tape = FALSE;
    if (!Request("Replace old tape ?", REQ_ASK))
      return;
  }
#endif

  if (!(file = fopen(filename, MODE_WRITE)))
  {
    Error(ERR_WARN, "cannot save level recording file '%s'", filename);
    return;
  }

  tape.file_version = FILE_VERSION_ACTUAL;
  tape.game_version = GAME_VERSION_ACTUAL;

  /* count number of participating players  */
  for (i = 0; i < MAX_PLAYERS; i++)
    if (tape.player_participates[i])
      num_participating_players++;

  info_chunk_size = 2 + (strlen(tape.level_identifier) + 1) + 2;
  body_chunk_size = (num_participating_players + 1) * tape.length;

  putFileChunkBE(file, "RND1", CHUNK_SIZE_UNDEFINED);
  putFileChunkBE(file, "TAPE", CHUNK_SIZE_NONE);

  putFileChunkBE(file, "VERS", TAPE_CHUNK_VERS_SIZE);
  SaveTape_VERS(file, &tape);

  putFileChunkBE(file, "HEAD", TAPE_CHUNK_HEAD_SIZE);
  SaveTape_HEAD(file, &tape);

  putFileChunkBE(file, "INFO", info_chunk_size);
  SaveTape_INFO(file, &tape);

  putFileChunkBE(file, "BODY", body_chunk_size);
  SaveTape_BODY(file, &tape);

  fclose(file);

  SetFilePermissions(filename, PERMS_PRIVATE);

  tape.changed = FALSE;

#if 0
  if (new_tape)
    Request("Tape saved !", REQ_CONFIRM);
#endif
}

boolean SaveTapeChecked(int nr)
{
  char *filename = getTapeFilename(nr);
  boolean new_tape = !fileExists(filename);
  boolean tape_saved = FALSE;

  if (new_tape || Request("Replace old tape ?", REQ_ASK))
  {
    SaveTape(nr);

    if (new_tape)
      Request("Tape saved !", REQ_CONFIRM);

    tape_saved = TRUE;
  }

  return tape_saved;
}

void DumpTape(struct TapeInfo *tape)
{
  int tape_frame_counter;
  int i, j;

  if (tape->no_valid_file)
  {
    Error(ERR_WARN, "cannot dump -- no valid tape file found");

    return;
  }

  printf_line("-", 79);
  printf("Tape of Level %03d (file version %08d, game version %08d)\n",
	 tape->level_nr, tape->file_version, tape->game_version);
  printf("                  (effective engine version %08d)\n",
	 tape->engine_version);
  printf("Level series identifier: '%s'\n", tape->level_identifier);
  printf_line("-", 79);

  tape_frame_counter = 0;

  for (i = 0; i < tape->length; i++)
  {
    if (i >= MAX_TAPE_LEN)
      break;

    printf("%04d: ", i);

    for (j = 0; j < MAX_PLAYERS; j++)
    {
      if (tape->player_participates[j])
      {
	int action = tape->pos[i].action[j];

	printf("%d:%02x ", j, action);
	printf("[%c%c%c%c|%c%c] - ",
	       (action & JOY_LEFT ? '<' : ' '),
	       (action & JOY_RIGHT ? '>' : ' '),
	       (action & JOY_UP ? '^' : ' '),
	       (action & JOY_DOWN ? 'v' : ' '),
	       (action & JOY_BUTTON_1 ? '1' : ' '),
	       (action & JOY_BUTTON_2 ? '2' : ' '));
      }
    }

    printf("(%03d) ", tape->pos[i].delay);
    printf("[%05d]\n", tape_frame_counter);

    tape_frame_counter += tape->pos[i].delay;
  }

  printf_line("-", 79);
}


/* ========================================================================= */
/* score file functions                                                      */
/* ========================================================================= */

void LoadScore(int nr)
{
  int i;
  char *filename = getScoreFilename(nr);
  char cookie[MAX_LINE_LEN];
  char line[MAX_LINE_LEN];
  char *line_ptr;
  FILE *file;

  /* always start with reliable default values */
  for (i = 0; i < MAX_SCORE_ENTRIES; i++)
  {
    strcpy(highscore[i].Name, EMPTY_PLAYER_NAME);
    highscore[i].Score = 0;
  }

  if (!(file = fopen(filename, MODE_READ)))
    return;

  /* check file identifier */
  fgets(cookie, MAX_LINE_LEN, file);
  if (strlen(cookie) > 0 && cookie[strlen(cookie) - 1] == '\n')
    cookie[strlen(cookie) - 1] = '\0';

  if (!checkCookieString(cookie, SCORE_COOKIE))
  {
    Error(ERR_WARN, "unknown format of score file '%s'", filename);
    fclose(file);
    return;
  }

  for (i = 0; i < MAX_SCORE_ENTRIES; i++)
  {
    fscanf(file, "%d", &highscore[i].Score);
    fgets(line, MAX_LINE_LEN, file);

    if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

    for (line_ptr = line; *line_ptr; line_ptr++)
    {
      if (*line_ptr != ' ' && *line_ptr != '\t' && *line_ptr != '\0')
      {
	strncpy(highscore[i].Name, line_ptr, MAX_PLAYER_NAME_LEN);
	highscore[i].Name[MAX_PLAYER_NAME_LEN] = '\0';
	break;
      }
    }
  }

  fclose(file);
}

void SaveScore(int nr)
{
  int i;
  char *filename = getScoreFilename(nr);
  FILE *file;

  InitScoreDirectory(leveldir_current->subdir);

  if (!(file = fopen(filename, MODE_WRITE)))
  {
    Error(ERR_WARN, "cannot save score for level %d", nr);
    return;
  }

  fprintf(file, "%s\n\n", SCORE_COOKIE);

  for (i = 0; i < MAX_SCORE_ENTRIES; i++)
    fprintf(file, "%d %s\n", highscore[i].Score, highscore[i].Name);

  fclose(file);

  SetFilePermissions(filename, PERMS_PUBLIC);
}


/* ========================================================================= */
/* setup file functions                                                      */
/* ========================================================================= */

#define TOKEN_STR_PLAYER_PREFIX			"player_"

/* global setup */
#define SETUP_TOKEN_PLAYER_NAME			0
#define SETUP_TOKEN_SOUND			1
#define SETUP_TOKEN_SOUND_LOOPS			2
#define SETUP_TOKEN_SOUND_MUSIC			3
#define SETUP_TOKEN_SOUND_SIMPLE		4
#define SETUP_TOKEN_TOONS			5
#define SETUP_TOKEN_SCROLL_DELAY		6
#define SETUP_TOKEN_SCROLL_DELAY_VALUE		7
#define SETUP_TOKEN_SOFT_SCROLLING		8
#define SETUP_TOKEN_FADE_SCREENS		9
#define SETUP_TOKEN_AUTORECORD			10
#define SETUP_TOKEN_SHOW_TITLESCREEN		11
#define SETUP_TOKEN_QUICK_DOORS			12
#define SETUP_TOKEN_TEAM_MODE			13
#define SETUP_TOKEN_HANDICAP			14
#define SETUP_TOKEN_SKIP_LEVELS			15
#define SETUP_TOKEN_TIME_LIMIT			16
#define SETUP_TOKEN_FULLSCREEN			17
#define SETUP_TOKEN_FULLSCREEN_MODE		18
#define SETUP_TOKEN_ASK_ON_ESCAPE		19
#define SETUP_TOKEN_ASK_ON_ESCAPE_EDITOR	20
#define SETUP_TOKEN_QUICK_SWITCH		21
#define SETUP_TOKEN_INPUT_ON_FOCUS		22
#define SETUP_TOKEN_PREFER_AGA_GRAPHICS		23
#define SETUP_TOKEN_GAME_FRAME_DELAY		24
#define SETUP_TOKEN_SP_SHOW_BORDER_ELEMENTS	25
#define SETUP_TOKEN_GRAPHICS_SET		26
#define SETUP_TOKEN_SOUNDS_SET			27
#define SETUP_TOKEN_MUSIC_SET			28
#define SETUP_TOKEN_OVERRIDE_LEVEL_GRAPHICS	29
#define SETUP_TOKEN_OVERRIDE_LEVEL_SOUNDS	30
#define SETUP_TOKEN_OVERRIDE_LEVEL_MUSIC	31

#define NUM_GLOBAL_SETUP_TOKENS			32

/* editor setup */
#define SETUP_TOKEN_EDITOR_EL_BOULDERDASH	0
#define SETUP_TOKEN_EDITOR_EL_EMERALD_MINE	1
#define SETUP_TOKEN_EDITOR_EL_EMERALD_MINE_CLUB	2
#define SETUP_TOKEN_EDITOR_EL_MORE		3
#define SETUP_TOKEN_EDITOR_EL_SOKOBAN		4
#define SETUP_TOKEN_EDITOR_EL_SUPAPLEX		5
#define SETUP_TOKEN_EDITOR_EL_DIAMOND_CAVES	6
#define SETUP_TOKEN_EDITOR_EL_DX_BOULDERDASH	7
#define SETUP_TOKEN_EDITOR_EL_CHARS		8
#define SETUP_TOKEN_EDITOR_EL_STEEL_CHARS	9
#define SETUP_TOKEN_EDITOR_EL_CUSTOM		10
#define SETUP_TOKEN_EDITOR_EL_HEADLINES		11
#define SETUP_TOKEN_EDITOR_EL_USER_DEFINED	12
#define SETUP_TOKEN_EDITOR_EL_DYNAMIC		13
#define SETUP_TOKEN_EDITOR_EL_BY_GAME		14
#define SETUP_TOKEN_EDITOR_EL_BY_TYPE		15
#define SETUP_TOKEN_EDITOR_SHOW_ELEMENT_TOKEN	16

#define NUM_EDITOR_SETUP_TOKENS			17

/* editor cascade setup */
#define SETUP_TOKEN_EDITOR_CASCADE_BD		0
#define SETUP_TOKEN_EDITOR_CASCADE_EM		1
#define SETUP_TOKEN_EDITOR_CASCADE_EMC		2
#define SETUP_TOKEN_EDITOR_CASCADE_RND		3
#define SETUP_TOKEN_EDITOR_CASCADE_SB		4
#define SETUP_TOKEN_EDITOR_CASCADE_SP		5
#define SETUP_TOKEN_EDITOR_CASCADE_DC		6
#define SETUP_TOKEN_EDITOR_CASCADE_DX		7
#define SETUP_TOKEN_EDITOR_CASCADE_TEXT		8
#define SETUP_TOKEN_EDITOR_CASCADE_STEELTEXT	9
#define SETUP_TOKEN_EDITOR_CASCADE_CE		10
#define SETUP_TOKEN_EDITOR_CASCADE_GE		11
#define SETUP_TOKEN_EDITOR_CASCADE_REF		12
#define SETUP_TOKEN_EDITOR_CASCADE_USER		13
#define SETUP_TOKEN_EDITOR_CASCADE_DYNAMIC	14

#define NUM_EDITOR_CASCADE_SETUP_TOKENS		15

/* shortcut setup */
#define SETUP_TOKEN_SHORTCUT_SAVE_GAME		0
#define SETUP_TOKEN_SHORTCUT_LOAD_GAME		1
#define SETUP_TOKEN_SHORTCUT_TOGGLE_PAUSE	2
#define SETUP_TOKEN_SHORTCUT_FOCUS_PLAYER_1	3
#define SETUP_TOKEN_SHORTCUT_FOCUS_PLAYER_2	4
#define SETUP_TOKEN_SHORTCUT_FOCUS_PLAYER_3	5
#define SETUP_TOKEN_SHORTCUT_FOCUS_PLAYER_4	6
#define SETUP_TOKEN_SHORTCUT_FOCUS_PLAYER_ALL	7
#define SETUP_TOKEN_SHORTCUT_TAPE_EJECT		8
#define SETUP_TOKEN_SHORTCUT_TAPE_STOP		9
#define SETUP_TOKEN_SHORTCUT_TAPE_PAUSE		10
#define SETUP_TOKEN_SHORTCUT_TAPE_RECORD	11
#define SETUP_TOKEN_SHORTCUT_TAPE_PLAY		12
#define SETUP_TOKEN_SHORTCUT_SOUND_SIMPLE	13
#define SETUP_TOKEN_SHORTCUT_SOUND_LOOPS	14
#define SETUP_TOKEN_SHORTCUT_SOUND_MUSIC	15
#define SETUP_TOKEN_SHORTCUT_SNAP_LEFT		16
#define SETUP_TOKEN_SHORTCUT_SNAP_RIGHT		17
#define SETUP_TOKEN_SHORTCUT_SNAP_UP		18
#define SETUP_TOKEN_SHORTCUT_SNAP_DOWN		19

#define NUM_SHORTCUT_SETUP_TOKENS		20

/* player setup */
#define SETUP_TOKEN_PLAYER_USE_JOYSTICK		0
#define SETUP_TOKEN_PLAYER_JOY_DEVICE_NAME	1
#define SETUP_TOKEN_PLAYER_JOY_XLEFT		2
#define SETUP_TOKEN_PLAYER_JOY_XMIDDLE		3
#define SETUP_TOKEN_PLAYER_JOY_XRIGHT		4
#define SETUP_TOKEN_PLAYER_JOY_YUPPER		5
#define SETUP_TOKEN_PLAYER_JOY_YMIDDLE		6
#define SETUP_TOKEN_PLAYER_JOY_YLOWER		7
#define SETUP_TOKEN_PLAYER_JOY_SNAP		8
#define SETUP_TOKEN_PLAYER_JOY_DROP		9
#define SETUP_TOKEN_PLAYER_KEY_LEFT		10
#define SETUP_TOKEN_PLAYER_KEY_RIGHT		11
#define SETUP_TOKEN_PLAYER_KEY_UP		12
#define SETUP_TOKEN_PLAYER_KEY_DOWN		13
#define SETUP_TOKEN_PLAYER_KEY_SNAP		14
#define SETUP_TOKEN_PLAYER_KEY_DROP		15

#define NUM_PLAYER_SETUP_TOKENS			16

/* system setup */
#define SETUP_TOKEN_SYSTEM_SDL_VIDEODRIVER	0
#define SETUP_TOKEN_SYSTEM_SDL_AUDIODRIVER	1
#define SETUP_TOKEN_SYSTEM_AUDIO_FRAGMENT_SIZE	2

#define NUM_SYSTEM_SETUP_TOKENS			3

/* options setup */
#define SETUP_TOKEN_OPTIONS_VERBOSE		0

#define NUM_OPTIONS_SETUP_TOKENS		1


static struct SetupInfo si;
static struct SetupEditorInfo sei;
static struct SetupEditorCascadeInfo seci;
static struct SetupShortcutInfo ssi;
static struct SetupInputInfo sii;
static struct SetupSystemInfo syi;
static struct OptionInfo soi;

static struct TokenInfo global_setup_tokens[] =
{
  { TYPE_STRING, &si.player_name,             "player_name"		},
  { TYPE_SWITCH, &si.sound,                   "sound"			},
  { TYPE_SWITCH, &si.sound_loops,             "repeating_sound_loops"	},
  { TYPE_SWITCH, &si.sound_music,             "background_music"	},
  { TYPE_SWITCH, &si.sound_simple,            "simple_sound_effects"	},
  { TYPE_SWITCH, &si.toons,                   "toons"			},
  { TYPE_SWITCH, &si.scroll_delay,            "scroll_delay"		},
  { TYPE_INTEGER,&si.scroll_delay_value,      "scroll_delay_value"	},
  { TYPE_SWITCH, &si.soft_scrolling,          "soft_scrolling"		},
  { TYPE_SWITCH, &si.fade_screens,            "fade_screens"		},
  { TYPE_SWITCH, &si.autorecord,              "automatic_tape_recording"},
  { TYPE_SWITCH, &si.show_titlescreen,        "show_titlescreen"	},
  { TYPE_SWITCH, &si.quick_doors,             "quick_doors"		},
  { TYPE_SWITCH, &si.team_mode,               "team_mode"		},
  { TYPE_SWITCH, &si.handicap,                "handicap"		},
  { TYPE_SWITCH, &si.skip_levels,             "skip_levels"		},
  { TYPE_SWITCH, &si.time_limit,              "time_limit"		},
  { TYPE_SWITCH, &si.fullscreen,              "fullscreen"		},
  { TYPE_STRING, &si.fullscreen_mode,         "fullscreen_mode"		},
  { TYPE_SWITCH, &si.ask_on_escape,           "ask_on_escape"		},
  { TYPE_SWITCH, &si.ask_on_escape_editor,    "ask_on_escape_editor"	},
  { TYPE_SWITCH, &si.quick_switch,            "quick_player_switch"	},
  { TYPE_SWITCH, &si.input_on_focus,          "input_on_focus"		},
  { TYPE_SWITCH, &si.prefer_aga_graphics,     "prefer_aga_graphics"	},
  { TYPE_INTEGER,&si.game_frame_delay,        "game_frame_delay"	},
  { TYPE_SWITCH, &si.sp_show_border_elements, "sp_show_border_elements"	},
  { TYPE_STRING, &si.graphics_set,            "graphics_set"		},
  { TYPE_STRING, &si.sounds_set,              "sounds_set"		},
  { TYPE_STRING, &si.music_set,               "music_set"		},
  { TYPE_SWITCH3,&si.override_level_graphics, "override_level_graphics"	},
  { TYPE_SWITCH3,&si.override_level_sounds,   "override_level_sounds"	},
  { TYPE_SWITCH3,&si.override_level_music,    "override_level_music"	},
};

static boolean not_used = FALSE;
static struct TokenInfo editor_setup_tokens[] =
{
#if 1
  { TYPE_SWITCH, &not_used,		"editor.el_boulderdash"		},
  { TYPE_SWITCH, &not_used,		"editor.el_emerald_mine"	},
  { TYPE_SWITCH, &not_used,		"editor.el_emerald_mine_club"	},
  { TYPE_SWITCH, &not_used,		"editor.el_more"		},
  { TYPE_SWITCH, &not_used,		"editor.el_sokoban"		},
  { TYPE_SWITCH, &not_used,		"editor.el_supaplex"		},
  { TYPE_SWITCH, &not_used,		"editor.el_diamond_caves"	},
  { TYPE_SWITCH, &not_used,		"editor.el_dx_boulderdash"	},
#else
  { TYPE_SWITCH, &sei.el_boulderdash,	"editor.el_boulderdash"		},
  { TYPE_SWITCH, &sei.el_emerald_mine,	"editor.el_emerald_mine"	},
  { TYPE_SWITCH, &sei.el_emerald_mine_club,"editor.el_emerald_mine_club"},
  { TYPE_SWITCH, &sei.el_more,		"editor.el_more"		},
  { TYPE_SWITCH, &sei.el_sokoban,	"editor.el_sokoban"		},
  { TYPE_SWITCH, &sei.el_supaplex,	"editor.el_supaplex"		},
  { TYPE_SWITCH, &sei.el_diamond_caves,	"editor.el_diamond_caves"	},
  { TYPE_SWITCH, &sei.el_dx_boulderdash,"editor.el_dx_boulderdash"	},
#endif
  { TYPE_SWITCH, &sei.el_chars,		"editor.el_chars"		},
  { TYPE_SWITCH, &sei.el_steel_chars,	"editor.el_steel_chars"		},
  { TYPE_SWITCH, &sei.el_custom,	"editor.el_custom"		},
#if 1
  { TYPE_SWITCH, &not_used,		"editor.el_headlines"		},
#else
  { TYPE_SWITCH, &sei.el_headlines,	"editor.el_headlines"		},
#endif
  { TYPE_SWITCH, &sei.el_user_defined,	"editor.el_user_defined"	},
  { TYPE_SWITCH, &sei.el_dynamic,	"editor.el_dynamic"		},
  { TYPE_SWITCH, &sei.el_by_game,	"editor.el_by_game"		},
  { TYPE_SWITCH, &sei.el_by_type,	"editor.el_by_type"		},
  { TYPE_SWITCH, &sei.show_element_token,"editor.show_element_token"	},
};

static struct TokenInfo editor_cascade_setup_tokens[] =
{
  { TYPE_SWITCH, &seci.el_bd,		"editor.cascade.el_bd"		},
  { TYPE_SWITCH, &seci.el_em,		"editor.cascade.el_em"		},
  { TYPE_SWITCH, &seci.el_emc,		"editor.cascade.el_emc"		},
  { TYPE_SWITCH, &seci.el_rnd,		"editor.cascade.el_rnd"		},
  { TYPE_SWITCH, &seci.el_sb,		"editor.cascade.el_sb"		},
  { TYPE_SWITCH, &seci.el_sp,		"editor.cascade.el_sp"		},
  { TYPE_SWITCH, &seci.el_dc,		"editor.cascade.el_dc"		},
  { TYPE_SWITCH, &seci.el_dx,		"editor.cascade.el_dx"		},
  { TYPE_SWITCH, &seci.el_chars,	"editor.cascade.el_chars"	},
  { TYPE_SWITCH, &seci.el_steel_chars,	"editor.cascade.el_steel_chars"	},
  { TYPE_SWITCH, &seci.el_ce,		"editor.cascade.el_ce"		},
  { TYPE_SWITCH, &seci.el_ge,		"editor.cascade.el_ge"		},
  { TYPE_SWITCH, &seci.el_ref,		"editor.cascade.el_ref"		},
  { TYPE_SWITCH, &seci.el_user,		"editor.cascade.el_user"	},
  { TYPE_SWITCH, &seci.el_dynamic,	"editor.cascade.el_dynamic"	},
};

static struct TokenInfo shortcut_setup_tokens[] =
{
  { TYPE_KEY_X11, &ssi.save_game,	"shortcut.save_game"		},
  { TYPE_KEY_X11, &ssi.load_game,	"shortcut.load_game"		},
  { TYPE_KEY_X11, &ssi.toggle_pause,	"shortcut.toggle_pause"		},
  { TYPE_KEY_X11, &ssi.focus_player[0],	"shortcut.focus_player_1"	},
  { TYPE_KEY_X11, &ssi.focus_player[1],	"shortcut.focus_player_2"	},
  { TYPE_KEY_X11, &ssi.focus_player[2],	"shortcut.focus_player_3"	},
  { TYPE_KEY_X11, &ssi.focus_player[3],	"shortcut.focus_player_4"	},
  { TYPE_KEY_X11, &ssi.focus_player_all,"shortcut.focus_player_all"	},
  { TYPE_KEY_X11, &ssi.tape_eject,	"shortcut.tape_eject"		},
  { TYPE_KEY_X11, &ssi.tape_stop,	"shortcut.tape_stop"		},
  { TYPE_KEY_X11, &ssi.tape_pause,	"shortcut.tape_pause"		},
  { TYPE_KEY_X11, &ssi.tape_record,	"shortcut.tape_record"		},
  { TYPE_KEY_X11, &ssi.tape_play,	"shortcut.tape_play"		},
  { TYPE_KEY_X11, &ssi.sound_simple,	"shortcut.sound_simple"		},
  { TYPE_KEY_X11, &ssi.sound_loops,	"shortcut.sound_loops"		},
  { TYPE_KEY_X11, &ssi.sound_music,	"shortcut.sound_music"		},
  { TYPE_KEY_X11, &ssi.snap_left,	"shortcut.snap_left"		},
  { TYPE_KEY_X11, &ssi.snap_right,	"shortcut.snap_right"		},
  { TYPE_KEY_X11, &ssi.snap_up,		"shortcut.snap_up"		},
  { TYPE_KEY_X11, &ssi.snap_down,	"shortcut.snap_down"		},
};

static struct TokenInfo player_setup_tokens[] =
{
  { TYPE_BOOLEAN, &sii.use_joystick,	".use_joystick"			},
  { TYPE_STRING,  &sii.joy.device_name,	".joy.device_name"		},
  { TYPE_INTEGER, &sii.joy.xleft,	".joy.xleft"			},
  { TYPE_INTEGER, &sii.joy.xmiddle,	".joy.xmiddle"			},
  { TYPE_INTEGER, &sii.joy.xright,	".joy.xright"			},
  { TYPE_INTEGER, &sii.joy.yupper,	".joy.yupper"			},
  { TYPE_INTEGER, &sii.joy.ymiddle,	".joy.ymiddle"			},
  { TYPE_INTEGER, &sii.joy.ylower,	".joy.ylower"			},
  { TYPE_INTEGER, &sii.joy.snap,	".joy.snap_field"		},
  { TYPE_INTEGER, &sii.joy.drop,	".joy.place_bomb"		},
  { TYPE_KEY_X11, &sii.key.left,	".key.move_left"		},
  { TYPE_KEY_X11, &sii.key.right,	".key.move_right"		},
  { TYPE_KEY_X11, &sii.key.up,		".key.move_up"			},
  { TYPE_KEY_X11, &sii.key.down,	".key.move_down"		},
  { TYPE_KEY_X11, &sii.key.snap,	".key.snap_field"		},
  { TYPE_KEY_X11, &sii.key.drop,	".key.place_bomb"		},
};

static struct TokenInfo system_setup_tokens[] =
{
  { TYPE_STRING,  &syi.sdl_videodriver,	"system.sdl_videodriver"	},
  { TYPE_STRING,  &syi.sdl_audiodriver,	"system.sdl_audiodriver"	},
  { TYPE_INTEGER, &syi.audio_fragment_size,"system.audio_fragment_size"	},
};

static struct TokenInfo options_setup_tokens[] =
{
  { TYPE_BOOLEAN, &soi.verbose,		"options.verbose"		},
};

static char *get_corrected_login_name(char *login_name)
{
  /* needed because player name must be a fixed length string */
  char *login_name_new = checked_malloc(MAX_PLAYER_NAME_LEN + 1);

  strncpy(login_name_new, login_name, MAX_PLAYER_NAME_LEN);
  login_name_new[MAX_PLAYER_NAME_LEN] = '\0';

  if (strlen(login_name) > MAX_PLAYER_NAME_LEN)		/* name has been cut */
    if (strchr(login_name_new, ' '))
      *strchr(login_name_new, ' ') = '\0';

  return login_name_new;
}

static void setSetupInfoToDefaults(struct SetupInfo *si)
{
  int i;

  si->player_name = get_corrected_login_name(getLoginName());

  si->sound = TRUE;
  si->sound_loops = TRUE;
  si->sound_music = TRUE;
  si->sound_simple = TRUE;
  si->toons = TRUE;
  si->scroll_delay = TRUE;
  si->scroll_delay_value = STD_SCROLL_DELAY;
  si->soft_scrolling = TRUE;
  si->fade_screens = TRUE;
  si->autorecord = TRUE;
  si->show_titlescreen = TRUE;
  si->quick_doors = FALSE;
  si->team_mode = FALSE;
  si->handicap = TRUE;
  si->skip_levels = TRUE;
  si->time_limit = TRUE;
  si->fullscreen = FALSE;
  si->fullscreen_mode = getStringCopy(DEFAULT_FULLSCREEN_MODE);
  si->ask_on_escape = TRUE;
  si->ask_on_escape_editor = TRUE;
  si->quick_switch = FALSE;
  si->input_on_focus = FALSE;
  si->prefer_aga_graphics = TRUE;
  si->game_frame_delay = GAME_FRAME_DELAY;
  si->sp_show_border_elements = FALSE;

  si->graphics_set = getStringCopy(GFX_DEFAULT_SUBDIR);
  si->sounds_set = getStringCopy(SND_DEFAULT_SUBDIR);
  si->music_set = getStringCopy(MUS_DEFAULT_SUBDIR);
  si->override_level_graphics = FALSE;
  si->override_level_sounds = FALSE;
  si->override_level_music = FALSE;

  si->editor.el_boulderdash		= TRUE;
  si->editor.el_emerald_mine		= TRUE;
  si->editor.el_emerald_mine_club	= TRUE;
  si->editor.el_more			= TRUE;
  si->editor.el_sokoban			= TRUE;
  si->editor.el_supaplex		= TRUE;
  si->editor.el_diamond_caves		= TRUE;
  si->editor.el_dx_boulderdash		= TRUE;
  si->editor.el_chars			= TRUE;
  si->editor.el_steel_chars		= TRUE;
  si->editor.el_custom			= TRUE;

  si->editor.el_headlines = TRUE;
  si->editor.el_user_defined = FALSE;
  si->editor.el_dynamic = TRUE;

  si->editor.show_element_token = FALSE;

  si->shortcut.save_game	= DEFAULT_KEY_SAVE_GAME;
  si->shortcut.load_game	= DEFAULT_KEY_LOAD_GAME;
  si->shortcut.toggle_pause	= DEFAULT_KEY_TOGGLE_PAUSE;

  si->shortcut.focus_player[0]	= DEFAULT_KEY_FOCUS_PLAYER_1;
  si->shortcut.focus_player[1]	= DEFAULT_KEY_FOCUS_PLAYER_2;
  si->shortcut.focus_player[2]	= DEFAULT_KEY_FOCUS_PLAYER_3;
  si->shortcut.focus_player[3]	= DEFAULT_KEY_FOCUS_PLAYER_4;
  si->shortcut.focus_player_all	= DEFAULT_KEY_FOCUS_PLAYER_ALL;

  si->shortcut.tape_eject	= DEFAULT_KEY_TAPE_EJECT;
  si->shortcut.tape_stop	= DEFAULT_KEY_TAPE_STOP;
  si->shortcut.tape_pause	= DEFAULT_KEY_TAPE_PAUSE;
  si->shortcut.tape_record	= DEFAULT_KEY_TAPE_RECORD;
  si->shortcut.tape_play	= DEFAULT_KEY_TAPE_PLAY;

  si->shortcut.sound_simple	= DEFAULT_KEY_SOUND_SIMPLE;
  si->shortcut.sound_loops	= DEFAULT_KEY_SOUND_LOOPS;
  si->shortcut.sound_music	= DEFAULT_KEY_SOUND_MUSIC;

  si->shortcut.snap_left	= DEFAULT_KEY_SNAP_LEFT;
  si->shortcut.snap_right	= DEFAULT_KEY_SNAP_RIGHT;
  si->shortcut.snap_up		= DEFAULT_KEY_SNAP_UP;
  si->shortcut.snap_down	= DEFAULT_KEY_SNAP_DOWN;

  for (i = 0; i < MAX_PLAYERS; i++)
  {
    si->input[i].use_joystick = FALSE;
    si->input[i].joy.device_name=getStringCopy(getDeviceNameFromJoystickNr(i));
    si->input[i].joy.xleft   = JOYSTICK_XLEFT;
    si->input[i].joy.xmiddle = JOYSTICK_XMIDDLE;
    si->input[i].joy.xright  = JOYSTICK_XRIGHT;
    si->input[i].joy.yupper  = JOYSTICK_YUPPER;
    si->input[i].joy.ymiddle = JOYSTICK_YMIDDLE;
    si->input[i].joy.ylower  = JOYSTICK_YLOWER;
    si->input[i].joy.snap  = (i == 0 ? JOY_BUTTON_1 : 0);
    si->input[i].joy.drop  = (i == 0 ? JOY_BUTTON_2 : 0);
    si->input[i].key.left  = (i == 0 ? DEFAULT_KEY_LEFT  : KSYM_UNDEFINED);
    si->input[i].key.right = (i == 0 ? DEFAULT_KEY_RIGHT : KSYM_UNDEFINED);
    si->input[i].key.up    = (i == 0 ? DEFAULT_KEY_UP    : KSYM_UNDEFINED);
    si->input[i].key.down  = (i == 0 ? DEFAULT_KEY_DOWN  : KSYM_UNDEFINED);
    si->input[i].key.snap  = (i == 0 ? DEFAULT_KEY_SNAP  : KSYM_UNDEFINED);
    si->input[i].key.drop  = (i == 0 ? DEFAULT_KEY_DROP  : KSYM_UNDEFINED);
  }

  si->system.sdl_videodriver = getStringCopy(ARG_DEFAULT);
  si->system.sdl_audiodriver = getStringCopy(ARG_DEFAULT);
  si->system.audio_fragment_size = DEFAULT_AUDIO_FRAGMENT_SIZE;

  si->options.verbose = FALSE;

#if defined(CREATE_SPECIAL_EDITION_RND_JUE)
  si->toons = FALSE;
  si->handicap = FALSE;
  si->fullscreen = TRUE;
  si->override_level_graphics = AUTO;
  si->override_level_sounds = AUTO;
  si->override_level_music = AUTO;
#endif
}

static void setSetupInfoToDefaults_EditorCascade(struct SetupInfo *si)
{
  si->editor_cascade.el_bd		= TRUE;
  si->editor_cascade.el_em		= TRUE;
  si->editor_cascade.el_emc		= TRUE;
  si->editor_cascade.el_rnd		= TRUE;
  si->editor_cascade.el_sb		= TRUE;
  si->editor_cascade.el_sp		= TRUE;
  si->editor_cascade.el_dc		= TRUE;
  si->editor_cascade.el_dx		= TRUE;

  si->editor_cascade.el_chars		= FALSE;
  si->editor_cascade.el_steel_chars	= FALSE;
  si->editor_cascade.el_ce		= FALSE;
  si->editor_cascade.el_ge		= FALSE;
  si->editor_cascade.el_ref		= FALSE;
  si->editor_cascade.el_user		= FALSE;
  si->editor_cascade.el_dynamic		= FALSE;
}

static void decodeSetupFileHash(SetupFileHash *setup_file_hash)
{
  int i, pnr;

  if (!setup_file_hash)
    return;

  /* global setup */
  si = setup;
  for (i = 0; i < NUM_GLOBAL_SETUP_TOKENS; i++)
    setSetupInfo(global_setup_tokens, i,
		 getHashEntry(setup_file_hash, global_setup_tokens[i].text));
  setup = si;

  /* editor setup */
  sei = setup.editor;
  for (i = 0; i < NUM_EDITOR_SETUP_TOKENS; i++)
    setSetupInfo(editor_setup_tokens, i,
		 getHashEntry(setup_file_hash,editor_setup_tokens[i].text));
  setup.editor = sei;

  /* shortcut setup */
  ssi = setup.shortcut;
  for (i = 0; i < NUM_SHORTCUT_SETUP_TOKENS; i++)
    setSetupInfo(shortcut_setup_tokens, i,
		 getHashEntry(setup_file_hash,shortcut_setup_tokens[i].text));
  setup.shortcut = ssi;

  /* player setup */
  for (pnr = 0; pnr < MAX_PLAYERS; pnr++)
  {
    char prefix[30];

    sprintf(prefix, "%s%d", TOKEN_STR_PLAYER_PREFIX, pnr + 1);

    sii = setup.input[pnr];
    for (i = 0; i < NUM_PLAYER_SETUP_TOKENS; i++)
    {
      char full_token[100];

      sprintf(full_token, "%s%s", prefix, player_setup_tokens[i].text);
      setSetupInfo(player_setup_tokens, i,
		   getHashEntry(setup_file_hash, full_token));
    }
    setup.input[pnr] = sii;
  }

  /* system setup */
  syi = setup.system;
  for (i = 0; i < NUM_SYSTEM_SETUP_TOKENS; i++)
    setSetupInfo(system_setup_tokens, i,
		 getHashEntry(setup_file_hash, system_setup_tokens[i].text));
  setup.system = syi;

  /* options setup */
  soi = setup.options;
  for (i = 0; i < NUM_OPTIONS_SETUP_TOKENS; i++)
    setSetupInfo(options_setup_tokens, i,
		 getHashEntry(setup_file_hash, options_setup_tokens[i].text));
  setup.options = soi;
}

static void decodeSetupFileHash_EditorCascade(SetupFileHash *setup_file_hash)
{
  int i;

  if (!setup_file_hash)
    return;

  /* editor cascade setup */
  seci = setup.editor_cascade;
  for (i = 0; i < NUM_EDITOR_CASCADE_SETUP_TOKENS; i++)
    setSetupInfo(editor_cascade_setup_tokens, i,
		 getHashEntry(setup_file_hash,
			      editor_cascade_setup_tokens[i].text));
  setup.editor_cascade = seci;
}

void LoadSetup()
{
  char *filename = getSetupFilename();
  SetupFileHash *setup_file_hash = NULL;

  /* always start with reliable default values */
  setSetupInfoToDefaults(&setup);

  setup_file_hash = loadSetupFileHash(filename);

  if (setup_file_hash)
  {
    char *player_name_new;

    checkSetupFileHashIdentifier(setup_file_hash, filename,getCookie("SETUP"));
    decodeSetupFileHash(setup_file_hash);

    freeSetupFileHash(setup_file_hash);

    /* needed to work around problems with fixed length strings */
    player_name_new = get_corrected_login_name(setup.player_name);
    free(setup.player_name);
    setup.player_name = player_name_new;

    /* "scroll_delay: on(3) / off(0)" was replaced by scroll delay value */
    if (setup.scroll_delay == FALSE)
    {
      setup.scroll_delay_value = MIN_SCROLL_DELAY;
      setup.scroll_delay = TRUE;			/* now always "on" */
    }

    /* make sure that scroll delay value stays inside valid range */
    setup.scroll_delay_value =
      MIN(MAX(MIN_SCROLL_DELAY, setup.scroll_delay_value), MAX_SCROLL_DELAY);
  }
  else
    Error(ERR_WARN, "using default setup values");
}

void LoadSetup_EditorCascade()
{
  char *filename = getPath2(getSetupDir(), EDITORCASCADE_FILENAME);
  SetupFileHash *setup_file_hash = NULL;

  /* always start with reliable default values */
  setSetupInfoToDefaults_EditorCascade(&setup);

  setup_file_hash = loadSetupFileHash(filename);

  if (setup_file_hash)
  {
    checkSetupFileHashIdentifier(setup_file_hash, filename,getCookie("SETUP"));
    decodeSetupFileHash_EditorCascade(setup_file_hash);

    freeSetupFileHash(setup_file_hash);
  }

  free(filename);
}

void SaveSetup()
{
  char *filename = getSetupFilename();
  FILE *file;
  int i, pnr;

  InitUserDataDirectory();

  if (!(file = fopen(filename, MODE_WRITE)))
  {
    Error(ERR_WARN, "cannot write setup file '%s'", filename);
    return;
  }

  fprintf(file, "%s\n", getFormattedSetupEntry(TOKEN_STR_FILE_IDENTIFIER,
					       getCookie("SETUP")));
  fprintf(file, "\n");

  /* global setup */
  si = setup;
  for (i = 0; i < NUM_GLOBAL_SETUP_TOKENS; i++)
  {
    /* just to make things nicer :) */
    if (i == SETUP_TOKEN_PLAYER_NAME + 1 ||
	i == SETUP_TOKEN_GRAPHICS_SET)
      fprintf(file, "\n");

    fprintf(file, "%s\n", getSetupLine(global_setup_tokens, "", i));
  }

  /* editor setup */
  sei = setup.editor;
  fprintf(file, "\n");
  for (i = 0; i < NUM_EDITOR_SETUP_TOKENS; i++)
    fprintf(file, "%s\n", getSetupLine(editor_setup_tokens, "", i));

  /* shortcut setup */
  ssi = setup.shortcut;
  fprintf(file, "\n");
  for (i = 0; i < NUM_SHORTCUT_SETUP_TOKENS; i++)
    fprintf(file, "%s\n", getSetupLine(shortcut_setup_tokens, "", i));

  /* player setup */
  for (pnr = 0; pnr < MAX_PLAYERS; pnr++)
  {
    char prefix[30];

    sprintf(prefix, "%s%d", TOKEN_STR_PLAYER_PREFIX, pnr + 1);
    fprintf(file, "\n");

    sii = setup.input[pnr];
    for (i = 0; i < NUM_PLAYER_SETUP_TOKENS; i++)
      fprintf(file, "%s\n", getSetupLine(player_setup_tokens, prefix, i));
  }

  /* system setup */
  syi = setup.system;
  fprintf(file, "\n");
  for (i = 0; i < NUM_SYSTEM_SETUP_TOKENS; i++)
    fprintf(file, "%s\n", getSetupLine(system_setup_tokens, "", i));

  /* options setup */
  soi = setup.options;
  fprintf(file, "\n");
  for (i = 0; i < NUM_OPTIONS_SETUP_TOKENS; i++)
    fprintf(file, "%s\n", getSetupLine(options_setup_tokens, "", i));

  fclose(file);

  SetFilePermissions(filename, PERMS_PRIVATE);
}

void SaveSetup_EditorCascade()
{
  char *filename = getPath2(getSetupDir(), EDITORCASCADE_FILENAME);
  FILE *file;
  int i;

  InitUserDataDirectory();

  if (!(file = fopen(filename, MODE_WRITE)))
  {
    Error(ERR_WARN, "cannot write editor cascade state file '%s'", filename);
    free(filename);
    return;
  }

  fprintf(file, "%s\n", getFormattedSetupEntry(TOKEN_STR_FILE_IDENTIFIER,
					       getCookie("SETUP")));
  fprintf(file, "\n");

  seci = setup.editor_cascade;
  fprintf(file, "\n");
  for (i = 0; i < NUM_EDITOR_CASCADE_SETUP_TOKENS; i++)
    fprintf(file, "%s\n", getSetupLine(editor_cascade_setup_tokens, "", i));

  fclose(file);

  SetFilePermissions(filename, PERMS_PRIVATE);

  free(filename);
}

void LoadCustomElementDescriptions()
{
  char *filename = getCustomArtworkConfigFilename(ARTWORK_TYPE_GRAPHICS);
  SetupFileHash *setup_file_hash;
  int i;

  for (i = 0; i < NUM_FILE_ELEMENTS; i++)
  {
    if (element_info[i].custom_description != NULL)
    {
      free(element_info[i].custom_description);
      element_info[i].custom_description = NULL;
    }
  }

  if ((setup_file_hash = loadSetupFileHash(filename)) == NULL)
    return;

  for (i = 0; i < NUM_FILE_ELEMENTS; i++)
  {
    char *token = getStringCat2(element_info[i].token_name, ".name");
    char *value = getHashEntry(setup_file_hash, token);

    if (value != NULL)
      element_info[i].custom_description = getStringCopy(value);

    free(token);
  }

  freeSetupFileHash(setup_file_hash);
}

static int getElementFromToken(char *token)
{
#if 1
  char *value = getHashEntry(element_token_hash, token);

  if (value != NULL)
    return atoi(value);
#else
  int i;

  /* !!! OPTIMIZE THIS BY USING HASH !!! */
  for (i = 0; i < MAX_NUM_ELEMENTS; i++)
    if (strEqual(token, element_info[i].token_name))
      return i;
#endif

  Error(ERR_WARN, "unknown element token '%s'", token);

  return EL_UNDEFINED;
}

static int get_token_parameter_value(char *token, char *value_raw)
{
  char *suffix;

  if (token == NULL || value_raw == NULL)
    return ARG_UNDEFINED_VALUE;

  suffix = strrchr(token, '.');
  if (suffix == NULL)
    suffix = token;

#if 1
  if (strEqual(suffix, ".element"))
    return getElementFromToken(value_raw);
#endif

#if 0
  if (strncmp(suffix, ".font", 5) == 0)
  {
    int i;

    /* !!! OPTIMIZE THIS BY USING HASH !!! */
    for (i = 0; i < NUM_FONTS; i++)
      if (strEqual(value_raw, font_info[i].token_name))
	return i;

    /* if font not found, use reliable default value */
    return FONT_INITIAL_1;
  }
#endif

  /* !!! USE CORRECT VALUE TYPE (currently works also for TYPE_BOOLEAN) !!! */
  return get_parameter_value(value_raw, suffix, TYPE_INTEGER);
}

void InitMenuDesignSettings_Static()
{
#if 0
  static SetupFileHash *image_config_hash = NULL;
#endif
  int i;

#if 0
  if (image_config_hash == NULL)
  {
    image_config_hash = newSetupFileHash();

    for (i = 0; image_config[i].token != NULL; i++)
      setHashEntry(image_config_hash,
		   image_config[i].token,
		   image_config[i].value);
  }
#endif

#if 1
  /* always start with reliable default values from static default config */
  for (i = 0; image_config_vars[i].token != NULL; i++)
  {
    char *value = getHashEntry(image_config_hash, image_config_vars[i].token);

    if (value != NULL)
      *image_config_vars[i].value =
	get_token_parameter_value(image_config_vars[i].token, value);
  }

#else

  int j;

  /* always start with reliable default values from static default config */
  for (i = 0; image_config_vars[i].token != NULL; i++)
    for (j = 0; image_config[j].token != NULL; j++)
      if (strEqual(image_config_vars[i].token, image_config[j].token))
	*image_config_vars[i].value =
	  get_token_parameter_value(image_config_vars[i].token,
				    image_config[j].value);
#endif
}

static void InitMenuDesignSettings_SpecialPreProcessing()
{
  int i;

  /* the following initializes hierarchical values from static configuration */

  /* special case: initialize "ARG_DEFAULT" values in static default config */
  /* (e.g., initialize "[titlemessage].fade_mode" from "[title].fade_mode") */
  titlemessage_initial_default.fade_mode  = title_initial_default.fade_mode;
  titlemessage_initial_default.fade_delay = title_initial_default.fade_delay;
  titlemessage_initial_default.post_delay = title_initial_default.post_delay;
  titlemessage_initial_default.auto_delay = title_initial_default.auto_delay;
  titlemessage_default.fade_mode  = title_default.fade_mode;
  titlemessage_default.fade_delay = title_default.fade_delay;
  titlemessage_default.post_delay = title_default.post_delay;
  titlemessage_default.auto_delay = title_default.auto_delay;

  /* special case: initialize "ARG_DEFAULT" values in static default config */
  /* (e.g., init "titlemessage_1.fade_mode" from "[titlemessage].fade_mode") */
  for (i = 0; i < MAX_NUM_TITLE_MESSAGES; i++)
  {
    titlemessage_initial[i] = titlemessage_initial_default;
    titlemessage[i] = titlemessage_default;
  }

  /* special case: initialize "ARG_DEFAULT" values in static default config */
  /* (eg, init "menu.enter_screen.SCORES.xyz" from "menu.enter_screen.xyz") */
  for (i = 0; i < NUM_SPECIAL_GFX_ARGS; i++)
  {
    menu.enter_screen[i] = menu.enter_screen[GFX_SPECIAL_ARG_DEFAULT];
    menu.leave_screen[i] = menu.leave_screen[GFX_SPECIAL_ARG_DEFAULT];
  }

  /* special case: initialize "ARG_DEFAULT" values in static default config */
  /* (eg, init "viewport.door_1.MAIN.xyz" from "viewport.door_1.xyz") */
  for (i = 0; i < NUM_SPECIAL_GFX_ARGS; i++)
  {
    viewport.playfield[i] = viewport.playfield[GFX_SPECIAL_ARG_DEFAULT];
    viewport.door_1[i] = viewport.door_1[GFX_SPECIAL_ARG_DEFAULT];
    if (i != GFX_SPECIAL_ARG_EDITOR)	/* editor value already initialized */
      viewport.door_2[i] = viewport.door_2[GFX_SPECIAL_ARG_DEFAULT];
  }
}

static void InitMenuDesignSettings_SpecialPostProcessing()
{
  /* special case: initialize later added SETUP list size from LEVELS value */
  if (menu.list_size[GAME_MODE_SETUP] == -1)
    menu.list_size[GAME_MODE_SETUP] = menu.list_size[GAME_MODE_LEVELS];
}

static void LoadMenuDesignSettingsFromFilename(char *filename)
{
  static struct TitleMessageInfo tmi;
  static struct TokenInfo titlemessage_tokens[] =
  {
    { TYPE_INTEGER,	&tmi.x,			".x"			},
    { TYPE_INTEGER,	&tmi.y,			".y"			},
    { TYPE_INTEGER,	&tmi.width,		".width"		},
    { TYPE_INTEGER,	&tmi.height,		".height"		},
    { TYPE_INTEGER,	&tmi.chars,		".chars"		},
    { TYPE_INTEGER,	&tmi.lines,		".lines"		},
    { TYPE_INTEGER,	&tmi.align,		".align"		},
    { TYPE_INTEGER,	&tmi.valign,		".valign"		},
    { TYPE_INTEGER,	&tmi.font,		".font"			},
    { TYPE_BOOLEAN,	&tmi.autowrap,		".autowrap"		},
    { TYPE_BOOLEAN,	&tmi.centered,		".centered"		},
    { TYPE_BOOLEAN,	&tmi.parse_comments,	".parse_comments"	},
    { TYPE_INTEGER,	&tmi.sort_priority,	".sort_priority"	},
    { TYPE_INTEGER,	&tmi.fade_mode,		".fade_mode"		},
    { TYPE_INTEGER,	&tmi.fade_delay,	".fade_delay"		},
    { TYPE_INTEGER,	&tmi.post_delay,	".post_delay"		},
    { TYPE_INTEGER,	&tmi.auto_delay,	".auto_delay"		},

    { -1,		NULL,			NULL			}
  };
  static struct
  {
    struct TitleMessageInfo *array;
    char *text;
  }
  titlemessage_arrays[] =
  {
    { titlemessage_initial,		"[titlemessage_initial]"	},
    { titlemessage,			"[titlemessage]"		},

    { NULL,				NULL				}
  };
  SetupFileHash *setup_file_hash;
  int i, j, k;

#if 0
  printf("LoadMenuDesignSettings from file '%s' ...\n", filename);
#endif

  if ((setup_file_hash = loadSetupFileHash(filename)) == NULL)
    return;

  /* the following initializes hierarchical values from dynamic configuration */

  /* special case: initialize with default values that may be overwritten */
  /* (e.g., init "menu.draw_xoffset.INFO" from "menu.draw_xoffset") */
  for (i = 0; i < NUM_SPECIAL_GFX_ARGS; i++)
  {
    char *value_1 = getHashEntry(setup_file_hash, "menu.draw_xoffset");
    char *value_2 = getHashEntry(setup_file_hash, "menu.draw_yoffset");
    char *value_3 = getHashEntry(setup_file_hash, "menu.list_size");

    if (value_1 != NULL)
      menu.draw_xoffset[i] = get_integer_from_string(value_1);
    if (value_2 != NULL)
      menu.draw_yoffset[i] = get_integer_from_string(value_2);
    if (value_3 != NULL)
      menu.list_size[i] = get_integer_from_string(value_3);
  }

  /* special case: initialize with default values that may be overwritten */
  /* (eg, init "menu.draw_xoffset.INFO[XXX]" from "menu.draw_xoffset.INFO") */
  for (i = 0; i < NUM_SPECIAL_GFX_INFO_ARGS; i++)
  {
    char *value_1 = getHashEntry(setup_file_hash, "menu.draw_xoffset.INFO");
    char *value_2 = getHashEntry(setup_file_hash, "menu.draw_yoffset.INFO");

    if (value_1 != NULL)
      menu.draw_xoffset_info[i] = get_integer_from_string(value_1);
    if (value_2 != NULL)
      menu.draw_yoffset_info[i] = get_integer_from_string(value_2);
  }

  /* special case: initialize with default values that may be overwritten */
  /* (eg, init "menu.draw_xoffset.SETUP[XXX]" from "menu.draw_xoffset.SETUP") */
  for (i = 0; i < NUM_SPECIAL_GFX_SETUP_ARGS; i++)
  {
    char *value_1 = getHashEntry(setup_file_hash, "menu.draw_xoffset.SETUP");
    char *value_2 = getHashEntry(setup_file_hash, "menu.draw_yoffset.SETUP");

    if (value_1 != NULL)
      menu.draw_xoffset_setup[i] = get_integer_from_string(value_1);
    if (value_2 != NULL)
      menu.draw_yoffset_setup[i] = get_integer_from_string(value_2);
  }

  /* special case: initialize with default values that may be overwritten */
  /* (eg, init "menu.enter_screen.SCORES.xyz" from "menu.enter_screen.xyz") */
  for (i = 0; i < NUM_SPECIAL_GFX_ARGS; i++)
  {
    char *token_1 = "menu.enter_screen.fade_mode";
    char *token_2 = "menu.enter_screen.fade_delay";
    char *token_3 = "menu.enter_screen.post_delay";
    char *token_4 = "menu.leave_screen.fade_mode";
    char *token_5 = "menu.leave_screen.fade_delay";
    char *token_6 = "menu.leave_screen.post_delay";
    char *value_1 = getHashEntry(setup_file_hash, token_1);
    char *value_2 = getHashEntry(setup_file_hash, token_2);
    char *value_3 = getHashEntry(setup_file_hash, token_3);
    char *value_4 = getHashEntry(setup_file_hash, token_4);
    char *value_5 = getHashEntry(setup_file_hash, token_5);
    char *value_6 = getHashEntry(setup_file_hash, token_6);

    if (value_1 != NULL)
      menu.enter_screen[i].fade_mode = get_token_parameter_value(token_1,
								 value_1);
    if (value_2 != NULL)
      menu.enter_screen[i].fade_delay = get_token_parameter_value(token_2,
								  value_2);
    if (value_3 != NULL)
      menu.enter_screen[i].post_delay = get_token_parameter_value(token_3,
								  value_3);
    if (value_4 != NULL)
      menu.leave_screen[i].fade_mode = get_token_parameter_value(token_4,
								 value_4);
    if (value_5 != NULL)
      menu.leave_screen[i].fade_delay = get_token_parameter_value(token_5,
								  value_5);
    if (value_6 != NULL)
      menu.leave_screen[i].post_delay = get_token_parameter_value(token_6,
								  value_6);
  }

  /* special case: initialize with default values that may be overwritten */
  /* (eg, init "viewport.door_1.MAIN.xyz" from "viewport.door_1.xyz") */
  for (i = 0; i < NUM_SPECIAL_GFX_ARGS; i++)
  {
    char *token_1 = "viewport.playfield.x";
    char *token_2 = "viewport.playfield.y";
    char *token_3 = "viewport.playfield.width";
    char *token_4 = "viewport.playfield.height";
    char *token_5 = "viewport.playfield.border_size";
    char *token_6 = "viewport.door_1.x";
    char *token_7 = "viewport.door_1.y";
    char *token_8 = "viewport.door_2.x";
    char *token_9 = "viewport.door_2.y";
    char *value_1 = getHashEntry(setup_file_hash, token_1);
    char *value_2 = getHashEntry(setup_file_hash, token_2);
    char *value_3 = getHashEntry(setup_file_hash, token_3);
    char *value_4 = getHashEntry(setup_file_hash, token_4);
    char *value_5 = getHashEntry(setup_file_hash, token_5);
    char *value_6 = getHashEntry(setup_file_hash, token_6);
    char *value_7 = getHashEntry(setup_file_hash, token_7);
    char *value_8 = getHashEntry(setup_file_hash, token_8);
    char *value_9 = getHashEntry(setup_file_hash, token_9);

    if (value_1 != NULL)
      viewport.playfield[i].x = get_token_parameter_value(token_1, value_1);
    if (value_2 != NULL)
      viewport.playfield[i].y = get_token_parameter_value(token_2, value_2);
    if (value_3 != NULL)
      viewport.playfield[i].width = get_token_parameter_value(token_3, value_3);
    if (value_4 != NULL)
      viewport.playfield[i].height = get_token_parameter_value(token_4,value_4);
    if (value_5 != NULL)
      viewport.playfield[i].border_size = get_token_parameter_value(token_5,
								    value_5);
    if (value_6 != NULL)
      viewport.door_1[i].x = get_token_parameter_value(token_6, value_6);
    if (value_7 != NULL)
      viewport.door_1[i].y = get_token_parameter_value(token_7, value_7);
    if (value_8 != NULL)
      viewport.door_2[i].x = get_token_parameter_value(token_8, value_8);
    if (value_9 != NULL)
      viewport.door_2[i].y = get_token_parameter_value(token_9, value_9);
  }

  /* special case: initialize with default values that may be overwritten */
  /* (e.g., init "titlemessage_1.fade_mode" from "[titlemessage].fade_mode") */
  for (i = 0; titlemessage_arrays[i].array != NULL; i++)
  {
    struct TitleMessageInfo *array = titlemessage_arrays[i].array;
    char *base_token = titlemessage_arrays[i].text;

    for (j = 0; titlemessage_tokens[j].type != -1; j++)
    {
      char *token = getStringCat2(base_token, titlemessage_tokens[j].text);
      char *value = getHashEntry(setup_file_hash, token);

      if (value != NULL)
      {
	int parameter_value = get_token_parameter_value(token, value);

	for (k = 0; k < MAX_NUM_TITLE_MESSAGES; k++)
	{
	  tmi = array[k];

	  if (titlemessage_tokens[j].type == TYPE_INTEGER)
	    *(boolean *)titlemessage_tokens[j].value = (boolean)parameter_value;
	  else
	    *(int     *)titlemessage_tokens[j].value = (int)parameter_value;

	  array[k] = tmi;
	}
      }

      free(token);
    }
  }

  /* read (and overwrite with) values that may be specified in config file */
  for (i = 0; image_config_vars[i].token != NULL; i++)
  {
    char *value = getHashEntry(setup_file_hash, image_config_vars[i].token);

    /* (ignore definitions set to "[DEFAULT]" which are already initialized) */
    if (value != NULL && !strEqual(value, ARG_DEFAULT))
      *image_config_vars[i].value =
	get_token_parameter_value(image_config_vars[i].token, value);
  }

  freeSetupFileHash(setup_file_hash);
}

void LoadMenuDesignSettings()
{
  char *filename_base = UNDEFINED_FILENAME, *filename_local;

  InitMenuDesignSettings_Static();
  InitMenuDesignSettings_SpecialPreProcessing();

#if 1
  if (!GFX_OVERRIDE_ARTWORK(ARTWORK_TYPE_GRAPHICS))
#else
  if (!SETUP_OVERRIDE_ARTWORK(setup, ARTWORK_TYPE_GRAPHICS))
#endif
  {
    /* first look for special settings configured in level series config */
    filename_base = getCustomArtworkLevelConfigFilename(ARTWORK_TYPE_GRAPHICS);

    if (fileExists(filename_base))
      LoadMenuDesignSettingsFromFilename(filename_base);
  }

  filename_local = getCustomArtworkConfigFilename(ARTWORK_TYPE_GRAPHICS);

  if (filename_local != NULL && !strEqual(filename_base, filename_local))
    LoadMenuDesignSettingsFromFilename(filename_local);

  InitMenuDesignSettings_SpecialPostProcessing();
}

void LoadUserDefinedEditorElementList(int **elements, int *num_elements)
{
  char *filename = getEditorSetupFilename();
  SetupFileList *setup_file_list, *list;
  SetupFileHash *element_hash;
  int num_unknown_tokens = 0;
  int i;

  if ((setup_file_list = loadSetupFileList(filename)) == NULL)
    return;

  element_hash = newSetupFileHash();

  for (i = 0; i < NUM_FILE_ELEMENTS; i++)
    setHashEntry(element_hash, element_info[i].token_name, i_to_a(i));

  /* determined size may be larger than needed (due to unknown elements) */
  *num_elements = 0;
  for (list = setup_file_list; list != NULL; list = list->next)
    (*num_elements)++;

  /* add space for up to 3 more elements for padding that may be needed */
  *num_elements += 3;

  /* free memory for old list of elements, if needed */
  checked_free(*elements);

  /* allocate memory for new list of elements */
  *elements = checked_malloc(*num_elements * sizeof(int));

  *num_elements = 0;
  for (list = setup_file_list; list != NULL; list = list->next)
  {
    char *value = getHashEntry(element_hash, list->token);

    if (value == NULL)		/* try to find obsolete token mapping */
    {
      char *mapped_token = get_mapped_token(list->token);

      if (mapped_token != NULL)
      {
	value = getHashEntry(element_hash, mapped_token);

	free(mapped_token);
      }
    }

    if (value != NULL)
    {
      (*elements)[(*num_elements)++] = atoi(value);
    }
    else
    {
      if (num_unknown_tokens == 0)
      {
	Error(ERR_INFO_LINE, "-");
	Error(ERR_INFO, "warning: unknown token(s) found in config file:");
	Error(ERR_INFO, "- config file: '%s'", filename);

	num_unknown_tokens++;
      }

      Error(ERR_INFO, "- token: '%s'", list->token);
    }
  }

  if (num_unknown_tokens > 0)
    Error(ERR_INFO_LINE, "-");

  while (*num_elements % 4)	/* pad with empty elements, if needed */
    (*elements)[(*num_elements)++] = EL_EMPTY;

  freeSetupFileList(setup_file_list);
  freeSetupFileHash(element_hash);

#if 0
  for (i = 0; i < *num_elements; i++)
    printf("editor: element '%s' [%d]\n",
	   element_info[(*elements)[i]].token_name, (*elements)[i]);
#endif
}

static struct MusicFileInfo *get_music_file_info_ext(char *basename, int music,
						     boolean is_sound)
{
  SetupFileHash *setup_file_hash = NULL;
  struct MusicFileInfo tmp_music_file_info, *new_music_file_info;
  char *filename_music, *filename_prefix, *filename_info;
  struct
  {
    char *token;
    char **value_ptr;
  }
  token_to_value_ptr[] =
  {
    { "title_header",	&tmp_music_file_info.title_header	},
    { "artist_header",	&tmp_music_file_info.artist_header	},
    { "album_header",	&tmp_music_file_info.album_header	},
    { "year_header",	&tmp_music_file_info.year_header	},

    { "title",		&tmp_music_file_info.title		},
    { "artist",		&tmp_music_file_info.artist		},
    { "album",		&tmp_music_file_info.album		},
    { "year",		&tmp_music_file_info.year		},

    { NULL,		NULL					},
  };
  int i;

  filename_music = (is_sound ? getCustomSoundFilename(basename) :
		    getCustomMusicFilename(basename));

  if (filename_music == NULL)
    return NULL;

  /* ---------- try to replace file extension ---------- */

  filename_prefix = getStringCopy(filename_music);
  if (strrchr(filename_prefix, '.') != NULL)
    *strrchr(filename_prefix, '.') = '\0';
  filename_info = getStringCat2(filename_prefix, ".txt");

#if 0
  printf("trying to load file '%s'...\n", filename_info);
#endif

  if (fileExists(filename_info))
    setup_file_hash = loadSetupFileHash(filename_info);

  free(filename_prefix);
  free(filename_info);

  if (setup_file_hash == NULL)
  {
    /* ---------- try to add file extension ---------- */

    filename_prefix = getStringCopy(filename_music);
    filename_info = getStringCat2(filename_prefix, ".txt");

#if 0
    printf("trying to load file '%s'...\n", filename_info);
#endif

    if (fileExists(filename_info))
      setup_file_hash = loadSetupFileHash(filename_info);

    free(filename_prefix);
    free(filename_info);
  }

  if (setup_file_hash == NULL)
    return NULL;

  /* ---------- music file info found ---------- */

  clear_mem(&tmp_music_file_info, sizeof(struct MusicFileInfo));

  for (i = 0; token_to_value_ptr[i].token != NULL; i++)
  {
    char *value = getHashEntry(setup_file_hash, token_to_value_ptr[i].token);

    *token_to_value_ptr[i].value_ptr =
      getStringCopy(value != NULL && *value != '\0' ? value : UNKNOWN_NAME);
  }

  tmp_music_file_info.basename = getStringCopy(basename);
  tmp_music_file_info.music = music;
  tmp_music_file_info.is_sound = is_sound;

  new_music_file_info = checked_malloc(sizeof(struct MusicFileInfo));
  *new_music_file_info = tmp_music_file_info;

  return new_music_file_info;
}

static struct MusicFileInfo *get_music_file_info(char *basename, int music)
{
  return get_music_file_info_ext(basename, music, FALSE);
}

static struct MusicFileInfo *get_sound_file_info(char *basename, int sound)
{
  return get_music_file_info_ext(basename, sound, TRUE);
}

static boolean music_info_listed_ext(struct MusicFileInfo *list,
				     char *basename, boolean is_sound)
{
  for (; list != NULL; list = list->next)
    if (list->is_sound == is_sound && strEqual(list->basename, basename))
      return TRUE;

  return FALSE;
}

static boolean music_info_listed(struct MusicFileInfo *list, char *basename)
{
  return music_info_listed_ext(list, basename, FALSE);
}

static boolean sound_info_listed(struct MusicFileInfo *list, char *basename)
{
  return music_info_listed_ext(list, basename, TRUE);
}

void LoadMusicInfo()
{
  char *music_directory = getCustomMusicDirectory();
  int num_music = getMusicListSize();
  int num_music_noconf = 0;
  int num_sounds = getSoundListSize();
  DIR *dir;
  struct dirent *dir_entry;
  struct FileInfo *music, *sound;
  struct MusicFileInfo *next, **new;
  int i;

  while (music_file_info != NULL)
  {
    next = music_file_info->next;

    checked_free(music_file_info->basename);

    checked_free(music_file_info->title_header);
    checked_free(music_file_info->artist_header);
    checked_free(music_file_info->album_header);
    checked_free(music_file_info->year_header);

    checked_free(music_file_info->title);
    checked_free(music_file_info->artist);
    checked_free(music_file_info->album);
    checked_free(music_file_info->year);

    free(music_file_info);

    music_file_info = next;
  }

  new = &music_file_info;

  for (i = 0; i < num_music; i++)
  {
    music = getMusicListEntry(i);

    if (music->filename == NULL)
      continue;

    if (strEqual(music->filename, UNDEFINED_FILENAME))
      continue;

    /* a configured file may be not recognized as music */
    if (!FileIsMusic(music->filename))
      continue;

#if 0
    printf("::: -> '%s' (configured)\n", music->filename);
#endif

    if (!music_info_listed(music_file_info, music->filename))
    {
      *new = get_music_file_info(music->filename, i);
#if 0
      if (*new != NULL)
	printf(":1: adding '%s' ['%s'] ...\n", (*new)->title, music->filename);
#endif
      if (*new != NULL)
	new = &(*new)->next;
    }
  }

  if ((dir = opendir(music_directory)) == NULL)
  {
    Error(ERR_WARN, "cannot read music directory '%s'", music_directory);
    return;
  }

  while ((dir_entry = readdir(dir)) != NULL)	/* loop until last dir entry */
  {
    char *basename = dir_entry->d_name;
    boolean music_already_used = FALSE;
    int i;

    /* skip all music files that are configured in music config file */
    for (i = 0; i < num_music; i++)
    {
      music = getMusicListEntry(i);

      if (music->filename == NULL)
	continue;

      if (strEqual(basename, music->filename))
      {
	music_already_used = TRUE;
	break;
      }
    }

    if (music_already_used)
      continue;

    if (!FileIsMusic(basename))
      continue;

#if 0
    printf("::: -> '%s' (found in directory)\n", basename);
#endif

    if (!music_info_listed(music_file_info, basename))
    {
      *new = get_music_file_info(basename, MAP_NOCONF_MUSIC(num_music_noconf));
#if 0
      if (*new != NULL)
	printf(":2: adding '%s' ['%s'] ...\n", (*new)->title, basename);
#endif
      if (*new != NULL)
	new = &(*new)->next;
    }

    num_music_noconf++;
  }

  closedir(dir);

  for (i = 0; i < num_sounds; i++)
  {
    sound = getSoundListEntry(i);

    if (sound->filename == NULL)
      continue;

    if (strEqual(sound->filename, UNDEFINED_FILENAME))
      continue;

    /* a configured file may be not recognized as sound */
    if (!FileIsSound(sound->filename))
      continue;

#if 0
    printf("::: -> '%s' (configured)\n", sound->filename);
#endif

    if (!sound_info_listed(music_file_info, sound->filename))
    {
      *new = get_sound_file_info(sound->filename, i);
      if (*new != NULL)
	new = &(*new)->next;
    }
  }

#if 0
  for (next = music_file_info; next != NULL; next = next->next)
    printf("::: title == '%s'\n", next->title);
#endif
}

void add_helpanim_entry(int element, int action, int direction, int delay,
			int *num_list_entries)
{
  struct HelpAnimInfo *new_list_entry;
  (*num_list_entries)++;

  helpanim_info =
    checked_realloc(helpanim_info,
		    *num_list_entries * sizeof(struct HelpAnimInfo));
  new_list_entry = &helpanim_info[*num_list_entries - 1];

  new_list_entry->element = element;
  new_list_entry->action = action;
  new_list_entry->direction = direction;
  new_list_entry->delay = delay;
}

void print_unknown_token(char *filename, char *token, int token_nr)
{
  if (token_nr == 0)
  {
    Error(ERR_INFO_LINE, "-");
    Error(ERR_INFO, "warning: unknown token(s) found in config file:");
    Error(ERR_INFO, "- config file: '%s'", filename);
  }

  Error(ERR_INFO, "- token: '%s'", token);
}

void print_unknown_token_end(int token_nr)
{
  if (token_nr > 0)
    Error(ERR_INFO_LINE, "-");
}

void LoadHelpAnimInfo()
{
  char *filename = getHelpAnimFilename();
  SetupFileList *setup_file_list = NULL, *list;
  SetupFileHash *element_hash, *action_hash, *direction_hash;
  int num_list_entries = 0;
  int num_unknown_tokens = 0;
  int i;

  if (fileExists(filename))
    setup_file_list = loadSetupFileList(filename);

  if (setup_file_list == NULL)
  {
    /* use reliable default values from static configuration */
    SetupFileList *insert_ptr;

    insert_ptr = setup_file_list =
      newSetupFileList(helpanim_config[0].token,
		       helpanim_config[0].value);

    for (i = 1; helpanim_config[i].token; i++)
      insert_ptr = addListEntry(insert_ptr,
				helpanim_config[i].token,
				helpanim_config[i].value);
  }

  element_hash   = newSetupFileHash();
  action_hash    = newSetupFileHash();
  direction_hash = newSetupFileHash();

  for (i = 0; i < MAX_NUM_ELEMENTS; i++)
    setHashEntry(element_hash, element_info[i].token_name, i_to_a(i));

  for (i = 0; i < NUM_ACTIONS; i++)
    setHashEntry(action_hash, element_action_info[i].suffix,
		 i_to_a(element_action_info[i].value));

  /* do not store direction index (bit) here, but direction value! */
  for (i = 0; i < NUM_DIRECTIONS_FULL; i++)
    setHashEntry(direction_hash, element_direction_info[i].suffix,
		 i_to_a(1 << element_direction_info[i].value));

  for (list = setup_file_list; list != NULL; list = list->next)
  {
    char *element_token, *action_token, *direction_token;
    char *element_value, *action_value, *direction_value;
    int delay = atoi(list->value);

    if (strEqual(list->token, "end"))
    {
      add_helpanim_entry(HELPANIM_LIST_NEXT, -1, -1, -1, &num_list_entries);

      continue;
    }

    /* first try to break element into element/action/direction parts;
       if this does not work, also accept combined "element[.act][.dir]"
       elements (like "dynamite.active"), which are unique elements */

    if (strchr(list->token, '.') == NULL)	/* token contains no '.' */
    {
      element_value = getHashEntry(element_hash, list->token);
      if (element_value != NULL)	/* element found */
	add_helpanim_entry(atoi(element_value), -1, -1, delay,
			   &num_list_entries);
      else
      {
	/* no further suffixes found -- this is not an element */
	print_unknown_token(filename, list->token, num_unknown_tokens++);
      }

      continue;
    }

    /* token has format "<prefix>.<something>" */

    action_token = strchr(list->token, '.');	/* suffix may be action ... */
    direction_token = action_token;		/* ... or direction */

    element_token = getStringCopy(list->token);
    *strchr(element_token, '.') = '\0';

    element_value = getHashEntry(element_hash, element_token);

    if (element_value == NULL)		/* this is no element */
    {
      element_value = getHashEntry(element_hash, list->token);
      if (element_value != NULL)	/* combined element found */
	add_helpanim_entry(atoi(element_value), -1, -1, delay,
			   &num_list_entries);
      else
	print_unknown_token(filename, list->token, num_unknown_tokens++);

      free(element_token);

      continue;
    }

    action_value = getHashEntry(action_hash, action_token);

    if (action_value != NULL)		/* action found */
    {
      add_helpanim_entry(atoi(element_value), atoi(action_value), -1, delay,
		    &num_list_entries);

      free(element_token);

      continue;
    }

    direction_value = getHashEntry(direction_hash, direction_token);

    if (direction_value != NULL)	/* direction found */
    {
      add_helpanim_entry(atoi(element_value), -1, atoi(direction_value), delay,
			 &num_list_entries);

      free(element_token);

      continue;
    }

    if (strchr(action_token + 1, '.') == NULL)
    {
      /* no further suffixes found -- this is not an action nor direction */

      element_value = getHashEntry(element_hash, list->token);
      if (element_value != NULL)	/* combined element found */
	add_helpanim_entry(atoi(element_value), -1, -1, delay,
			   &num_list_entries);
      else
	print_unknown_token(filename, list->token, num_unknown_tokens++);

      free(element_token);

      continue;
    }

    /* token has format "<prefix>.<suffix>.<something>" */

    direction_token = strchr(action_token + 1, '.');

    action_token = getStringCopy(action_token);
    *strchr(action_token + 1, '.') = '\0';

    action_value = getHashEntry(action_hash, action_token);

    if (action_value == NULL)		/* this is no action */
    {
      element_value = getHashEntry(element_hash, list->token);
      if (element_value != NULL)	/* combined element found */
	add_helpanim_entry(atoi(element_value), -1, -1, delay,
			   &num_list_entries);
      else
	print_unknown_token(filename, list->token, num_unknown_tokens++);

      free(element_token);
      free(action_token);

      continue;
    }

    direction_value = getHashEntry(direction_hash, direction_token);

    if (direction_value != NULL)	/* direction found */
    {
      add_helpanim_entry(atoi(element_value), atoi(action_value),
			 atoi(direction_value), delay, &num_list_entries);

      free(element_token);
      free(action_token);

      continue;
    }

    /* this is no direction */

    element_value = getHashEntry(element_hash, list->token);
    if (element_value != NULL)		/* combined element found */
      add_helpanim_entry(atoi(element_value), -1, -1, delay,
			 &num_list_entries);
    else
      print_unknown_token(filename, list->token, num_unknown_tokens++);

    free(element_token);
    free(action_token);
  }

  print_unknown_token_end(num_unknown_tokens);

  add_helpanim_entry(HELPANIM_LIST_NEXT, -1, -1, -1, &num_list_entries);
  add_helpanim_entry(HELPANIM_LIST_END,  -1, -1, -1, &num_list_entries);

  freeSetupFileList(setup_file_list);
  freeSetupFileHash(element_hash);
  freeSetupFileHash(action_hash);
  freeSetupFileHash(direction_hash);

#if 0
  for (i = 0; i < num_list_entries; i++)
    printf("::: '%s': %d, %d, %d => %d\n",
	   EL_NAME(helpanim_info[i].element),
	   helpanim_info[i].element,
	   helpanim_info[i].action,
	   helpanim_info[i].direction,
	   helpanim_info[i].delay);
#endif
}

void LoadHelpTextInfo()
{
  char *filename = getHelpTextFilename();
  int i;

  if (helptext_info != NULL)
  {
    freeSetupFileHash(helptext_info);
    helptext_info = NULL;
  }

  if (fileExists(filename))
    helptext_info = loadSetupFileHash(filename);

  if (helptext_info == NULL)
  {
    /* use reliable default values from static configuration */
    helptext_info = newSetupFileHash();

    for (i = 0; helptext_config[i].token; i++)
      setHashEntry(helptext_info,
		   helptext_config[i].token,
		   helptext_config[i].value);
  }

#if 0
  BEGIN_HASH_ITERATION(helptext_info, itr)
  {
    printf("::: '%s' => '%s'\n",
	   HASH_ITERATION_TOKEN(itr), HASH_ITERATION_VALUE(itr));
  }
  END_HASH_ITERATION(hash, itr)
#endif
}


/* ------------------------------------------------------------------------- */
/* convert levels                                                            */
/* ------------------------------------------------------------------------- */

#define MAX_NUM_CONVERT_LEVELS		1000

void ConvertLevels()
{
  static LevelDirTree *convert_leveldir = NULL;
  static int convert_level_nr = -1;
  static int num_levels_handled = 0;
  static int num_levels_converted = 0;
  static boolean levels_failed[MAX_NUM_CONVERT_LEVELS];
  int i;

  convert_leveldir = getTreeInfoFromIdentifier(leveldir_first,
					       global.convert_leveldir);

  if (convert_leveldir == NULL)
    Error(ERR_EXIT, "no such level identifier: '%s'",
	  global.convert_leveldir);

  leveldir_current = convert_leveldir;

  if (global.convert_level_nr != -1)
  {
    convert_leveldir->first_level = global.convert_level_nr;
    convert_leveldir->last_level  = global.convert_level_nr;
  }

  convert_level_nr = convert_leveldir->first_level;

  printf_line("=", 79);
  printf("Converting levels\n");
  printf_line("-", 79);
  printf("Level series identifier: '%s'\n", convert_leveldir->identifier);
  printf("Level series name:       '%s'\n", convert_leveldir->name);
  printf("Level series author:     '%s'\n", convert_leveldir->author);
  printf("Number of levels:        %d\n",   convert_leveldir->levels);
  printf_line("=", 79);
  printf("\n");

  for (i = 0; i < MAX_NUM_CONVERT_LEVELS; i++)
    levels_failed[i] = FALSE;

  while (convert_level_nr <= convert_leveldir->last_level)
  {
    char *level_filename;
    boolean new_level;

    level_nr = convert_level_nr++;

    printf("Level %03d: ", level_nr);

    LoadLevel(level_nr);
    if (level.no_valid_file)
    {
      printf("(no level)\n");
      continue;
    }

    printf("converting level ... ");

    level_filename = getDefaultLevelFilename(level_nr);
    new_level = !fileExists(level_filename);

    if (new_level)
    {
      SaveLevel(level_nr);

      num_levels_converted++;

      printf("converted.\n");
    }
    else
    {
      if (level_nr >= 0 && level_nr < MAX_NUM_CONVERT_LEVELS)
	levels_failed[level_nr] = TRUE;

      printf("NOT CONVERTED -- LEVEL ALREADY EXISTS.\n");
    }

    num_levels_handled++;
  }

  printf("\n");
  printf_line("=", 79);
  printf("Number of levels handled: %d\n", num_levels_handled);
  printf("Number of levels converted: %d (%d%%)\n", num_levels_converted,
	 (num_levels_handled ?
	  num_levels_converted * 100 / num_levels_handled : 0));
  printf_line("-", 79);
  printf("Summary (for automatic parsing by scripts):\n");
  printf("LEVELDIR '%s', CONVERTED %d/%d (%d%%)",
	 convert_leveldir->identifier, num_levels_converted,
	 num_levels_handled,
	 (num_levels_handled ?
	  num_levels_converted * 100 / num_levels_handled : 0));

  if (num_levels_handled != num_levels_converted)
  {
    printf(", FAILED:");
    for (i = 0; i < MAX_NUM_CONVERT_LEVELS; i++)
      if (levels_failed[i])
	printf(" %03d", i);
  }

  printf("\n");
  printf_line("=", 79);

  CloseAllAndExit(0);
}


/* ------------------------------------------------------------------------- */
/* create and save images for use in level sketches (raw BMP format)         */
/* ------------------------------------------------------------------------- */

void CreateLevelSketchImages()
{
#if defined(TARGET_SDL)
  Bitmap *bitmap1;
  Bitmap *bitmap2;
  int i;

  InitElementPropertiesGfxElement();

  bitmap1 = CreateBitmap(TILEX, TILEY, DEFAULT_DEPTH);
  bitmap2 = CreateBitmap(MINI_TILEX, MINI_TILEY, DEFAULT_DEPTH);

  for (i = 0; i < NUM_FILE_ELEMENTS; i++)
  {
    Bitmap *src_bitmap;
    int src_x, src_y;
    int element = getMappedElement(i);
    int graphic = el2edimg(element);
    char basename1[16];
    char basename2[16];
    char *filename1;
    char *filename2;

    sprintf(basename1, "%03d.bmp", i);
    sprintf(basename2, "%03ds.bmp", i);

    filename1 = getPath2(global.create_images_dir, basename1);
    filename2 = getPath2(global.create_images_dir, basename2);

    getGraphicSource(graphic, 0, &src_bitmap, &src_x, &src_y);
    BlitBitmap(src_bitmap, bitmap1, src_x, src_y, TILEX, TILEY, 0, 0);

    if (SDL_SaveBMP(bitmap1->surface, filename1) != 0)
      Error(ERR_EXIT, "cannot save level sketch image file '%s'", filename1);

    getMiniGraphicSource(graphic, &src_bitmap, &src_x, &src_y);
    BlitBitmap(src_bitmap, bitmap2, src_x, src_y, MINI_TILEX, MINI_TILEY, 0, 0);

    if (SDL_SaveBMP(bitmap2->surface, filename2) != 0)
      Error(ERR_EXIT, "cannot save level sketch image file '%s'", filename2);

    free(filename1);
    free(filename2);

    if (options.debug)
      printf("%03d `%03d%c", i, i, (i % 10 < 9 ? ' ' : '\n'));
  }

  FreeBitmap(bitmap1);
  FreeBitmap(bitmap2);

  if (options.debug)
    printf("\n");

  Error(ERR_INFO, "%d normal and small images created", NUM_FILE_ELEMENTS);

  CloseAllAndExit(0);
#endif
}


/* ------------------------------------------------------------------------- */
/* create and save images for custom and group elements (raw BMP format)     */
/* ------------------------------------------------------------------------- */

void CreateCustomElementImages()
{
#if defined(TARGET_SDL)
  char *filename = "graphics.classic/RocksCE.bmp";
  Bitmap *bitmap;
  Bitmap *src_bitmap;
  int dummy_graphic = IMG_CUSTOM_99;
  int yoffset_ce = 0;
  int yoffset_ge = (TILEY * NUM_CUSTOM_ELEMENTS / 16);
  int src_x, src_y;
  int i;

  bitmap = CreateBitmap(TILEX * 16 * 2,
			TILEY * (NUM_CUSTOM_ELEMENTS + NUM_GROUP_ELEMENTS) / 16,
			DEFAULT_DEPTH);

  getGraphicSource(dummy_graphic, 0, &src_bitmap, &src_x, &src_y);

  for (i = 0; i < NUM_CUSTOM_ELEMENTS; i++)
  {
    int x = i % 16;
    int y = i / 16;
    int ii = i + 1;
    int j;

    BlitBitmap(src_bitmap, bitmap, 0, 0, TILEX, TILEY,
	       TILEX * x, TILEY * y + yoffset_ce);

    BlitBitmap(src_bitmap, bitmap, 0, TILEY, TILEX, TILEY,
	       TILEX * x + TILEX * 16, TILEY * y + yoffset_ce);

    for (j = 2; j >= 0; j--)
    {
      int c = ii % 10;

      BlitBitmap(src_bitmap, bitmap, TILEX + c * 7, 0, 6, 10,
		 TILEX * x + 6 + j * 7,
		 TILEY * y + 11 + yoffset_ce);

      BlitBitmap(src_bitmap, bitmap, TILEX + c * 8, TILEY, 6, 10,
		 TILEX * 16 + TILEX * x + 6 + j * 8,
		 TILEY * y + 10 + yoffset_ce);

      ii /= 10;
    }
  }

  for (i = 0; i < NUM_GROUP_ELEMENTS; i++)
  {
    int x = i % 16;
    int y = i / 16;
    int ii = i + 1;
    int j;

    BlitBitmap(src_bitmap, bitmap, 0, 0, TILEX, TILEY,
	       TILEX * x, TILEY * y + yoffset_ge);

    BlitBitmap(src_bitmap, bitmap, 0, TILEY, TILEX, TILEY,
	       TILEX * x + TILEX * 16, TILEY * y + yoffset_ge);

    for (j = 1; j >= 0; j--)
    {
      int c = ii % 10;

      BlitBitmap(src_bitmap, bitmap, TILEX + c * 10, 11, 10, 10,
		 TILEX * x + 6 + j * 10,
		 TILEY * y + 11 + yoffset_ge);

      BlitBitmap(src_bitmap, bitmap, TILEX + c * 8, TILEY + 12, 6, 10,
		 TILEX * 16 + TILEX * x + 10 + j * 8,
		 TILEY * y + 10 + yoffset_ge);

      ii /= 10;
    }
  }

  if (SDL_SaveBMP(bitmap->surface, filename) != 0)
    Error(ERR_EXIT, "cannot save CE graphics file '%s'", filename);

  FreeBitmap(bitmap);

  CloseAllAndExit(0);
#endif
}

#if 0
void CreateLevelSketchImages_TEST()
{
  void CreateCustomElementImages()
}
#endif
