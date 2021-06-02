#include "MeleeCodes.h"

#include "../../kernel/gecko/g_ucf.h" // UCF codeset
#include "../../kernel/gecko/g_ucf_stealth.h" // UCF Stealth

#include "../../kernel/gecko/g_pal.h" // PAL codeset

#include "../../kernel/gecko/g_mods_stealth.h" // Mods: Stealth
#include "../../kernel/gecko/g_mods_tournament.h" // Mods: Tournament
#include "../../kernel/gecko/g_mods_friendlies.h" // Mods: Friendlies

#include "../../kernel/gecko/g_lag_pd.h" // Lag Reduction: PD
#include "../../kernel/gecko/g_lag_pdvb.h" // Lag Reduction: PD+VB

#include "../../kernel/gecko/g_stages_stadium.h" // Stages: Frozen Pokemon
#include "../../kernel/gecko/g_stages_all.h" // Stages: Frozen Pokemon

#include "../../kernel/gecko/g_gameplay_lgl.h" // Gameplay: LGL
#include "../../kernel/gecko/g_gameplay_wobbling.h" // Gameplay: Wobbling
#include "../../kernel/gecko/g_gameplay_both.h" // Gameplay: Both

#include "../../kernel/gecko/g_screen_wide.h" // Screen: Widescreen
#include "../../kernel/gecko/g_screen_mon43.h" // Screen: 4:3 -> 73:60

#define NULL ((void *)0)

/***************************************
 * Controller Fix Options
 ***************************************/ 
const MeleeCodeOption cfOptionOff = {
	1, // value
	"Off", // name
	0, // codeLen
	NULL, // code
};

const MeleeCodeOption cfOptionUcf = {
	2, // value
	"UCF", // name
	g_ucf_size, // codeLen
	g_ucf, // code
};

const MeleeCodeOption cfOptionStealthUcf = {
	3, // value
	"Stealth", // name
	g_ucf_stealth_size, // codeLen
	g_ucf_stealth, // code
};

const MeleeCodeOption *cfOptions[MELEE_CODES_CF_OPTION_COUNT] = {
	&cfOptionOff,
	&cfOptionUcf,
	&cfOptionStealthUcf,
};

static const char *cfDescription[] = {
	"The type of controller fix to apply",
	"",
	"  [UCF]",
	"    Will enable UCF 0.8. Includes",
	"    dashback, shield drop, and",
	"    wiggle out of tumble.",
	"",
	"  [Stealth]",
	"    Enables UCF 0.8 without the",
	"    CSS text",
	NULL
};

const MeleeCodeLineItem cfLineItem = {
	MELEE_CODES_CF_OPTION_ID, // identifier
	"Controller Fix", // name
	cfDescription, // description
	2, // defaultValue
	MELEE_CODES_CF_OPTION_COUNT, // optionCount
	cfOptions, // options
};

/***************************************
 * Version Options
 ***************************************/
const MeleeCodeOption versionOptionNtsc = {
	1,
	"Off",
	0,
	NULL,
};

const MeleeCodeOption versionOptionPal = {
	2,
	"On",
	g_pal_size,
	g_pal,
};

const MeleeCodeOption *versionOptions[MELEE_CODES_VERSION_OPTION_COUNT] = {
	&versionOptionNtsc,
	&versionOptionPal,
};

static const char *versionDescription[] = {
	"Applies PAL balance changes",
	"to a NTSC 1.02 ISO",
	"",
	"You MUST use an NTSC 1.02 ISO",
	"for Slippi to work",
	NULL
};

const MeleeCodeLineItem versionLineItem = {
	MELEE_CODES_VERSION_OPTION_ID, // identifier
	"PAL Patch",
	versionDescription,
	1,
	MELEE_CODES_VERSION_OPTION_COUNT,
	versionOptions,
};

/***************************************
 * Mods Options
 ***************************************/
const MeleeCodeOption modsOptionOff = {
	1,
	"Off",
	0,
	NULL,
};

