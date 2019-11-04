#include "MeleeCodes.h"

#include "../../kernel/gecko/g_ucf.h" // UCF codeset
#include "../../kernel/gecko/g_pal.h" // PAL codeset

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
	0, // codeLen
	NULL, // code
};

const MeleeCodeOption cfOptionToggle = {
	4, // value
	"Toggle", // name
	0, // codeLen
	NULL, // code
};

const MeleeCodeOption *cfOptions[MELEE_CODES_CF_OPTION_COUNT] = {
	&cfOptionOff,
	&cfOptionUcf,
	&cfOptionStealthUcf,
	&cfOptionToggle,
};

static const char *cfDescription[] = {
	"Emulates a memory card in",
	"Slot A using a .raw file.",
	"",
	"Disable this option if you",
	"want to use a real memory",
	"card on an original Wii.",
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
	"NTSC",
	0,
	NULL,
};

const MeleeCodeOption versionOptionPal = {
	2,
	"PAL",
	g_pal_size,
	g_pal,
};

const MeleeCodeOption *versionOptions[MELEE_CODES_VERSION_OPTION_COUNT] = {
	&versionOptionNtsc,
	&versionOptionPal,
};

static const char *versionDescription[] = {
	"Emulates a memory card in",
	"Slot A using a .raw file.",
	"",
	"Disable this option if you",
	"want to use a real memory",
	"card on an original Wii.",
	NULL
};

const MeleeCodeLineItem versionLineItem = {
	MELEE_CODES_VERSION_OPTION_ID, // identifier
	"Game Version",
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
	0,
	NULL,
};

const MeleeCodeOption modsOptionTournament = {
	3,
	"Tournament",
	0,
	NULL,
};

const MeleeCodeOption modsOptionFriendlies = {
	4,
	"Friendlies",
	0,
	NULL,
};

const MeleeCodeOption *modsOptions[MELEE_CODES_MODS_OPTION_COUNT] = {
	&modsOptionOff,
	&modsOptionStealth,
	&modsOptionTournament,
	&modsOptionFriendlies,
};

static const char *modsDescription[] = {
	"Emulates a memory card in",
	"Slot A using a .raw file.",
	"",
	"Disable this option if you",
	"want to use a real memory",
	"card on an original Wii.",
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
	0,
	NULL,
};

const MeleeCodeOption lagReductionOptionPdVb = {
	3,
	"PD + VB",
	0,
	NULL,
};

const MeleeCodeOption *lagReductionOptions[MELEE_CODES_LAG_REDUCTION_OPTION_COUNT] = {
	&lagReductionOptionOff,
	&lagReductionOptionPd,
	&lagReductionOptionPdVb,
};

static const char *lagReductionDescription[] = {
	"Emulates a memory card in",
	"Slot A using a .raw file.",
	"",
	"Disable this option if you",
	"want to use a real memory",
	"card on an original Wii.",
	NULL
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
 * Mods Options
 ***************************************/
const MeleeCodeOption rulesetOptionVanilla = {
	1,
	"Vanilla",
	0,
	NULL,
};

const MeleeCodeOption rulesetOptionFrozenStadium = {
	2,
	"Frz Stadium",
	0,
	NULL,
};

const MeleeCodeOption *rulesetOptions[MELEE_CODES_RULESET_OPTION_COUNT] = {
	&rulesetOptionVanilla,
	&rulesetOptionFrozenStadium,
};

static const char *rulesetDescription[] = {
	"Emulates a memory card in",
	"Slot A using a .raw file.",
	"",
	"Disable this option if you",
	"want to use a real memory",
	"card on an original Wii.",
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
