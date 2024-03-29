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
* conf_snd.c                                               *
***********************************************************/

#include "libgame/libgame.h"
#include "main.h"


/* List values that are not defined in the configuration file are set to
   reliable default values. If that value is SND_ARG_UNDEFINED, it will
   be dynamically determined, using some of the other list values. */

struct ConfigTypeInfo sound_config_suffix[] =
{
  { ".mode_loop",			ARG_UNDEFINED,	TYPE_BOOLEAN	},
  { ".volume",				"100",		TYPE_INTEGER	},
  { ".priority",			"0",		TYPE_INTEGER	},

  { NULL,				NULL,		0		}
};

struct ConfigInfo sound_config[] =
{
  /* some default sounds */
  { "[default].digging",		"schlurf.wav"			},
  { "[default].collecting",		"pong.wav"			},
  { "[default].snapping",		"pong.wav"			},
  { "[default].pushing",		"pusch.wav"			},
  { "[default].impact",			"klopf.wav"			},
  { "[default].walking",		"empty.wav"			},
  { "[default].passing",		"gate.wav"			},
  { "[default].dying",			"autsch.wav"			},
  { "[default].exploding",		"roaaar.wav"			},
  { "[sp_default].exploding",		"booom.wav"			},

  /* sounds for Boulder Dash style elements and actions */
  { "bd_diamond.collecting",		"pong.wav"			},
  { "bd_diamond.impact",		"pling.wav"			},
  { "bd_rock.pushing",			"pusch.wav"			},
  { "bd_rock.impact",			"klopf.wav"			},
  { "bd_magic_wall.activating",		"quirk.wav"			},
  { "bd_magic_wall.active",		"miep.wav"			},
  { "bd_magic_wall.filling",		"quirk.wav"			},
  { "bd_amoeba.waiting",		UNDEFINED_FILENAME		},
  { "bd_amoeba.growing",		"amoebe.wav"			},
  { "bd_amoeba.turning_to_gem",		"pling.wav"			},
  { "bd_amoeba.turning_to_rock",	"klopf.wav"			},
  { "bd_butterfly.moving",		"klapper.wav"			},
  { "bd_butterfly.waiting",		"klapper.wav"			},
  { "bd_firefly.moving",		"roehr.wav"			},
  { "bd_firefly.waiting",		"roehr.wav"			},

  /* sounds for Supaplex style elements and actions */
  { "sp_base.digging",			"base.wav"			},
  { "sp_buggy_base.digging",		"base.wav"			},
  { "sp_buggy_base.active",		"bug.wav"			},
  { "sp_infotron.collecting",		"infotron.wav"			},
  { "sp_infotron.impact",		"pling.wav"			},
  { "sp_zonk.pushing",			"zonkpush.wav"			},
  { "sp_zonk.impact",			"zonkdown.wav"			},
  { "sp_disk_red.collecting",		"infotron.wav"			},
  { "sp_disk_orange.pushing",		"zonkpush.wav"			},
  { "sp_disk_yellow.pushing",		"pusch.wav"			},
  { "[sp_port].passing",		"gate.wav"			},
  { "[sp_exit].passing",		"exit.wav"			},
  { "[sp_exit].opening",		UNDEFINED_FILENAME		},
  { "[sp_exit].closing",		UNDEFINED_FILENAME		},
  { "sp_sniksnak.moving",		UNDEFINED_FILENAME		},
  { "sp_sniksnak.waiting",		UNDEFINED_FILENAME		},
  { "sp_electron.moving",		UNDEFINED_FILENAME		},
  { "sp_electron.waiting",		UNDEFINED_FILENAME		},
  { "sp_terminal.activating",		UNDEFINED_FILENAME		},
  { "sp_terminal.active",		UNDEFINED_FILENAME		},

  /* sounds for Sokoban style elements and actions */
  { "[sokoban].pushing",		"pusch.wav"			},
  { "[sokoban].filling",		"deng.wav"			},
  { "[sokoban].emptying",		UNDEFINED_FILENAME		},

