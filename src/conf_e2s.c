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
* conf_e2s.c                                               *
***********************************************************/

/* ----- this file was automatically generated -- do not edit by hand ----- */

#ifndef CONF_E2S_C
#define CONF_E2S_C

/* values for element/sounds mapping configuration */

static struct
{
  int element;
  boolean is_class;
  int action;

  int sound;
}
element_to_sound[] =
{
  {
    EL_DEFAULT, TRUE,				ACTION_DIGGING,
    SND_CLASS_DEFAULT_DIGGING
  },
  {
    EL_DEFAULT, TRUE,				ACTION_COLLECTING,
    SND_CLASS_DEFAULT_COLLECTING
  },
  {
    EL_DEFAULT, TRUE,				ACTION_SNAPPING,
    SND_CLASS_DEFAULT_SNAPPING
  },
  {
    EL_DEFAULT, TRUE,				ACTION_PUSHING,
    SND_CLASS_DEFAULT_PUSHING
  },
  {
    EL_DEFAULT, TRUE,				ACTION_IMPACT,
    SND_CLASS_DEFAULT_IMPACT
  },
  {
    EL_DEFAULT, TRUE,				ACTION_WALKING,
    SND_CLASS_DEFAULT_WALKING
  },
  {
    EL_DEFAULT, TRUE,				ACTION_PASSING,
    SND_CLASS_DEFAULT_PASSING
  },
  {
    EL_DEFAULT, TRUE,				ACTION_DYING,
    SND_CLASS_DEFAULT_DYING
  },
  {
    EL_DEFAULT, TRUE,				ACTION_EXPLODING,
    SND_CLASS_DEFAULT_EXPLODING
  },
  {
    EL_SP_DEFAULT, TRUE,			ACTION_EXPLODING,
    SND_CLASS_SP_DEFAULT_EXPLODING
  },
  {
    EL_BD_DIAMOND, FALSE,			ACTION_COLLECTING,
    SND_BD_DIAMOND_COLLECTING
  },
  {
    EL_BD_DIAMOND, FALSE,			ACTION_IMPACT,
    SND_BD_DIAMOND_IMPACT
  },
  {
    EL_BD_ROCK, FALSE,				ACTION_PUSHING,
    SND_BD_ROCK_PUSHING
  },
  {
    EL_BD_ROCK, FALSE,				ACTION_IMPACT,
    SND_BD_ROCK_IMPACT
  },
  {
    EL_BD_MAGIC_WALL, FALSE,			ACTION_ACTIVATING,
    SND_BD_MAGIC_WALL_ACTIVATING
  },
  {
    EL_BD_MAGIC_WALL_ACTIVE, FALSE,		-1,
    SND_BD_MAGIC_WALL_ACTIVE
  },
  {
    EL_BD_MAGIC_WALL, FALSE,			ACTION_ACTIVE,
    SND_BD_MAGIC_WALL_ACTIVE
  },
  {
    EL_BD_MAGIC_WALL_FILLING, FALSE,		-1,
    SND_BD_MAGIC_WALL_FILLING
  },
  {
    EL_BD_MAGIC_WALL, FALSE,			ACTION_FILLING,
    SND_BD_MAGIC_WALL_FILLING
  },
  {
    EL_BD_AMOEBA, FALSE,			ACTION_WAITING,
    SND_BD_AMOEBA_WAITING
  },
  {
    EL_BD_AMOEBA, FALSE,			ACTION_GROWING,
    SND_BD_AMOEBA_GROWING
  },
  {
    EL_BD_BUTTERFLY, FALSE,			ACTION_MOVING,
    SND_BD_BUTTERFLY_MOVING
  },
  {
    EL_BD_BUTTERFLY, FALSE,			ACTION_WAITING,
    SND_BD_BUTTERFLY_WAITING
  },
  {
    EL_BD_FIREFLY, FALSE,			ACTION_MOVING,
    SND_BD_FIREFLY_MOVING
  },
  {
    EL_BD_FIREFLY, FALSE,			ACTION_WAITING,
    SND_BD_FIREFLY_WAITING
  },
  {
    EL_SP_BASE, FALSE,				ACTION_DIGGING,
    SND_SP_BASE_DIGGING
  },
  {
    EL_SP_BUGGY_BASE, FALSE,			ACTION_DIGGING,
    SND_SP_BUGGY_BASE_DIGGING
  },
  {
    EL_SP_BUGGY_BASE_ACTIVE, FALSE,		-1,
    SND_SP_BUGGY_BASE_ACTIVE
  },
  {
    EL_SP_BUGGY_BASE, FALSE,			ACTION_ACTIVE,
    SND_SP_BUGGY_BASE_ACTIVE
  },
  {
    EL_SP_INFOTRON, FALSE,			ACTION_COLLECTING,
    SND_SP_INFOTRON_COLLECTING
  },
  {
    EL_SP_INFOTRON, FALSE,			ACTION_IMPACT,
    SND_SP_INFOTRON_IMPACT
  },
  {
    EL_SP_ZONK, FALSE,				ACTION_PUSHING,
    SND_SP_ZONK_PUSHING
  },
  {
    EL_SP_ZONK, FALSE,				ACTION_IMPACT,
    SND_SP_ZONK_IMPACT
  },
  {
    EL_SP_DISK_RED, FALSE,			ACTION_COLLECTING,
    SND_SP_DISK_RED_COLLECTING
  },
  {
    EL_SP_DISK_ORANGE, FALSE,			ACTION_PUSHING,
    SND_SP_DISK_ORANGE_PUSHING
  },
  {
    EL_SP_DISK_YELLOW, FALSE,			ACTION_PUSHING,
    SND_SP_DISK_YELLOW_PUSHING
  },
  {
    EL_SP_PORT_RIGHT, TRUE,			ACTION_PASSING,
    SND_CLASS_SP_PORT_PASSING
  },
  {
    EL_SP_EXIT_CLOSED, TRUE,			ACTION_PASSING,
    SND_CLASS_SP_EXIT_PASSING
  },
  {
    EL_SP_EXIT_CLOSED, TRUE,			ACTION_OPENING,
    SND_CLASS_SP_EXIT_OPENING
  },
  {
    EL_SP_EXIT_CLOSED, TRUE,			ACTION_CLOSING,
    SND_CLASS_SP_EXIT_CLOSING
  },
  {
    EL_SP_SNIKSNAK, FALSE,			ACTION_MOVING,
    SND_SP_SNIKSNAK_MOVING
  },
  {
    EL_SP_SNIKSNAK, FALSE,			ACTION_WAITING,
    SND_SP_SNIKSNAK_WAITING
  },
  {
    EL_SP_ELECTRON, FALSE,			ACTION_MOVING,
    SND_SP_ELECTRON_MOVING
  },
  {
    EL_SP_ELECTRON, FALSE,			ACTION_WAITING,
    SND_SP_ELECTRON_WAITING
  },
  {
    EL_SP_TERMINAL, FALSE,			ACTION_ACTIVATING,
    SND_SP_TERMINAL_ACTIVATING
  },
  {
    EL_SP_TERMINAL_ACTIVE, FALSE,		-1,
    SND_SP_TERMINAL_ACTIVE
  },
  {
    EL_SP_TERMINAL, FALSE,			ACTION_ACTIVE,
    SND_SP_TERMINAL_ACTIVE
  },
  {
    EL_SOKOBAN_FIELD_PLAYER, TRUE,		ACTION_PUSHING,
    SND_CLASS_SOKOBAN_PUSHING
  },
  {
    EL_SOKOBAN_FIELD_PLAYER, TRUE,		ACTION_FILLING,
    SND_CLASS_SOKOBAN_FILLING
  },
  {
    EL_SOKOBAN_FIELD_PLAYER, TRUE,		ACTION_EMPTYING,
    SND_CLASS_SOKOBAN_EMPTYING
  },
  {
    EL_PLAYER_1, TRUE,				ACTION_MOVING,
    SND_CLASS_PLAYER_MOVING
  },
  {
    EL_SAND, FALSE,				ACTION_DIGGING,
    SND_SAND_DIGGING
  },
  {
    EL_EMERALD, TRUE,				ACTION_COLLECTING,
    SND_CLASS_EMERALD_COLLECTING
  },
  {
    EL_EMERALD, TRUE,				ACTION_IMPACT,
    SND_CLASS_EMERALD_IMPACT
  },
  {
    EL_DIAMOND, FALSE,				ACTION_COLLECTING,
    SND_DIAMOND_COLLECTING
  },
  {
    EL_DIAMOND, FALSE,				ACTION_IMPACT,
    SND_DIAMOND_IMPACT
  },
  {
    EL_DIAMOND_BREAKING, FALSE,			-1,
    SND_DIAMOND_BREAKING
  },
  {
    EL_DIAMOND, FALSE,				ACTION_BREAKING,
    SND_DIAMOND_BREAKING
  },
  {
    EL_ROCK, FALSE,				ACTION_PUSHING,
    SND_ROCK_PUSHING
  },
  {
    EL_ROCK, FALSE,				ACTION_IMPACT,
    SND_ROCK_IMPACT
  },
  {
    EL_BOMB, FALSE,				ACTION_PUSHING,
    SND_BOMB_PUSHING
  },
  {
    EL_NUT, FALSE,				ACTION_PUSHING,
    SND_NUT_PUSHING
  },
  {
    EL_NUT_BREAKING, FALSE,			-1,
    SND_NUT_BREAKING
  },
  {
    EL_NUT, FALSE,				ACTION_BREAKING,
    SND_NUT_BREAKING
  },
  {
    EL_NUT, FALSE,				ACTION_IMPACT,
    SND_NUT_IMPACT
  },
  {
    EL_DYNAMITE_ACTIVE, TRUE,			ACTION_COLLECTING,
    SND_CLASS_DYNAMITE_COLLECTING
  },
  {
    EL_DYNAMITE_ACTIVE, TRUE,			ACTION_DROPPING,
    SND_CLASS_DYNAMITE_DROPPING
  },
  {
    EL_DYNAMITE_ACTIVE, TRUE,			ACTION_ACTIVE,
    SND_CLASS_DYNAMITE_ACTIVE
  },
  {
    EL_KEY_1, TRUE,				ACTION_COLLECTING,
    SND_CLASS_KEY_COLLECTING
  },
  {
    EL_GATE_1, TRUE,				ACTION_PASSING,
    SND_CLASS_GATE_PASSING
  },
  {
    EL_BUG, FALSE,				ACTION_MOVING,
    SND_BUG_MOVING
  },
  {
    EL_BUG, FALSE,				ACTION_WAITING,
    SND_BUG_WAITING
  },
  {
    EL_SPACESHIP, FALSE,			ACTION_MOVING,
    SND_SPACESHIP_MOVING
  },
  {
    EL_SPACESHIP, FALSE,			ACTION_WAITING,
    SND_SPACESHIP_WAITING
  },
  {
    EL_YAMYAM, FALSE,				ACTION_MOVING,
    SND_YAMYAM_MOVING
  },
  {
    EL_YAMYAM, FALSE,				ACTION_WAITING,
    SND_YAMYAM_WAITING
  },
  {
    EL_YAMYAM, FALSE,				ACTION_DIGGING,
    SND_YAMYAM_DIGGING
  },
  {
    EL_ROBOT, FALSE,				ACTION_MOVING,
    SND_ROBOT_MOVING
  },
  {
    EL_ROBOT, FALSE,				ACTION_WAITING,
    SND_ROBOT_WAITING
  },
  {
    EL_ROBOT_WHEEL, FALSE,			ACTION_ACTIVATING,
    SND_ROBOT_WHEEL_ACTIVATING
  },
  {
    EL_ROBOT_WHEEL_ACTIVE, FALSE,		-1,
    SND_ROBOT_WHEEL_ACTIVE
  },
  {
    EL_ROBOT_WHEEL, FALSE,			ACTION_ACTIVE,
    SND_ROBOT_WHEEL_ACTIVE
  },
  {
    EL_MAGIC_WALL, FALSE,			ACTION_ACTIVATING,
    SND_MAGIC_WALL_ACTIVATING
  },
  {
    EL_MAGIC_WALL_ACTIVE, FALSE,		-1,
    SND_MAGIC_WALL_ACTIVE
  },
  {
    EL_MAGIC_WALL, FALSE,			ACTION_ACTIVE,
    SND_MAGIC_WALL_ACTIVE
  },
  {
    EL_MAGIC_WALL_FILLING, FALSE,		-1,
    SND_MAGIC_WALL_FILLING
  },
  {
    EL_MAGIC_WALL, FALSE,			ACTION_FILLING,
    SND_MAGIC_WALL_FILLING
  },
  {
    EL_DC_MAGIC_WALL, FALSE,			ACTION_ACTIVATING,
    SND_DC_MAGIC_WALL_ACTIVATING
  },
  {
    EL_DC_MAGIC_WALL_ACTIVE, FALSE,		-1,
    SND_DC_MAGIC_WALL_ACTIVE
  },
  {
    EL_DC_MAGIC_WALL, FALSE,			ACTION_ACTIVE,
    SND_DC_MAGIC_WALL_ACTIVE
  },
  {
    EL_DC_MAGIC_WALL_FILLING, FALSE,		-1,
    SND_DC_MAGIC_WALL_FILLING
  },
  {
    EL_DC_MAGIC_WALL, FALSE,			ACTION_FILLING,
    SND_DC_MAGIC_WALL_FILLING
  },
  {
    EL_AMOEBA_DEAD, TRUE,			ACTION_WAITING,
    SND_CLASS_AMOEBA_WAITING
  },
  {
    EL_AMOEBA_DEAD, TRUE,			ACTION_GROWING,
    SND_CLASS_AMOEBA_GROWING
  },
  {
    EL_AMOEBA_DEAD, TRUE,			ACTION_DROPPING,
    SND_CLASS_AMOEBA_DROPPING
  },
  {
    EL_ACID, FALSE,				ACTION_SPLASHING,
    SND_ACID_SPLASHING
  },
  {
    EL_QUICKSAND_EMPTY, TRUE,			ACTION_FILLING,
    SND_CLASS_QUICKSAND_FILLING
  },
  {
    EL_QUICKSAND_EMPTY, TRUE,			ACTION_EMPTYING,
    SND_CLASS_QUICKSAND_EMPTYING
  },
  {
    EL_EXIT_CLOSED, TRUE,			ACTION_OPENING,
    SND_CLASS_EXIT_OPENING
  },
  {
    EL_EXIT_CLOSED, TRUE,			ACTION_CLOSING,
    SND_CLASS_EXIT_CLOSING
  },
  {
    EL_EXIT_CLOSED, TRUE,			ACTION_PASSING,
    SND_CLASS_EXIT_PASSING
  },
  {
    EL_STEEL_EXIT_CLOSED, TRUE,			ACTION_OPENING,
    SND_CLASS_STEEL_EXIT_OPENING
  },
  {
    EL_STEEL_EXIT_CLOSED, TRUE,			ACTION_CLOSING,
    SND_CLASS_STEEL_EXIT_CLOSING
  },
  {
    EL_STEEL_EXIT_CLOSED, TRUE,			ACTION_PASSING,
    SND_CLASS_STEEL_EXIT_PASSING
  },
  {
    EL_EM_EXIT_CLOSED, TRUE,			ACTION_OPENING,
    SND_CLASS_EM_EXIT_OPENING
  },
  {
    EL_EM_EXIT_CLOSED, TRUE,			ACTION_CLOSING,
    SND_CLASS_EM_EXIT_CLOSING
  },
  {
    EL_EM_EXIT_CLOSED, TRUE,			ACTION_PASSING,
    SND_CLASS_EM_EXIT_PASSING
  },
  {
    EL_EM_STEEL_EXIT_CLOSED, TRUE,		ACTION_OPENING,
    SND_CLASS_EM_STEEL_EXIT_OPENING
  },
  {
    EL_EM_STEEL_EXIT_CLOSED, TRUE,		ACTION_CLOSING,
    SND_CLASS_EM_STEEL_EXIT_CLOSING
  },
  {
    EL_EM_STEEL_EXIT_CLOSED, TRUE,		ACTION_PASSING,
    SND_CLASS_EM_STEEL_EXIT_PASSING
  },
  {
    EL_PENGUIN, FALSE,				ACTION_PASSING,
    SND_PENGUIN_PASSING
  },
  {
    EL_BALLOON, FALSE,				ACTION_MOVING,
    SND_BALLOON_MOVING
  },
  {
    EL_BALLOON, FALSE,				ACTION_WAITING,
    SND_BALLOON_WAITING
  },
  {
    EL_BALLOON, FALSE,				ACTION_PUSHING,
    SND_BALLOON_PUSHING
  },
  {
    EL_BALLOON_SWITCH_LEFT, TRUE,		ACTION_ACTIVATING,
    SND_CLASS_BALLOON_SWITCH_ACTIVATING
  },
  {
    EL_SPRING, FALSE,				ACTION_MOVING,
    SND_SPRING_MOVING
  },
  {
    EL_SPRING, FALSE,				ACTION_PUSHING,
    SND_SPRING_PUSHING
  },
  {
    EL_SPRING, FALSE,				ACTION_IMPACT,
    SND_SPRING_IMPACT
  },
  {
    EL_WALL, TRUE,				ACTION_GROWING,
    SND_CLASS_WALL_GROWING
  },
  {
    EL_EMC_ANDROID, FALSE,			ACTION_PUSHING,
    SND_EMC_ANDROID_PUSHING
  },
  {
    EL_EMC_ANDROID, FALSE,			ACTION_MOVING,
    SND_EMC_ANDROID_MOVING
  },
  {
    EL_EMC_ANDROID, FALSE,			ACTION_DROPPING,
    SND_EMC_ANDROID_DROPPING
  },
  {
    EL_EMC_MAGIC_BALL, FALSE,			ACTION_DROPPING,
    SND_EMC_MAGIC_BALL_DROPPING
  },
  {
    EL_PEARL, FALSE,				ACTION_COLLECTING,
    SND_PEARL_COLLECTING
  },
  {
    EL_PEARL_BREAKING, FALSE,			-1,
    SND_PEARL_BREAKING
  },
  {
    EL_PEARL, FALSE,				ACTION_BREAKING,
    SND_PEARL_BREAKING
  },
  {
    EL_PEARL, FALSE,				ACTION_IMPACT,
    SND_PEARL_IMPACT
  },
  {
    EL_CRYSTAL, FALSE,				ACTION_COLLECTING,
    SND_CRYSTAL_COLLECTING
  },
  {
    EL_CRYSTAL, FALSE,				ACTION_IMPACT,
    SND_CRYSTAL_IMPACT
  },
  {
    EL_ENVELOPE_1, TRUE,			ACTION_COLLECTING,
    SND_CLASS_ENVELOPE_COLLECTING
  },
  {
    EL_ENVELOPE_1, TRUE,			ACTION_OPENING,
    SND_CLASS_ENVELOPE_OPENING
  },
  {
    EL_ENVELOPE_1, TRUE,			ACTION_CLOSING,
    SND_CLASS_ENVELOPE_CLOSING
  },
  {
    EL_INVISIBLE_SAND, FALSE,			ACTION_DIGGING,
    SND_INVISIBLE_SAND_DIGGING
  },
  {
    EL_INVISIBLE_SAND_ACTIVE, FALSE,		ACTION_DIGGING,
    SND_INVISIBLE_SAND_ACTIVE_DIGGING
  },
  {
    EL_SHIELD_NORMAL, FALSE,			ACTION_COLLECTING,
    SND_SHIELD_NORMAL_COLLECTING
  },
  {
    EL_SHIELD_NORMAL_ACTIVE, FALSE,		-1,
    SND_SHIELD_NORMAL_ACTIVE
  },
  {
    EL_SHIELD_NORMAL, FALSE,			ACTION_ACTIVE,
    SND_SHIELD_NORMAL_ACTIVE
  },
  {
    EL_SHIELD_DEADLY, FALSE,			ACTION_COLLECTING,
    SND_SHIELD_DEADLY_COLLECTING
  },
  {
    EL_SHIELD_DEADLY_ACTIVE, FALSE,		-1,
    SND_SHIELD_DEADLY_ACTIVE
  },
  {
    EL_SHIELD_DEADLY, FALSE,			ACTION_ACTIVE,
    SND_SHIELD_DEADLY_ACTIVE
  },
  {
    EL_EXTRA_TIME, FALSE,			ACTION_COLLECTING,
    SND_EXTRA_TIME_COLLECTING
  },
  {
    EL_MOLE, FALSE,				ACTION_MOVING,
    SND_MOLE_MOVING
  },
  {
    EL_MOLE, FALSE,				ACTION_WAITING,
    SND_MOLE_WAITING
  },
  {
    EL_MOLE, FALSE,				ACTION_DIGGING,
    SND_MOLE_DIGGING
  },
  {
    EL_SWITCHGATE_SWITCH_UP, TRUE,		ACTION_ACTIVATING,
    SND_CLASS_SWITCHGATE_SWITCH_ACTIVATING
  },
  {
    EL_SWITCHGATE_OPEN, TRUE,			ACTION_OPENING,
    SND_CLASS_SWITCHGATE_OPENING
  },
  {
    EL_SWITCHGATE_OPEN, TRUE,			ACTION_CLOSING,
    SND_CLASS_SWITCHGATE_CLOSING
  },
  {
    EL_SWITCHGATE_OPEN, TRUE,			ACTION_PASSING,
    SND_CLASS_SWITCHGATE_PASSING
  },
  {
    EL_TIMEGATE_SWITCH_ACTIVE, TRUE,		ACTION_ACTIVATING,
    SND_CLASS_TIMEGATE_SWITCH_ACTIVATING
  },
  {
    EL_TIMEGATE_SWITCH_ACTIVE, TRUE,		ACTION_ACTIVE,
    SND_CLASS_TIMEGATE_SWITCH_ACTIVE
  },
  {
    EL_TIMEGATE_SWITCH_ACTIVE, TRUE,		ACTION_DEACTIVATING,
    SND_CLASS_TIMEGATE_SWITCH_DEACTIVATING
  },
  {
    EL_TIMEGATE_OPEN, TRUE,			ACTION_OPENING,
    SND_CLASS_TIMEGATE_OPENING
  },
  {
    EL_TIMEGATE_OPEN, TRUE,			ACTION_CLOSING,
    SND_CLASS_TIMEGATE_CLOSING
  },
  {
    EL_TIMEGATE_OPEN, TRUE,			ACTION_PASSING,
    SND_CLASS_TIMEGATE_PASSING
  },
  {
    EL_CONVEYOR_BELT_1_SWITCH_LEFT, TRUE,	ACTION_ACTIVATING,
    SND_CLASS_CONVEYOR_BELT_SWITCH_ACTIVATING
  },
  {
    EL_CONVEYOR_BELT_1_LEFT, TRUE,		ACTION_ACTIVE,
    SND_CLASS_CONVEYOR_BELT_ACTIVE
  },
  {
    EL_CONVEYOR_BELT_1_SWITCH_LEFT, TRUE,	ACTION_DEACTIVATING,
    SND_CLASS_CONVEYOR_BELT_SWITCH_DEACTIVATING
  },
  {
    EL_LIGHT_SWITCH, FALSE,			ACTION_ACTIVATING,
    SND_LIGHT_SWITCH_ACTIVATING
  },
  {
    EL_LIGHT_SWITCH, FALSE,			ACTION_DEACTIVATING,
    SND_LIGHT_SWITCH_DEACTIVATING
  },
  {
    EL_DX_SUPABOMB, FALSE,			ACTION_PUSHING,
    SND_DX_SUPABOMB_PUSHING
  },
  {
    EL_TRAP, FALSE,				ACTION_DIGGING,
    SND_TRAP_DIGGING
  },
  {
    EL_TRAP, FALSE,				ACTION_ACTIVATING,
    SND_TRAP_ACTIVATING
  },
  {
    EL_TUBE_ANY, TRUE,				ACTION_WALKING,
    SND_CLASS_TUBE_WALKING
  },
  {
    EL_SPEED_PILL, FALSE,			ACTION_COLLECTING,
    SND_SPEED_PILL_COLLECTING
  },
  {
    EL_DYNABOMB_INCREASE_NUMBER, FALSE,		ACTION_COLLECTING,
    SND_DYNABOMB_INCREASE_NUMBER_COLLECTING
  },
  {
    EL_DYNABOMB_INCREASE_SIZE, FALSE,		ACTION_COLLECTING,
    SND_DYNABOMB_INCREASE_SIZE_COLLECTING
  },
  {
    EL_DYNABOMB_INCREASE_POWER, FALSE,		ACTION_COLLECTING,
    SND_DYNABOMB_INCREASE_POWER_COLLECTING
  },
  {
    EL_DYNABOMB_INCREASE_NUMBER, TRUE,		ACTION_DROPPING,
    SND_CLASS_DYNABOMB_DROPPING
  },
  {
    EL_DYNABOMB_INCREASE_NUMBER, TRUE,		ACTION_ACTIVE,
    SND_CLASS_DYNABOMB_ACTIVE
  },
  {
    EL_SATELLITE, FALSE,			ACTION_MOVING,
    SND_SATELLITE_MOVING
  },
  {
    EL_SATELLITE, FALSE,			ACTION_WAITING,
    SND_SATELLITE_WAITING
  },
  {
    EL_SATELLITE, FALSE,			ACTION_PUSHING,
    SND_SATELLITE_PUSHING
  },
  {
    EL_LAMP, FALSE,				ACTION_ACTIVATING,
    SND_LAMP_ACTIVATING
  },
  {
    EL_LAMP, FALSE,				ACTION_DEACTIVATING,
    SND_LAMP_DEACTIVATING
  },
  {
    EL_TIME_ORB_FULL, FALSE,			ACTION_COLLECTING,
    SND_TIME_ORB_FULL_COLLECTING
  },
  {
    EL_TIME_ORB_FULL, FALSE,			ACTION_IMPACT,
    SND_TIME_ORB_FULL_IMPACT
  },
  {
    EL_TIME_ORB_EMPTY, FALSE,			ACTION_PUSHING,
    SND_TIME_ORB_EMPTY_PUSHING
  },
  {
    EL_TIME_ORB_EMPTY, FALSE,			ACTION_IMPACT,
    SND_TIME_ORB_EMPTY_IMPACT
  },
  {
    EL_GAME_OF_LIFE, FALSE,			ACTION_WAITING,
    SND_GAME_OF_LIFE_WAITING
  },
  {
    EL_GAME_OF_LIFE, FALSE,			ACTION_GROWING,
    SND_GAME_OF_LIFE_GROWING
  },
  {
    EL_BIOMAZE, FALSE,				ACTION_WAITING,
    SND_BIOMAZE_WAITING
  },
  {
    EL_BIOMAZE, FALSE,				ACTION_GROWING,
    SND_BIOMAZE_GROWING
  },
  {
    EL_PACMAN, FALSE,				ACTION_MOVING,
    SND_PACMAN_MOVING
  },
  {
    EL_PACMAN, FALSE,				ACTION_WAITING,
    SND_PACMAN_WAITING
  },
  {
    EL_PACMAN, FALSE,				ACTION_DIGGING,
    SND_PACMAN_DIGGING
  },
  {
    EL_DARK_YAMYAM, FALSE,			ACTION_MOVING,
    SND_DARK_YAMYAM_MOVING
  },
  {
    EL_DARK_YAMYAM, FALSE,			ACTION_WAITING,
    SND_DARK_YAMYAM_WAITING
  },
  {
    EL_DARK_YAMYAM, FALSE,			ACTION_DIGGING,
    SND_DARK_YAMYAM_DIGGING
  },
  {
    EL_PENGUIN, FALSE,				ACTION_MOVING,
    SND_PENGUIN_MOVING
  },
  {
    EL_PENGUIN, FALSE,				ACTION_WAITING,
    SND_PENGUIN_WAITING
  },
  {
    EL_PIG, FALSE,				ACTION_MOVING,
    SND_PIG_MOVING
  },
  {
    EL_PIG, FALSE,				ACTION_WAITING,
    SND_PIG_WAITING
  },
  {
    EL_PIG, FALSE,				ACTION_DIGGING,
    SND_PIG_DIGGING
  },
  {
    EL_DRAGON, FALSE,				ACTION_MOVING,
    SND_DRAGON_MOVING
  },
  {
    EL_DRAGON, FALSE,				ACTION_WAITING,
    SND_DRAGON_WAITING
  },
  {
    EL_DRAGON, FALSE,				ACTION_ATTACKING,
    SND_DRAGON_ATTACKING
  },
  {
    -1, -1,					-1,
    -1
  },
};

#endif	/* CONF_E2S_C */