const MeleeCodeOption modsOptionStealth = {
	2,
	"Stealth",
	g_mods_stealth_size,
	g_mods_stealth,
};

const MeleeCodeOption modsOptionTournament = {
	3,
	"Tournament",
	g_mods_tournament_size,
	g_mods_tournament,
};

const MeleeCodeOption modsOptionFriendlies = {
	4,
	"Friendlies",
	g_mods_friendlies_size,
	g_mods_friendlies,
};

const MeleeCodeOption *modsOptions[MELEE_CODES_MODS_OPTION_COUNT] = {
	&modsOptionOff,
	&modsOptionStealth,
	&modsOptionTournament,
	&modsOptionFriendlies,
};

static const char *modsDescription[] = {
	"Convenience changes. Each option",
	"includes all changes above it",
	"",
	"  [Stealth]",
	"    Neutral Spawns",
	"    Hide tags when invisible",
	"    Preserve tag in rotation",
	"",
	"  [Tournament]",
	"    D-Pad Rumble",
	"    Stage Striking",
	"",
	"  [Friendlies]",
	"    Skip Results",
	"    Salty Runback (A+B)",
	NULL
};

const MeleeCodeLineItem modsLineItem = {
	MELEE_CODES_MODS_OPTION_ID, // identifier
	"Convenience",
	modsDescription,
	2,
	MELEE_CODES_MODS_OPTION_COUNT,
	modsOptions,
};

/***************************************
 * Lag Reduction Options
 ***************************************/
const MeleeCodeOption lagReductionOptionOff = {
	1,
	"Off",
	0,
	NULL,
};

const MeleeCodeOption lagReductionOptionPd = {
	2,
	"PD",
	g_lag_pd_size,
	g_lag_pd,
};

const MeleeCodeOption lagReductionOptionPdVb = {
	3,
	"PD + VB",
	g_lag_pdvb_size,
	g_lag_pdvb,
};

const MeleeCodeOption *lagReductionOptions[MELEE_CODES_LAG_REDUCTION_OPTION_COUNT] = {
	&lagReductionOptionOff,
	&lagReductionOptionPd,
	&lagReductionOptionPdVb,
};

static const char *lagReductionDescription[] = {
	"Lag reduction codes",
	"",
	"  [PD]",
	"    Polling drift fix. Ensures",
	"    inputs are polled consistently.",
	"    Average 4.17ms reduction",
	"",
	"  [PD+VB]",
  "    VB is the visual buffer.",
	"    Removes one frame of lag.",
	"    Average 20.83ms reduction",
	NULL,
};

const MeleeCodeLineItem lagReductionLineItem = {
	MELEE_CODES_LAG_REDUCTION_OPTION_ID, // identifier
	"Lag Reduction",
	lagReductionDescription,
	1,
	MELEE_CODES_LAG_REDUCTION_OPTION_COUNT,
	lagReductionOptions,
};

/***************************************
 * Stage Options
 ***************************************/
const MeleeCodeOption stagesOptionVanilla = {
	1,
	"Off",
	0,
	NULL,
};

const MeleeCodeOption stagesOptionFrozenStadium = {
	2,
	"Stadium",
	g_stages_stadium_size,
	g_stages_stadium,
};

const MeleeCodeOption stagesOptionFrozenAll = {
	3,
	"All",
	g_stages_all_size,
	g_stages_all,
};

const MeleeCodeOption *stagesOptions[MELEE_CODES_STAGES_OPTION_COUNT] = {
	&stagesOptionVanilla,
	&stagesOptionFrozenStadium,
	&stagesOptionFrozenAll,
};

static const char *stagesDescription[] = {
	"Choose which stages should be frozen.",
	"",
	"  [Stadium]",
	"    Freezes Stadium only",
	"",
	"  [All]",
	"    Freezes Stadium, Whispy,",
	"    Shy Guys, FD backgrounds",
	NULL
};

