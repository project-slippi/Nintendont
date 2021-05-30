#include "MeleeCodes.h"

#include "../../kernel/gecko/g_ucf.h" // UCF codeset
#include "../../kernel/gecko/g_ucf_stealth.h" // UCF Stealth

#include "../../kernel/gecko/g_pal.h" // PAL codeset

#include "../../kernel/gecko/g_mods_stealth.h" // Mods: Stealth
#include "../../kernel/gecko/g_mods_tournament.h" // Mods: Tournament
#include "../../kernel/gecko/g_mods_friendlies.h" // Mods: Friendlies

#include "../../kernel/gecko/g_lag_pd.h" // Lag Reduction: PD
#include "../../kernel/gecko/g_lag_pdvb.h" // Lag Reduction: PD+VB

#include "../../kernel/gecko/g_frozen.h" // Ruleset: Frozen Pokemon

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
	"1. UCF will enable UCF 0.8",
	"2. Stealth will enable UCF", 
	"   without the CSS text",
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
	"Game mods to apply",
	"Each option includes",
	"all mods above it",
	"",
	"  [Stealth]",
	"    Neutral Spawns,",
	"    Hide tags when invisible,",
	"    Preserve tag in rotation",
	"",
	"  [Tournament]",
	"    D-Pad Rumble",
	"    Stage Striking",
	"",
	"  [Friendlies]",
	"    Skip Results,",
	"    Salty Runback (A+B)",
	NULL
};

const MeleeCodeLineItem modsLineItem = {
	MELEE_CODES_MODS_OPTION_ID, // identifier
	"Game Mods",
	modsDescription,
	1,
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
	"1. PD is the polling drift fix",
	"   it will make sure",
	"   inputs are polled consistently.",
	"   Average 4.17ms reduction",
	"",
	"2. PD+VB, VB is the visual buffer",
	"   it removes one frame of lag.",
	"   Average 20.83ms reduction",
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
 * Ruleset Options
 ***************************************/
const MeleeCodeOption rulesetOptionVanilla = {
	1,
	"Off",
	0,
	NULL,
};

const MeleeCodeOption rulesetOptionFrozenStadium = {
	2,
	"Frzn Pkmn",
	g_frozen_size,
	g_frozen,
};

const MeleeCodeOption *rulesetOptions[MELEE_CODES_RULESET_OPTION_COUNT] = {
	&rulesetOptionVanilla,
	&rulesetOptionFrozenStadium,
};

static const char *rulesetDescription[] = {
	"Codes that change the",
	"rules of the game.",
	"",
	"Frz Pkm will freeze stadium",
	NULL
};

const MeleeCodeLineItem rulesetLineItem = {
	MELEE_CODES_RULESET_OPTION_ID, // identifier
	"Ruleset",
	rulesetDescription,
	1,
	MELEE_CODES_RULESET_OPTION_COUNT,
	rulesetOptions,
};

/***************************************
 * Combine Everything
 ***************************************/
const MeleeCodeLineItem *meleeCodeLineItems[] = {
	&cfLineItem,
	&versionLineItem,
	&modsLineItem,
	&lagReductionLineItem,
	&rulesetLineItem,
};

const MeleeCodeConfig mcconfig = {
	MELEE_CODES_LINE_ITEM_COUNT,
	meleeCodeLineItems,
};

const MeleeCodeConfig *GetMeleeCodeConfig() {
	return &mcconfig;
}
