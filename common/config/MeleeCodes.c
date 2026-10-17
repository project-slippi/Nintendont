#include "MeleeCodes.h"

#include "../../kernel/gecko/g_ucf_084.h" // UCF codeset
#include "../../kernel/gecko/g_ucf_084_stealth.h" // UCF Stealth

#include "../../kernel/gecko/g_pal.h" // PAL codeset

#include "../../kernel/gecko/g_mods_stealth.h" // Mods: Stealth
#include "../../kernel/gecko/g_mods_tournament.h" // Mods: Tournament
#include "../../kernel/gecko/g_mods_friendlies.h" // Mods: Friendlies

#include "../../kernel/gecko/g_lag_pd.h" // Lag Reduction: PD
#include "../../kernel/gecko/g_lag_pdhalfvb.h" // Lag Reduction: PD+VB

#include "../../kernel/gecko/g_stages_stadium.h" // Stages: Frozen Pokemon
#include "../../kernel/gecko/g_stages_all.h" // Stages: Frozen Pokemon

#include "../../kernel/gecko/g_gameplay_lgl.h" // Gameplay: LGL
#include "../../kernel/gecko/g_gameplay_wobbling.h" // Gameplay: Wobbling
#include "../../kernel/gecko/g_gameplay_both.h" // Gameplay: Both

#include "../../kernel/gecko/g_screen_wide.h" // Screen: Widescreen
#include "../../kernel/gecko/g_screen_wide_shutters.h" // Screen: 4:3 -> 73:60

#include "../../kernel/gecko/g_safety.h" // Safety codeset

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
	g_ucf_084_size, // codeLen
	g_ucf_084, // code
};

const MeleeCodeOption cfOptionStealthUcf = {
	3, // value
	"Stealth", // name
	g_ucf_084_stealth_size, // codeLen
	g_ucf_084_stealth, // code
};

const MeleeCodeOption *cfOptions[MELEE_CODES_CF_OPTION_COUNT] = {
	&cfOptionOff,
	&cfOptionUcf,
	&cfOptionStealthUcf,
};

// an additional space was added to all lines to accomdate "Stealth 0.84"
// we can remove them once 0.84 is the only option
static const char *cfDescription[] = {
	"The type of controller fix to apply",
	"",
	"   [UCF]",
	"    Will enable UCF 0.84. Includes",
	"    dashback, shield drop,",
	"    wiggle out of tumble,",
	"    1.0 cardinals, frame 1 SDI fix,",
	"    and shield drop range up",
	"    conditional increase.",
	"",
	"   [Stealth]",
	"    Enables UCF 0.84 without the",
	"    CSS text",
	"",
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
	"    Freeze Glitch Fix",
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
	"PDF",
	g_lag_pd_size,
	g_lag_pd,
};

// Stands for every amount of the visual buffer code. The amount is saved in
// the config value and applied by the kernel (see GetLagReductionPollLines).
const MeleeCodeOption lagReductionOptionPdVb = {
	MELEE_CODES_LAG_VB_VALUE,
	"PDF + 1/2F",
	g_lag_pdhalfvb_size,
	g_lag_pdhalfvb,
};

const MeleeCodeOption *lagReductionOptions[MELEE_CODES_LAG_REDUCTION_OPTION_COUNT] = {
	&lagReductionOptionOff,
	&lagReductionOptionPd,
	&lagReductionOptionPdVb,
};

static const char *lagReductionDescription[] = {
	"Lag reduction codes",
	"",
	"  [PDF]",
	"    Polling drift fix. Ensures",
	"    inputs are polled consistently.",
	"    Average 4.17ms reduction",
	"",
	"  [PDF + 0.05F to 0.95F]",
	"    Also removes the chosen amount",
	"    of lag. 1F is 16.68ms, so",
	"    0.50F averages 12.50ms total.",
	"    High values may drop frames.",
	"    Not for SD CRTs. Forces 480p.",
	"",
	"  A: Off, PDF, 0.25, 0.50, 0.75",
	"  Left/Right: adjust by 0.01",
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
	3,
	"Wobbling",
	g_gameplay_wobbling_size,
	g_gameplay_wobbling,
};