const MeleeCodeLineItem stagesLineItem = {
	MELEE_CODES_STAGES_OPTION_ID, // identifier
	"Frozen Stages",
	stagesDescription,
	1,
	MELEE_CODES_STAGES_OPTION_COUNT,
	stagesOptions,
};

/***************************************
 * Gameplay Options
 ***************************************/
const MeleeCodeOption gameplayOptionVanilla = {
	1,
	"Off",
	0,
	NULL,
};

const MeleeCodeOption gameplayOptionLgl = {
	2,
	"LGL",
	g_gameplay_lgl_size,
	g_gameplay_lgl,
};

const MeleeCodeOption gameplayOptionWobbling = {
	2,
	"Wobbling",
	g_gameplay_wobbling_size,
	g_gameplay_wobbling,
};

const MeleeCodeOption gameplayOptionBoth = {
	2,
	"Both",
	g_gameplay_both_size,
	g_gameplay_both,
};

const MeleeCodeOption *gameplayOptions[MELEE_CODES_GAMEPLAY_OPTION_COUNT] = {
	&gameplayOptionVanilla,
	&gameplayOptionLgl,
	&gameplayOptionWobbling,
	&gameplayOptionBoth,
};

static const char *gameplayDescription[] = {
	"Choose gameplay changes",
	"",
	"  [LGL]",
	"    Ledge grab limit. In the event",
	"    of a timeout, a player with",
	"    more than 45 ledge grabs will",
	"    lose",
	"",
	"  [Wobbling]",
	"    Prevents wobbling. After Nana",
	"    hits opponent 3 times, grab is",
	"    broken.",
	"",
	"  [Both]",
	"    Enables both other codes",
	NULL
};

const MeleeCodeLineItem gameplayLineItem = {
	MELEE_CODES_GAMEPLAY_OPTION_ID, // identifier
	"Gameplay",
	gameplayDescription,
	1,
	MELEE_CODES_GAMEPLAY_OPTION_COUNT,
	gameplayOptions,
};

/***************************************
 * Screen Options
 ***************************************/
const MeleeCodeOption screenOptionDefault = {
	1,
	"Default",
	0,
	NULL,
};

const MeleeCodeOption screenOptionMonitor43 = {
	MELEE_CODES_43_MONITOR_VALUE,
	"Monitor 4:3",
	g_screen_mon43_size,
	g_screen_mon43,
};

const MeleeCodeOption screenOptionMonitor169 = {
	MELEE_CODES_WIDE_VALUE,
	"Widescreen",
	g_screen_wide_size,
	g_screen_wide,
};

const MeleeCodeOption *screenOptions[MELEE_CODES_SCREEN_OPTION_COUNT] = {
	&screenOptionDefault,
	&screenOptionMonitor43,
	&screenOptionMonitor169,
};

static const char *screenDescription[] = {
	"Codes used for aspect ratio.",
	"",
	"  [Default]",
	"    Use on CRT or for no change.",
	"",
	"  [Monitor 4:3]",
	"    Use with monitor with aspect",
	"    forced to 4:3. Will modify",
	"    game to look 73:60.",
	"",
	"  [Widescreen]",
	"    Use on a 16:9 monitor. Will",
	"    apply logic to expand game",
	"    visuals.",
	NULL
};

const MeleeCodeLineItem screenLineItem = {
	MELEE_CODES_SCREEN_OPTION_ID, // identifier
	"Screen",
	screenDescription,
	1,
	MELEE_CODES_SCREEN_OPTION_COUNT,
	screenOptions,
};

/***************************************
 * Combine Everything
 ***************************************/
const MeleeCodeLineItem *meleeCodeLineItems[] = {
	&cfLineItem,
	&versionLineItem,
	&modsLineItem,
	&lagReductionLineItem,
	&stagesLineItem,
	&gameplayLineItem,
	&screenLineItem,
};

const MeleeCodeConfig mcconfig = {
	MELEE_CODES_LINE_ITEM_COUNT,
	meleeCodeLineItems,
};

const MeleeCodeConfig *GetMeleeCodeConfig() {
	return &mcconfig;
}