  /* sounds for Emerald Mine style elements and actions */
  { "[player].moving",			"empty.wav"			},
  { "[player].moving.mode_loop",	"false"				},
  { "sand.digging",			"schlurf.wav"			},
  { "[emerald].collecting",		"pong.wav"			},
  { "[emerald].impact",			"pling.wav"			},
  { "diamond.collecting",		"pong.wav"			},
  { "diamond.impact",			"pling.wav"			},
  { "diamond.breaking",			"quirk.wav"			},
  { "rock.pushing",			"pusch.wav"			},
  { "rock.impact",			"klopf.wav"			},
  { "bomb.pushing",			"pusch.wav"			},
  { "nut.pushing",			"knurk.wav"			},
  { "nut.breaking",			"knack.wav"			},
  { "nut.impact",			"klumpf.wav"			},
  { "[dynamite].collecting",		"pong.wav"			},
  { "[dynamite].dropping",		"deng.wav"			},
  { "[dynamite].active",		"zisch.wav"			},
  { "[key].collecting",			"pong.wav"			},
  { "[gate].passing",			"gate.wav"			},
  { "bug.moving",			"klapper.wav"			},
  { "bug.waiting",			"klapper.wav"			},
  { "spaceship.moving",			"roehr.wav"			},
  { "spaceship.waiting",		"roehr.wav"			},
  { "yamyam.moving",			UNDEFINED_FILENAME		},
  { "yamyam.waiting",			"njam.wav"			},
  { "yamyam.digging",			"njam.wav"			},
  { "robot.moving",			"schlurf.wav"			},
  { "robot.moving.mode_loop",		"false"				},
  { "robot.waiting",			UNDEFINED_FILENAME		},
  { "robot_wheel.activating",		"deng.wav"			},
  { "robot_wheel.active",		"miep.wav"			},
  { "magic_wall.activating",		"quirk.wav"			},
  { "magic_wall.active",		"miep.wav"			},
  { "magic_wall.filling",		"quirk.wav"			},
  { "dc_magic_wall.activating",		"quirk.wav"			},
  { "dc_magic_wall.active",		"miep.wav"			},
  { "dc_magic_wall.filling",		"quirk.wav"			},
  { "[amoeba].waiting",			UNDEFINED_FILENAME		},
  { "[amoeba].growing",			"amoebe.wav"			},
  { "[amoeba].dropping",		UNDEFINED_FILENAME		},
  { "acid.splashing",			"blurb.wav"			},
  { "[quicksand].filling",		UNDEFINED_FILENAME		},
  { "[quicksand].emptying",		UNDEFINED_FILENAME		},
  { "[exit].opening",			"oeffnen.wav"			},
  { "[exit].closing",			"oeffnen.wav"			},
  { "[exit].passing",			"buing.wav"			},
  { "[steel_exit].opening",		"oeffnen.wav"			},
  { "[steel_exit].closing",		"oeffnen.wav"			},
  { "[steel_exit].passing",		"buing.wav"			},
  { "[em_exit].opening",		"gong.wav"			},
  { "[em_exit].closing",		UNDEFINED_FILENAME		},
  { "[em_exit].passing",		"buing.wav"			},
  { "[em_steel_exit].opening",		"gong.wav"			},
  { "[em_steel_exit].closing",		UNDEFINED_FILENAME		},
  { "[em_steel_exit].passing",		"buing.wav"			},
  { "penguin.passing",			"buing.wav"			},

  /* sounds for Emerald Mine Club style elements and actions */
  { "balloon.moving",			UNDEFINED_FILENAME		},
  { "balloon.waiting",			UNDEFINED_FILENAME		},
  { "balloon.pushing",			"schlurf.wav"			},
  { "[balloon_switch].activating",	UNDEFINED_FILENAME		},
  { "spring.moving",			UNDEFINED_FILENAME		},
  { "spring.pushing",			"pusch.wav"			},
  { "spring.impact",			"klopf.wav"			},
  { "[wall].growing",			UNDEFINED_FILENAME		},
  { "emc_android.pushing",		"pusch.wav"			},
  { "emc_android.moving",		"roehr.wav"			},
  { "emc_android.moving.mode_loop",	"false"				},
  { "emc_android.dropping",		"deng.wav"			},
  { "emc_magic_ball.dropping",		"deng.wav"			},

