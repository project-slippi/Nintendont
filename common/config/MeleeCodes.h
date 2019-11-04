#ifndef _MELEE_CODES_H
#define _MELEE_CODES_H

#define MELEE_CODES_CF_OPTION_COUNT 4
#define MELEE_CODES_VERSION_OPTION_COUNT 2
#define MELEE_CODES_MODS_OPTION_COUNT 4
#define MELEE_CODES_LAG_REDUCTION_OPTION_COUNT 3
#define MELEE_CODES_RULESET_OPTION_COUNT 2

#define MELEE_CODES_CF_OPTION_ID 0
#define MELEE_CODES_VERSION_OPTION_ID 1
#define MELEE_CODES_MODS_OPTION_ID 2
#define MELEE_CODES_LAG_REDUCTION_OPTION_ID 3
#define MELEE_CODES_RULESET_OPTION_ID 4

// Indicates the maximum number possible for ID. This is important in case the line items change,
// we don't want to persist setting values for a different line item
#define MELEE_CODES_MAX_ID 4

#define MELEE_CODES_LINE_ITEM_COUNT 5

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
	const char *const *description;
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
