#ifndef _MELEE_CODES_H
#define _MELEE_CODES_H

#define MELEE_CODES_CF_OPTION_COUNT 5
#define MELEE_CODES_VERSION_OPTION_COUNT 2
#define MELEE_CODES_MODS_OPTION_COUNT 4
#define MELEE_CODES_LAG_REDUCTION_OPTION_COUNT 3
#define MELEE_CODES_STAGES_OPTION_COUNT 3
#define MELEE_CODES_SCREEN_OPTION_COUNT 3
#define MELEE_CODES_GAMEPLAY_OPTION_COUNT 4
#define MELEE_CODES_SAFETY_OPTION_COUNT 2

#define MELEE_CODES_CF_OPTION_ID 0
#define MELEE_CODES_VERSION_OPTION_ID 1
#define MELEE_CODES_MODS_OPTION_ID 2
#define MELEE_CODES_LAG_REDUCTION_OPTION_ID 3
#define MELEE_CODES_STAGES_OPTION_ID 4
#define MELEE_CODES_GAMEPLAY_OPTION_ID 5
#define MELEE_CODES_SCREEN_OPTION_ID 6
#define MELEE_CODES_SAFETY_OPTION_ID 7

// Indicates the maximum number possible for ID. This is important in case the line items change,
// we don't want to persist setting values for a different line item
// THIS AFFECTS sizeof(NIN_CFG).
// IF YOU CHANGE THIS NUMBER, YOU MUST BUMP NIN_CFG_VERSION AND UPDATE LoadNinCFG.
#define MELEE_CODES_MAX_ID 7

#define MELEE_CODES_LINE_ITEM_COUNT 8

#define MELEE_CODES_WIDE_SHUTTERS_VALUE 2
#define MELEE_CODES_WIDE_VALUE 3

typedef struct MeleeCodeOption
{
	const int value;
	const char *name;
	const int codeLen;
	const unsigned char *code;
} MeleeCodeOption;

typedef struct MeleeCodeLineItem
{
	const int identifier; // unique identifier for a line item
	const char *name;
	const char *const *description; // max lines is 18, line length should be <= 36
	const int defaultValue;
	const int optionCount;
	const MeleeCodeOption **options;
} MeleeCodeLineItem;

typedef struct MeleeCodeConfig
{
	const int lineItemCount;
	const MeleeCodeLineItem **items;
} MeleeCodeConfig;

const MeleeCodeConfig *GetMeleeCodeConfig();

#endif // _MELEE_CODES_H