  /* sounds for Diamond Caves style elements and actions */
  { "pearl.collecting",			"pong.wav"			},
  { "pearl.breaking",			"knack.wav"			},
  { "pearl.impact",			"pling.wav"			},
  { "crystal.collecting",		"pong.wav"			},
  { "crystal.impact",			"pling.wav"			},
  { "[envelope].collecting",		"pong.wav"			},
  { "[envelope].opening",		UNDEFINED_FILENAME		},
  { "[envelope].closing",		UNDEFINED_FILENAME		},
  { "invisible_sand.digging",		"schlurf.wav"			},
  { "invisible_sand.active.digging",	"schlurf.wav"			},
  { "shield_normal.collecting",		"pong.wav"			},
  { "shield_normal.active",		UNDEFINED_FILENAME		},
  { "shield_deadly.collecting",		"pong.wav"			},
  { "shield_deadly.active",		UNDEFINED_FILENAME		},
  { "extra_time.collecting",		"gong.wav"			},
  { "mole.moving",			UNDEFINED_FILENAME		},
  { "mole.waiting",			UNDEFINED_FILENAME		},
  { "mole.digging",			"blurb.wav"			},
  { "[switchgate_switch].activating",	UNDEFINED_FILENAME		},
  { "[switchgate].opening",		"oeffnen.wav"			},
  { "[switchgate].closing",		"oeffnen.wav"			},
  { "[switchgate].passing",		"gate.wav"			},
  { "[timegate_switch].activating",	"deng.wav"			},
  { "[timegate_switch].active",		"miep.wav"			},
  { "[timegate_switch].deactivating",	UNDEFINED_FILENAME		},
  { "[timegate].opening",		"oeffnen.wav"			},
  { "[timegate].closing",		"oeffnen.wav"			},
  { "[timegate].passing",		"gate.wav"			},
  { "[conveyor_belt_switch].activating",UNDEFINED_FILENAME		},
  { "[conveyor_belt].active",		UNDEFINED_FILENAME		},
  { "[conveyor_belt_switch].deactivating",UNDEFINED_FILENAME		},
  { "light_switch.activating",		UNDEFINED_FILENAME		},
  { "light_switch.deactivating",	UNDEFINED_FILENAME		},

  /* sounds for DX Boulderdash style elements and actions */
  { "dx_supabomb.pushing",		"pusch.wav"			},
  { "trap.digging",			"schlurf.wav"			},
  { "trap.activating",			UNDEFINED_FILENAME		},
  { "[tube].walking",			UNDEFINED_FILENAME		},

  /* sounds for Rocks'n'Diamonds style elements and actions */
  { "amoeba.turning_to_gem",		"pling.wav"			},
  { "amoeba.turning_to_rock",		"klopf.wav"			},
  { "speed_pill.collecting",		"pong.wav"			},
  { "dynabomb_increase_number.collecting","pong.wav"			},
  { "dynabomb_increase_size.collecting","pong.wav"			},
  { "dynabomb_increase_power.collecting","pong.wav"			},
  { "[dynabomb].dropping",		"deng.wav"			},
  { "[dynabomb].active",		"zisch.wav"			},
  { "satellite.moving",			UNDEFINED_FILENAME		},
  { "satellite.waiting",		UNDEFINED_FILENAME		},
  { "satellite.pushing",		"pusch.wav"			},
  { "lamp.activating",			"deng.wav"			},
  { "lamp.deactivating",		"deng.wav"			},
  { "time_orb_full.collecting",		"gong.wav"			},
  { "time_orb_full.impact",		"deng.wav"			},
  { "time_orb_empty.pushing",		"pusch.wav"			},
  { "time_orb_empty.impact",		"deng.wav"			},
  { "game_of_life.waiting",		UNDEFINED_FILENAME		},
  { "game_of_life.growing",		"amoebe.wav"			},
  { "biomaze.waiting",			UNDEFINED_FILENAME		},
  { "biomaze.growing",			"amoebe.wav"			},
  { "pacman.moving",			UNDEFINED_FILENAME		},
  { "pacman.waiting",			UNDEFINED_FILENAME		},
  { "pacman.digging",			UNDEFINED_FILENAME		},
  { "dark_yamyam.moving",		UNDEFINED_FILENAME		},
  { "dark_yamyam.waiting",		"njam.wav"			},
  { "dark_yamyam.digging",		UNDEFINED_FILENAME		},
  { "penguin.moving",			UNDEFINED_FILENAME		},
  { "penguin.waiting",			UNDEFINED_FILENAME		},
  { "pig.moving",			UNDEFINED_FILENAME		},
  { "pig.waiting",			UNDEFINED_FILENAME		},
  { "pig.digging",			UNDEFINED_FILENAME		},
  { "dragon.moving",			UNDEFINED_FILENAME		},
  { "dragon.waiting",			UNDEFINED_FILENAME		},
  { "dragon.attacking",			UNDEFINED_FILENAME		},