const MeleeCodeOption gameplayOptionBoth = {
	4,
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
	"    Prevents wobbling. After Popo",
	"    pummels 4 times in hitstun,",
	"    grab is broken.",
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

const MeleeCodeOption screenOptionMonitor7360 = {
	MELEE_CODES_WIDE_SHUTTERS_VALUE,
	"16:9➧73:60",
	g_screen_wide_shutters_size,
	g_screen_wide_shutters,
};

const MeleeCodeOption screenOptionMonitor169 = {
	MELEE_CODES_WIDE_VALUE,
	"Widescreen",
	g_screen_wide_size,
	g_screen_wide,
};

const MeleeCodeOption *screenOptions[MELEE_CODES_SCREEN_OPTION_COUNT] = {
	&screenOptionDefault,
	&screenOptionMonitor7360,
	&screenOptionMonitor169,
};

static const char *screenDescription[] = {
	"Codes used for aspect ratio.",
	"",
	"  [Default]",
	"    Use on CRT or for no change.",
	"",
	"  [16:9➧73:60]",
	"    Use on monitor to show Melee",
	"    at its native resolution.",
	"    Set monitor to full/stretch",
	"    as if using widescreen.",
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
 * Version Options
 ***************************************/
const MeleeCodeOption safetyOptionOff = {
	1,
	"Off",
	0,
	NULL,
};

const MeleeCodeOption safetyOptionOn = {
	2,
	"On",
	g_safety_size,
	g_safety,
};

const MeleeCodeOption *safetyOptions[MELEE_CODES_SAFETY_OPTION_COUNT] = {
	&safetyOptionOff,
	&safetyOptionOn,
};

static const char *safetyDescription[] = {
	"Codes used for tournament safety.",
	"",
	"  [On]",
	"    Force 1.0 Damage Ratio",
	"    Pause always on for timed mode",
	"    Pause always on for 5+ stock",
	NULL
};

const MeleeCodeLineItem safetyLineItem = {
	MELEE_CODES_SAFETY_OPTION_ID, // identifier
	"Safety",
	safetyDescription,
	1,
	MELEE_CODES_SAFETY_OPTION_COUNT,
	safetyOptions,
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
	&safetyLineItem,
};

const MeleeCodeConfig mcconfig = {
	MELEE_CODES_LINE_ITEM_COUNT,
	meleeCodeLineItems,
};

const MeleeCodeConfig *GetMeleeCodeConfig() {
	return &mcconfig;
}

const MeleeCodeOption *GetMeleeCodeOption(const MeleeCodeLineItem *item, unsigned int value) {
	if (item->identifier == MELEE_CODES_LAG_REDUCTION_OPTION_ID && GetLagReductionHundredths(value) >= 0)
		value = MELEE_CODES_LAG_VB_VALUE;

	int i;
	for (i = 0; i < item->optionCount; i++) {
		if (item->options[i]->value == (int)value)
			return item->options[i];
	}
	return NULL;
}

/***************************************
 * Lag Reduction Amount
 ***************************************/
static const int lagReductionPresets[] = { 25, 50, 75 };

// Returns the visual buffer amount in hundredths of a frame, or -1 when the
// value is Off, PDF or unknown.
int GetLagReductionHundredths(unsigned int value) {
	if (value == MELEE_CODES_LAG_VB_VALUE)
		return 50;
	if (value >= MELEE_CODES_LAG_VB_VALUE_BASE + MELEE_CODES_LAG_VB_MIN &&
		value <= MELEE_CODES_LAG_VB_VALUE_BASE + MELEE_CODES_LAG_VB_MAX)
		return value - MELEE_CODES_LAG_VB_VALUE_BASE;
	return -1;
}

unsigned int GetLagReductionValue(int hundredths) {
	if (hundredths == 50)
		return MELEE_CODES_LAG_VB_VALUE;
	return MELEE_CODES_LAG_VB_VALUE_BASE + hundredths;
}

// Left/Right walk Off, PDF, then the visual buffer amounts one step at a time.
unsigned int StepLagReductionValue(unsigned int value, int direction) {
	int hundredths = GetLagReductionHundredths(value);
	if (hundredths < 0) {
		if (direction > 0)
			return value == MELEE_CODES_LAG_PD_VALUE ?
				GetLagReductionValue(MELEE_CODES_LAG_VB_MIN) : MELEE_CODES_LAG_PD_VALUE;
		return MELEE_CODES_LAG_OFF_VALUE;
	}

	hundredths += direction;
	if (hundredths < MELEE_CODES_LAG_VB_MIN)
		return MELEE_CODES_LAG_PD_VALUE;
	if (hundredths > MELEE_CODES_LAG_VB_MAX)
		hundredths = MELEE_CODES_LAG_VB_MAX;
	return GetLagReductionValue(hundredths);
}

// A cycles Off, PDF, then the presets, and wraps back to Off.
unsigned int NextLagReductionPreset(unsigned int value) {
	int hundredths = GetLagReductionHundredths(value);
	if (hundredths < 0)
		return value == MELEE_CODES_LAG_PD_VALUE ?
			GetLagReductionValue(lagReductionPresets[0]) : MELEE_CODES_LAG_PD_VALUE;

	unsigned int i;
	for (i = 0; i < sizeof(lagReductionPresets) / sizeof(lagReductionPresets[0]); i++) {
		if (lagReductionPresets[i] > hundredths)
			return GetLagReductionValue(lagReductionPresets[i]);
	}
	return MELEE_CODES_LAG_OFF_VALUE;
}

// The visual buffer code starts each frame on the second SI poll after
// retrace. Melee polls at sampling rate 11, and that rate's entry in the SDK's
// XYNTSC table holds the lines between polls, doubled in 480p, so a frame is
// 262.5 of them. Melee's own entry is 131, which is the 1/2F code as released.
// Returns 0 when the value isn't the visual buffer code.
unsigned int GetLagReductionPollLines(unsigned int value) {
	int hundredths = GetLagReductionHundredths(value);
	if (hundredths < 0)
		return 0;
	return (hundredths * 525 + 100) / 200;
}