  /* sounds not associated to game elements (used for menu screens etc.) */
  /* keyword to stop parser: "NO_MORE_ELEMENT_SOUNDS" <-- do not change! */

  /* sounds for other game actions */
  { "game.starting",			UNDEFINED_FILENAME		},
  { "game.running_out_of_time",		"gong.wav"			},
  { "game.leveltime_bonus",		"sirr.wav"			},
  { "game.losing",			"lachen.wav"			},
  { "game.winning",			UNDEFINED_FILENAME		},
  { "game.sokoban_solving",		"buing.wav"			},

  /* sounds for other non-game actions */
  { "door.opening",			"oeffnen.wav"			},
  { "door.closing",			"oeffnen.wav"			},

  /* sounds for menu actions */
  { "menu.item.activating",		"empty.wav"			},
  { "menu.item.selecting",		"base.wav"			},

  { "background.TITLE_INITIAL",		UNDEFINED_FILENAME		},
  { "background.TITLE",			UNDEFINED_FILENAME		},
  { "background.MAIN",			UNDEFINED_FILENAME		},
  { "background.LEVELS",		UNDEFINED_FILENAME		},
  { "background.SCORES",		"halloffame.wav"		},
  { "background.SCORES.mode_loop",	"false"				},
  { "background.EDITOR",		UNDEFINED_FILENAME		},
  { "background.INFO",			UNDEFINED_FILENAME		},
  { "background.SETUP",			UNDEFINED_FILENAME		},

  { "background.titlescreen_initial_1",	UNDEFINED_FILENAME		},
  { "background.titlescreen_initial_2",	UNDEFINED_FILENAME		},
  { "background.titlescreen_initial_3",	UNDEFINED_FILENAME		},
  { "background.titlescreen_initial_4",	UNDEFINED_FILENAME		},
  { "background.titlescreen_initial_5",	UNDEFINED_FILENAME		},
  { "background.titlescreen_1",		UNDEFINED_FILENAME		},
  { "background.titlescreen_2",		UNDEFINED_FILENAME		},
  { "background.titlescreen_3",		UNDEFINED_FILENAME		},
  { "background.titlescreen_4",		UNDEFINED_FILENAME		},
  { "background.titlescreen_5",		UNDEFINED_FILENAME		},
  { "background.titlemessage_initial_1",UNDEFINED_FILENAME		},
  { "background.titlemessage_initial_2",UNDEFINED_FILENAME		},
  { "background.titlemessage_initial_3",UNDEFINED_FILENAME		},
  { "background.titlemessage_initial_4",UNDEFINED_FILENAME		},
  { "background.titlemessage_initial_5",UNDEFINED_FILENAME		},
  { "background.titlemessage_1",	UNDEFINED_FILENAME		},
  { "background.titlemessage_2",	UNDEFINED_FILENAME		},
  { "background.titlemessage_3",	UNDEFINED_FILENAME		},
  { "background.titlemessage_4",	UNDEFINED_FILENAME		},
  { "background.titlemessage_5",	UNDEFINED_FILENAME		},

#if 0
  { "[not used]",			"antigrav.wav"			},
  { "[not used]",			"bong.wav"			},
  { "[not used]",		 	"fuel.wav"			},
  { "[not used]",			"holz.wav"			},
  { "[not used]",			"hui.wav"			},
  { "[not used]",			"kabumm.wav"			},
  { "[not used]",			"kink.wav"			},
  { "[not used]",			"kling.wav"			},
  { "[not used]",			"krach.wav"			},
  { "[not used]",			"laser.wav"			},
  { "[not used]",			"quiek.wav"			},
  { "[not used]",			"rumms.wav"			},
  { "[not used]",			"schlopp.wav"			},
  { "[not used]",			"schrff.wav"			},
  { "[not used]",			"schwirr.wav"			},
  { "[not used]",			"slurp.wav"			},
  { "[not used]",			"sproing.wav"			},
  { "[not used]",			"warnton.wav"			},
  { "[not used]",			"whoosh.wav"			},
  { "[not used]",			"boom.wav"			},
#endif

  { NULL,				NULL				}
};
