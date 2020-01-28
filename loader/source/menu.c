/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <gccore.h>
#include <sys/param.h>
#include "font.h"
#include "exi.h"
#include "global.h"
#include "FPad.h"
#include "Config.h"
#include "update.h"
#include "titles.h"
#include "dip.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ogc/stm.h>
#include <ogc/consol.h>
#include <ogc/system.h>
//#include <fat.h>
#include <di/di.h>

#include "menu.h"
#include "../../common/include/CommonConfigStrings.h"
#include "../../common/config/MeleeCodes.h"
#include "ff_utf8.h"
#include "ShowGameInfo.h"

static u8 meleeCodeSelectionIndices[MELEE_CODES_LINE_ITEM_COUNT];
static u8 devState = DEV_OK;
extern NIN_CFG* ncfg;

// Disc format colors
const u32 DiscFormatColors[8] =
{
	BLACK,		// Full
	0x551A00FF,	// Shrunken (dark brown)
	0x00551AFF,	// Extracted FST
	0x001A55FF,	// CISO
	0x551A55FF,	// Multi-Game
	GRAY,		// undefined
	GRAY,		// undefined
	GRAY,		// undefined
};

// Supported disc image filenames
static const char disc_filenames[8][16] = {
	"game.ciso", "game.cso", "game.gcm", "game.iso",
	"disc2.ciso", "disc2.cso", "disc2.gcm", "disc2.iso"
};

static const char *desc_cheats[] = { 
	"Read and use Gecko codes from",
	"a .gct file on your USB/SD.",
	"",
	"WARNING: This may interfere",
	"with Slippi and toggleable",
	"codes (UCF/PAL toggle).",
	NULL,
};
static const char *desc_mcemu[] = {
	"Emulates a memory card in",
	"Slot A using a .raw file.",
	"",
	"Disable this option if you",
	"want to use a real memory",
	"card on an original Wii.",
	NULL
};
static const char *const desc_force_wide[] = {
	"Patch games to use a 16:9",
	"aspect ratio. (widescreen)",
	"",
	"Not all games support this",
	"option. The patches will not",
	"be applied to games that have",
	"built-in support for 16:9;",
	"use the game's options screen",
	"to configure the display mode.",
	NULL
};
static const char *const desc_force_prog[] = {
	"Patch games to always use 480p",
	"(progressive scan) output.",
	"",
	"Requires component cables, or",
	"an HDMI cable on Wii U.",
	NULL
};
static const char *desc_led[] = {
	"Use the drive slot LED as a",
	"disk activity indicator.",
	"",
	"The LED will be turned on",
	"when reading from or writing",
	"to the storage device.",
	"",
	"This option has no effect on",
	"Wii U, since the Wii U does",
	"not have a drive slot LED.",
	NULL
};
static const char *desc_remlimit[] = {
	"Disc read speed is normally",
	"limited to the performance of",
	"the original GameCube disc",
	"drive.",
	"",
	"Unlocking read speed can allow",
	"for faster load times, but it",
	"can cause problems with games",
	"that are extremely sensitive",
	"to disc read timing.",
	NULL
};
static const char *desc_language[] = {
	"Set the system language.",
	"",
	"This option is normally only",
	"found on PAL GameCubes, so",
	"it usually won't have an",
	"effect on NTSC games.",
	NULL
};
static const char *desc_memcard_blocks[] = {
	"Default size for new memory",
	"card images.",
	"",
	"NOTE: Sizes larger than 251",
	"blocks are known to cause",
	"issues.",
	NULL
};
static const char *desc_memcard_multi[] = {
	"Nintendont usually uses one",
	"emulated memory card image",
	"per game.",
	"",
	"Enabling MULTI switches this",
	"to use one memory card image",
	"for all USA and PAL games,",
	"and one image for all JPN",
	"games.",
	NULL
};
static const char *desc_networking[] = {
	"Enable Slippi networking.",
	"Network settings must be configured",
	"in the Wii settings menu",
	"in order to use this feature.",
	NULL
};
static const char *desc_slippi_file_write[] = {
	"Write Slippi replays to a",
	"secondary storage device.",
	"Requires a USB AND SD device",
	"to be plugged in.",
	NULL
};
static const char *desc_slippi_port_a[] = {
	"When enabled, emulate Slippi",
	"on Port A instead of Port B.",
	" (DEBUGGING FEATURE) ",
	NULL
};

const pageinfo slippiSettingsPage = {
	PAGE_SLIPPI_SETTINGS,
	"Slippi Settings",
};

const pageinfo ninSettingsPage = {
	PAGE_SETTINGS,
	"Nintendont Settings",
};

pageinfo allPages[] = {
	slippiSettingsPage,
	ninSettingsPage,
};

// Inline function declarations
FPAD_WRAPPER_REPEAT(Up)
FPAD_WRAPPER_REPEAT(Down)
FPAD_WRAPPER_REPEAT(Left)
FPAD_WRAPPER_REPEAT(Right)
inline u32 SettingY(u32 row) { return 127 + 16 * row; }

u32 Shutdown = 0;
void SetShutdown(void) { Shutdown = 1; }
void HandleWiiMoteEvent(s32 chan) { SetShutdown(); }

int compare_names(const void *a, const void *b)
{
	const gameinfo *da = (const gameinfo *) a;
	const gameinfo *db = (const gameinfo *) b;

	int ret = strcasecmp(da->Name, db->Name);
	if (ret == 0)
	{
		// Names are equal. Check disc number.
		const uint8_t dnuma = (da->Flags & GIFLAG_DISCNUMBER_MASK);
		const uint8_t dnumb = (db->Flags & GIFLAG_DISCNUMBER_MASK);
		if (dnuma < dnumb)
			ret = -1;
		else if (dnuma > dnumb)
			ret = 1;
		else
			ret = 0;
	}
	return ret;
}



/**
 * Check if a disc image is valid.
 * @param filename	[in]  Disc image filename. (ISO/GCM)
 * @param discNumber	[in]  Disc number.
 * @param gi		[out] gameinfo struct to store game information if the disc is valid.
 * @return True if the image is valid; false if not.
 */
static bool IsDiscImageValid(const char *filename, int discNumber, gameinfo *gi)
{
	// TODO: Handle FST format (sys/boot.bin).
	u8 buf[0x100];			// Disc header.
	u32 BI2region_addr = 0x458;	// BI2 region code address.
	FIL in;

	// Could not open the disc image.
	if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) != FR_OK)
		return false;

	// Read the disc header
	//gprintf("(%s) ok\n", filename );
	bool ret = false;
	UINT read;
	f_read(&in, buf, sizeof(buf), &read);
	if (read != sizeof(buf))
	{
		// Error reading from the file.
		f_close(&in);
		return false;
	}

	// Check for CISO magic with 2 MB block size.
	// NOTE: CISO block size is little-endian.
	static const uint8_t CISO_MAGIC[8] = {'C','I','S','O',0x00,0x00,0x20,0x00};
	if (!memcmp(buf, CISO_MAGIC, sizeof(CISO_MAGIC)) && !IsGCGame(buf))
	{
		// CISO magic is present, and GCN magic isn't.
		// This is most likely a CISO image.
		// Read the actual GCN header.
		f_lseek(&in, 0x8000);
		f_read(&in, buf, 0x100, &read);
		if (read != 0x100)
		{
			// Error reading from the file.
			f_close(&in);
			return false;
		}

		// Adjust the BI2 region code address for CISO.
		BI2region_addr = 0x8458;

		gi->Flags = GIFLAG_FORMAT_CISO;
	}
	else
	{
		// Standard GameCube disc image.
		// TODO: Detect Triforce images and exclude them
		// from size checking?

		// Full 1:1 GameCube image.
		if (in.obj.objsize == 1459978240)
			gi->Flags = GIFLAG_FORMAT_FULL;
		// Shrunken GameCube image.
		else
			gi->Flags = GIFLAG_FORMAT_SHRUNKEN;
	}

	// Must be GC game
	if (IsGCGame(buf))
	{
		// Read the BI2 region code.
		u32 BI2region;
		f_lseek(&in, BI2region_addr);
		f_read(&in, &BI2region, sizeof(BI2region), &read);

		// Error reading from the file.
		if (read != sizeof(BI2region))
		{
			f_close(&in);
			return false;
		}

		// Save the region code for later.
		gi->Flags |= ((BI2region & 3) << 3);

		// Save the game ID.
		memcpy(gi->ID, buf, 6); //ID for EXI
		gi->Flags |= (discNumber & 3) << 5;

		// Save the revision number.
		gi->Revision = buf[0x07];

		// Check if this is a multi-game image.
		// Reference: https://gbatemp.net/threads/wit-wiimms-iso-tools-gamecube-disc-support.251630/#post-3088119
		const bool is_multigame = IsMultiGameDisc((const char*)buf);
		if (is_multigame)
		{
			// Multi-game + CISO is NOT supported.
			if (gi->Flags == GIFLAG_FORMAT_CISO)
				ret = false;
			else
			{
				char *name = (char*)malloc(65);
				const char *slash_pos = strrchr(filename, '/');
				snprintf(name, 65, "Multi-Game Disc (%s)", (slash_pos ? slash_pos+1 : filename));
				gi->Name = name;
				gi->Flags = GIFLAG_FORMAT_MULTI | GIFLAG_NAME_ALLOC;
			}
		}
		else
		{
			// Check if this title is in titles.txt.
			bool isTriforce;
			const char *dbTitle = SearchTitles(gi->ID, &isTriforce);
			if (dbTitle)
			{
				// Title found.
				gi->Name = (char*)dbTitle;
				gi->Flags &= ~GIFLAG_NAME_ALLOC;

				if (isTriforce)
				{
					// Clear the format value if it's "shrunken",
					// since Triforce titles are never the size
					// of a full 1:1 GameCube disc image.
					if ((gi->Flags & GIFLAG_FORMAT_MASK) == GIFLAG_FORMAT_SHRUNKEN)
						gi->Flags &= ~GIFLAG_FORMAT_MASK;
				}
			}

			// Title not found - use title from disc header
			if (!dbTitle)
			{
				// Make sure the title in the header is NULL terminated.
				buf[0x20+65] = 0;
				gi->Name = strdup((const char*)&buf[0x20]);
				gi->Flags |= GIFLAG_NAME_ALLOC;
			}
		}

		gi->Path = strdup(filename);
		ret = true;
	}

	f_close(&in);
	return ret;
}

void initMeleeCodeSelections()
{
	const MeleeCodeConfig *codeConfig = GetMeleeCodeConfig();

	// Since codes might be added/removed, we don't want old settings to persist. 
	// This uses the values of the options to figure out which index the selection should be at
	int i, j;
	for (i = 0; i < codeConfig->lineItemCount; i++) {
		const MeleeCodeLineItem *lineItem = codeConfig->items[i];

		bool valueFound = false;
		for (j = 0; j < lineItem->optionCount; j++)
		{
			const MeleeCodeOption *option = lineItem->options[j];

			u32 currentValue = ncfg->MeleeCodeOptions[lineItem->identifier];
			if (currentValue != option->value)
				continue;

			meleeCodeSelectionIndices[i] = j;
			valueFound = true;
			break;
		}

		if (!valueFound)
		{
			// If value was not found, set default value
			for (j = 0; j < lineItem->optionCount; j++)
			{
				const MeleeCodeOption *option = lineItem->options[j];

				if (lineItem->defaultValue != option->value)
					continue;

				// Set index and option value
				meleeCodeSelectionIndices[i] = j;
				ncfg->MeleeCodeOptions[lineItem->identifier] = option->value;
			}
		}
	}
}

/**
 * Get all games from the games/ directory on the selected storage device.
 * On Wii, this also adds a pseudo-game for loading GameCube games from disc.
 *
 * @param gi           [out] Array of gameinfo structs.
 * @param sz           [in]  Maximum number of elements in gi.
 * @param pGameCount   [out] Number of games loaded. (Includes GCN pseudo-game for Wii.)
 *
 * @return DevState value:
 * - DEV_OK: Device opened and has GC titles in "games/" directory.
 * - DEV_NO_OPEN: Could not open the storage device.
 * - DEV_NO_GAMES: No "games/" directory was found.
 * - DEV_NO_TITLES: "games/" directory contains no GC titles.
 */
static DevState LoadGameList(gameinfo *gi, u32 sz, u32 *pGameCount)
{
	// Create a list of games
	char filename[MAXPATHLEN];	// Current filename.
	u8 buf[0x100];			// Disc header.
	int gamecount = 0;		// Current game count.

	// Pseudo game for booting a GameCube disc on Wii.
	gi[0].ID[0] = 'D',gi[0].ID[1] = 'I',gi[0].ID[2] = 'S';
	gi[0].ID[3] = 'C',gi[0].ID[4] = '0',gi[0].ID[5] = '1';
	gi[0].Name = "Boot GC Disc in Drive";
	gi[0].Revision = 0;
	gi[0].Flags = 0;
	gi[0].Path = strdup("di:di");
	gamecount++;

	DIR pdir;
	snprintf(filename, sizeof(filename), "%s:/games", GetRootDevice());
	if (f_opendir_char(&pdir, filename) != FR_OK)
	{
		// Could not open the "games" directory.

		// Attempt to open the device root.
		snprintf(filename, sizeof(filename), "%s:/", GetRootDevice());
		if (f_opendir_char(&pdir, filename) != FR_OK)
		{
			// Could not open the device root.
			if (pGameCount)
				*pGameCount = 0;
			return DEV_NO_OPEN;
		}

		// Device root opened.
		// This means the device is usable, but it
		// doesn't have a "games" directory.
		f_closedir(&pdir);
		if (pGameCount)
			*pGameCount = gamecount;
		return DEV_NO_GAMES;
	}

	// Process the directory.
	// TODO: chdir into /games/?
	FILINFO fInfo;
	FIL in;
	while (f_readdir(&pdir, &fInfo) == FR_OK && fInfo.fname[0] != '\0')
	{
		/**
		 * Game layout should be:
		 *
		 * ISO/GCM format:
		 * - /games/GAMEID/game.gcm
		 * - /games/GAMEID/game.iso
		 * - /games/GAMEID/disc2.gcm
		 * - /games/GAMEID/disc2.iso
		 * - /games/[anything].gcm
		 * - /games/[anything].iso
		 *
		 * CISO format:
		 * - /games/GAMEID/game.ciso
		 * - /games/GAMEID/game.cso
		 * - /games/GAMEID/disc2.ciso
		 * - /games/GAMEID/disc2.cso
		 * - /games/[anything].ciso
		 *
		 * FST format:
		 * - /games/GAMEID/sys/boot.bin plus other files
		 *
		 * NOTE: 2-disc games currently only work with the
		 * subdirectory layout, and the second disc must be
		 * named either disc2.iso or disc2.gcm.
		 */

		// Skip "." and "..".
		// This will also skip "hidden" directories.
		if (fInfo.fname[0] == '.')
			continue;

		// Subdirectory
		if (fInfo.fattrib & AM_DIR)
		{

			// Prepare the filename buffer with the directory name.
			// game.iso/disc2.iso will be appended later.
			// NOTE: fInfo.fname[] is UTF-16.
			const char *filename_utf8 = wchar_to_char(fInfo.fname);
			int fnlen = snprintf(filename, sizeof(filename), "%s:/games/%s/",
					     GetRootDevice(), filename_utf8);

			//Test if game.iso exists and add to list
			bool found = false;

			u32 i;
			for (i = 0; i < 8; i++)
			{
				const u32 discNumber = i / 4;

				// Append the disc filename.
				strcpy(&filename[fnlen], disc_filenames[i]);

				// Attempt to load disc information.
				if (IsDiscImageValid(filename, discNumber, &gi[gamecount]))
				{
					// Disc image exists and is a GameCube disc.
					gamecount++;
					found = true;
					// Next disc number.
					i = (discNumber * 4) + 3;
				}
			}

			// If none of the expected files were found, check for FST format.
			if (!found)
			{
				// Read the disc header from boot.bin.
				memcpy(&filename[fnlen], "sys/boot.bin", 13);
				if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) != FR_OK)
					continue;

				UINT read;
				f_read(&in, buf, 0x100, &read);
				f_close(&in);
				if (read != 0x100 || !IsGCGame(buf))
					continue;

				// Read the BI2.bin region code.
				memcpy(&filename[fnlen], "sys/bi2.bin", 12);
				if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) != FR_OK)
					continue;

				u32 BI2region;
				f_lseek(&in, 0x18);
				f_read(&in, &BI2region, sizeof(BI2region), &read);
				f_close(&in);
				if (read != sizeof(BI2region))
					continue;

				// Terminate the filename at the game's base directory.
				filename[fnlen] = 0;

				// Make sure the title in the header is NULL terminated.
				buf[0x20+65] = 0;

				memcpy(gi[gamecount].ID, buf, 6); //ID for EXI
				gi[gamecount].Revision = 0;

				// TODO: Check titles.txt?
				gi[gamecount].Name = strdup((const char*)&buf[0x20]);
				gi[gamecount].Flags = GIFLAG_NAME_ALLOC | GIFLAG_FORMAT_FST | ((BI2region & 3) << 3);

				gi[gamecount].Path = strdup(filename);
				gamecount++;
			}
		}
		else
		{
			// Regular file.

			// Make sure its extension is ".iso" or ".gcm".
			const char *filename_utf8 = wchar_to_char(fInfo.fname);
			if (IsSupportedFileExt(filename_utf8))
			{
				// Create the full pathname.
				snprintf(filename, sizeof(filename), "%s:/games/%s",
					 GetRootDevice(), filename_utf8);

				// Attempt to load disc information.
				// (NOTE: Only disc 1 is supported right now.)
				// Disc image exists and is a GameCube disc.
				if (IsDiscImageValid(filename, 0, &gi[gamecount]))
					gamecount++;
			}
		}

		if (gamecount >= sz)	//if array is full
			break;
	}
	f_closedir(&pdir);

	// Sort the list alphabetically.
	// On Wii, the pseudo-entry for GameCube discs is always kept at the top.
	if(gamecount > 1)
		qsort(&gi[1], gamecount-1, sizeof(gameinfo), compare_names);

	// Save the game count.
	if (pGameCount)
		*pGameCount = gamecount;

	if(gamecount == 0)
		return DEV_NO_TITLES;

	initMeleeCodeSelections();

	return DEV_OK;
}

/* Menu_GameSelection_InputHandler()
 * Handle controller inputs for the game selection menu.
 */
static bool Menu_GameSelection_InputHandler(MenuCtx *ctx)
{
	if(FPAD_X(0))
	{
		// Can we show information for the selected game?
		if (ctx->games.canShowInfo)
		{
			// Show game information.
			ShowGameInfo(&ctx->games.gi[ctx->games.posX + ctx->games.scrollX]);
			ctx->redraw = true;
		}
	}

	if (FPAD_Down_Repeat(ctx))
	{
		// Remove the current arrow.
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );

		// Adjust the scrolling position.
		if (ctx->games.posX + 1 >= ctx->games.listMax)
		{
			// Need to adjust the scroll position.
			if (ctx->games.posX + 1 + ctx->games.scrollX < ctx->games.gamecount)
				ctx->games.scrollX++;
			// Otherwise, wrap around
			else 
			{
				ctx->games.posX	= 0;
				ctx->games.scrollX = 0;
			}
		} 
		else 
			ctx->games.posX++;

		ctx->redraw = true;
		ctx->saveSettings = true;
	}

	if (FPAD_Right_Repeat(ctx))
	{
		// Remove the current arrow.
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );

		// Adjust the scrolling position.
		if (ctx->games.posX == ctx->games.listMax - 1)
		{
			if (ctx->games.posX + ctx->games.listMax + ctx->games.scrollX < ctx->games.gamecount)
				ctx->games.scrollX += ctx->games.listMax;
			else if (ctx->games.posX + ctx->games.scrollX != ctx->games.gamecount - 1)
				ctx->games.scrollX = ctx->games.gamecount - ctx->games.listMax;
			else 
			{
				ctx->games.posX	= 0;
				ctx->games.scrollX = 0;
			}
		} 
		else if (ctx->games.listMax) 
			ctx->games.posX = ctx->games.listMax - 1;
		else 
			ctx->games.posX	= 0;

		ctx->redraw = true;
		ctx->saveSettings = true;
	}

	if (FPAD_Up_Repeat(ctx))
	{
		// Remove the current arrow.
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );

		// Adjust the scrolling position.
		if (ctx->games.posX <= 0)
		{
			if (ctx->games.scrollX > 0)
				ctx->games.scrollX--;
			else 
			{
				// Wraparound.
				if(ctx->games.listMax)
					ctx->games.posX = ctx->games.listMax - 1;
				else
					ctx->games.posX	= 0;
				ctx->games.scrollX = ctx->games.gamecount - ctx->games.listMax;
			}
		} 
		else 
			ctx->games.posX--;

		ctx->redraw = true;
		ctx->saveSettings = true;
	}

	if (FPAD_Left_Repeat(ctx))
	{
		// Remove the current arrow.
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );

		// Adjust the scrolling position.
		if (ctx->games.posX == 0)
		{
			if (ctx->games.scrollX - (s32)ctx->games.listMax >= 0)
				ctx->games.scrollX -= ctx->games.listMax;
			else if (ctx->games.scrollX != 0)
				ctx->games.scrollX = 0;
			else
				ctx->games.scrollX = ctx->games.gamecount - ctx->games.listMax;
		} 
		else 
		{
			ctx->games.posX = 0;
		}

		ctx->redraw = true;
		ctx->saveSettings = true;
	}

	// User selected a game.
	if (FPAD_OK(0) && ctx->games.canBeBooted)
	{
		ctx->selected = true;
		return true;
	}
	return false;
}


/* Menu_GameSelection_Redraw()
 * Redraw the game selection menu.
 */
extern u32 usb_replays_left;
extern u32 usb_attached;
static void Menu_GameSelection_Redraw(MenuCtx *ctx)
{
	u32 i;

	// Starting position.
	int gamelist_y = MENU_POS_Y + 20*5 + 10;

	const gameinfo *gi = &ctx->games.gi[ctx->games.scrollX];
	int gamesToPrint = ctx->games.gamecount - ctx->games.scrollX;
	if (gamesToPrint > ctx->games.listMax)
		gamesToPrint = ctx->games.listMax;

	for (i = 0; i < gamesToPrint; ++i, gamelist_y += 20, gi++)
	{
		// FIXME: Print all 64 characters of the game name?
		// Currently truncated to 50.

		// Determine color based on disc format.
		// NOTE: On Wii, DISC01 is GIFLAG_FORMAT_FULL.
		const u32 color = DiscFormatColors[gi->Flags & GIFLAG_FORMAT_MASK];

		const u8 discNumber = ((gi->Flags & GIFLAG_DISCNUMBER_MASK) >> 5);
		if (discNumber == 0)
		{
			// Disc 1.
			PrintFormat(DEFAULT_SIZE, color, MENU_POS_X, gamelist_y,
				    "%50.50s [%.6s]%s",
				    gi->Name, gi->ID,
				    i == ctx->games.posX ? ARROW_LEFT : " ");
		}
		else
		{
			// Disc 2 or higher.
			PrintFormat(DEFAULT_SIZE, color, MENU_POS_X, gamelist_y,
				    "%46.46s (%d) [%.6s]%s",
				    gi->Name, discNumber+1, gi->ID,
				    i == ctx->games.posX ? ARROW_LEFT : " ");
		}

		// Render current Slippi settings if a Melee ISO is selected
		if ((strncmp(gi->ID, "GALE01", 6) == 0) && (i == ctx->games.posX))
		{
			gamelist_y += 20;

			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+340, gamelist_y, "[Slippi] ");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+340+(9*10), gamelist_y, "NET: ");
			PrintFormat(DEFAULT_SIZE, (ncfg->Config & (NIN_CFG_NETWORK)) ? GREEN : RED, 
				MENU_POS_X+340+(13*10), gamelist_y, "%-3s", (ncfg->Config & (NIN_CFG_NETWORK)) ? "ON" : "OFF");

			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+340+(17*10), gamelist_y, "FILE: ");
			PrintFormat(DEFAULT_SIZE, (ncfg->Config & (NIN_CFG_SLIPPI_FILE_WRITE)) ? GREEN : RED, MENU_POS_X+340+(22*10),
					gamelist_y, "%-3s", (ncfg->Config & (NIN_CFG_SLIPPI_FILE_WRITE)) ? "ON" : "OFF");

			// Warn the user if they're running low on USB disk space
			if ((usb_attached == 1) && (ncfg->Config & (NIN_CFG_SLIPPI_FILE_WRITE)))
			{
				int lowUsbWarnThreshold = 500;
				int lowUsbErrorThreshold = 50;

				if ((usb_replays_left < lowUsbWarnThreshold) && (usb_replays_left > lowUsbErrorThreshold))
					PrintFormat(MENU_SIZE, ORANGE, MENU_POS_X, SettingY(11),"[!] WARNING, LOW USB SPACE");
				if (usb_replays_left <= lowUsbErrorThreshold)
					PrintFormat(MENU_SIZE, RED, MENU_POS_X, SettingY(11),"[!] WARNING, LOW USB SPACE");

				if (usb_replays_left < lowUsbWarnThreshold) {
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(12), "Your USB drive is running");
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(13), "low on free space. There ");
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(14), "should be enough space for");
					PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(15), "about %d more replays.", 
							(int)usb_replays_left);
				}
			}
		}
	}

	if(ctx->games.gamecount && (ctx->games.scrollX + ctx->games.posX) >= 0 
		&& (ctx->games.scrollX + ctx->games.posX) < ctx->games.gamecount)
	{
		ctx->games.canBeBooted = true;

		// Don't display information when selecting the disc drive
		if ((ctx->games.scrollX + ctx->games.posX) == 0)
			ctx->games.canShowInfo = false;
		else
			ctx->games.canShowInfo = true;

		if (ctx->games.canShowInfo)
		{
			// Print the selected game's filename.
			const gameinfo *const gi = &ctx->games.gi[ctx->games.scrollX + ctx->games.posX];
			const int len = strlen(gi->Path);
			const int x = (640 - (len*10)) / 2;

			const u32 color = DiscFormatColors[gi->Flags & GIFLAG_FORMAT_MASK];
			PrintFormat(DEFAULT_SIZE, color, x, MENU_POS_Y + 20*4+5, "%s", gi->Path);

			bool isGALE = (strncmp(gi->ID, "GALE01", 6) == 0);
			bool isRev2 = (gi->Revision == 0x02);
			if (!isGALE || (isGALE && !isRev2))
			{
				PrintFormat(MENU_SIZE, ORANGE, MENU_POS_X, SettingY(11),"[!] WARNING, PLEASE READ ");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(12), "Project Slippi Nintendont");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(13), "supports the NTSC v1.02");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(14), "version of Melee (GALE01).");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X, SettingY(15), "This is not NTSC v1.02.");

				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X,SettingY(17), "This game may not behave");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X,SettingY(18), "correctly. Please use the");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X,SettingY(19), "vanilla Nintendont build");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X,SettingY(20), "for the best experience.");
			}
		}
	}
	else
	{
		//invalid title selected
		ctx->games.canBeBooted = false;
		ctx->games.canShowInfo = false;
	}
}


/**
 * Update the Game Select menu.
 * @param ctx	[in] Menu context.
 * @return True to exit; false to continue.
 */
static bool Menu_GameSelection_Handler(MenuCtx *ctx)
{
	// If input handler returns true, the user has selected a game to boot
	if (Menu_GameSelection_InputHandler(ctx))
		return true;

	// Otherwise, potentially redraw the game list and return
	if (ctx->redraw) 
		Menu_GameSelection_Redraw(ctx);

	return false;
}

/* GetMeleeDescription()
 * Get description text for Gecko code related Slippi settings.
 */
static const char *const *GetMeleeDescription(int posX)
{
	// Codes start at index NIN_SLIPPI_DYNAMIC_CODES_START
	int index = posX - NIN_SLIPPI_DYNAMIC_CODES_START; 

	const MeleeCodeConfig *codeConfig = GetMeleeCodeConfig();

	// If outside of the range for codes, do nothing
	if (index < 0 || index > codeConfig->lineItemCount)
		return NULL;

	const MeleeCodeLineItem *item = codeConfig->items[index];
	return item->description;
}

/**
 * Get a description for the Settings menu.
 * @param ctx	[in] Menu context.
 * @return Description, or NULL if none is available.
 */
static const char *const *GetSettingsDescription(const MenuCtx *ctx)
{
	switch (ctx->pages.selected) {
	case PAGE_SETTINGS:
		switch (ctx->settings.posX) {
		case NIN_CFG_BIT_CHEATS: 
			return desc_cheats;
		//case NIN_CFG_BIT_MEMCARDEMU: 
		//	return desc_mcemu;
		//case NIN_CFG_BIT_FORCE_WIDE: 
		//	return desc_force_wide;
		case NIN_CFG_BIT_FORCE_PROG: 
			return desc_force_prog;
		//case NIN_CFG_BIT_LED: 
		//	return desc_led;
		//case NIN_CFG_BIT_REMLIMIT: 
		//	return desc_remlimit;
		case NIN_SETTINGS_LANGUAGE: 
			return desc_language;
		case NIN_SETTINGS_MEMCARDBLOCKS: 
			return desc_memcard_blocks;
		//case NIN_SETTINGS_MEMCARDMULTI: 
		//	return desc_memcard_multi;
		case NIN_CFG_AUTO_BOOT:
		default: 
			return NULL;
		}
	case PAGE_SLIPPI_SETTINGS:
		switch (ctx->settings.posX) {
		case NIN_SLIPPI_NETWORKING: 
			return desc_networking;
		case NIN_SLIPPI_FILE_WRITE: 
			return desc_slippi_file_write;
		case NIN_SLIPPI_PORT_A: 
			return desc_slippi_port_a;
		case NIN_SLIPPI_CUSTOM_CODES:
			return desc_cheats;
		default: 
			return GetMeleeDescription(ctx->settings.posX);
		}
	default: 
		return NULL;
	}
}

/* Menu_Settings_InputHandler()
 * Handle inputs for the settings menu.
 */
static void Menu_Settings_InputHandler(MenuCtx *ctx)
{
	if (FPAD_Down_Repeat(ctx))
	{
		// Down: Move the cursor down by 1 setting.
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 30, SettingY(ctx->settings.posX), " " );
		ctx->settings.posX++;

		switch (ctx->pages.selected) {
		case PAGE_SETTINGS:
			// Wrap around to the beginning if we're at the end
			if (ctx->settings.posX > NIN_SETTINGS_LAST - 1)
				ctx->settings.posX = 0;
			// Some items are hidden if certain values aren't set.
			if (((ncfg->VideoMode & NIN_VID_FORCE) == 0) &&
			    (ctx->settings.posX == NIN_SETTINGS_VIDEOMODE))
			{
				ctx->settings.posX++;
			}
			if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) &&
			    (ctx->settings.posX == NIN_SETTINGS_MEMCARDBLOCKS))
			{
				ctx->settings.posX++;
			}
			if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) &&
			    (ctx->settings.posX == NIN_SETTINGS_MEMCARDMULTI))
			{
				ctx->settings.posX++;
			}
			break;
		case PAGE_SLIPPI_SETTINGS: ;
			const MeleeCodeConfig *codeConfig = GetMeleeCodeConfig();

			// Handle slippi page position
			if (ctx->settings.posX > NIN_SLIPPI_DYNAMIC_CODES_START + codeConfig->lineItemCount - 1)
				ctx->settings.posX = 0;
			else if (ctx->settings.posX == NIN_SLIPPI_BLANK_1 || 
				ctx->settings.posX == NIN_SLIPPI_BLANK_2)
			{
				// Settings 3 and 4 are skipped
				ctx->settings.posX = NIN_SLIPPI_DYNAMIC_CODES_START;
			}
			break;
		}
		ctx->redraw = true;
	}
	else if (FPAD_Up_Repeat(ctx))
	{
		// Up: Move the cursor up by 1 setting.
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 30, SettingY(ctx->settings.posX), " " );
		ctx->settings.posX--;

		switch (ctx->pages.selected) {
		case PAGE_SETTINGS:
			// Wrap around to the last entry
			if (ctx->settings.posX < 0)
				ctx->settings.posX = NIN_SETTINGS_LAST - 1;

			// Some items are hidden if certain values aren't set.
			if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (ctx->settings.posX == NIN_SETTINGS_MEMCARDMULTI))
				ctx->settings.posX--;
			if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) && (ctx->settings.posX == NIN_SETTINGS_MEMCARDBLOCKS))
				ctx->settings.posX--;
			if (((ncfg->VideoMode & NIN_VID_FORCE) == 0) && (ctx->settings.posX == NIN_SETTINGS_VIDEOMODE))
				ctx->settings.posX--;
			break;
		case PAGE_SLIPPI_SETTINGS: ;
			const MeleeCodeConfig *codeConfig = GetMeleeCodeConfig();

			// Handle slippi page positioning
			if (ctx->settings.posX < 0)
			{
				ctx->settings.posX = NIN_SLIPPI_DYNAMIC_CODES_START + codeConfig->lineItemCount - 1;
			}

			if (ctx->settings.posX == NIN_SLIPPI_BLANK_1 || ctx->settings.posX == NIN_SLIPPI_BLANK_2)
			{
				// Blank spots are skipped
				ctx->settings.posX = NIN_SLIPPI_DYNAMIC_CODES_START - 3;
			}
			break;
		}
		ctx->redraw = true;
	}

	if (FPAD_Left_Repeat(ctx))
	{
		if (ctx->pages.selected == PAGE_SETTINGS)
		{
			ctx->saveSettings = true;
			switch (ctx->settings.posX) {
			case NIN_SETTINGS_VIDEO_WIDTH:
				// Video width.
				if (ncfg->VideoScale == 0)
				{
					ncfg->VideoScale = 120;
				} 
				else 
				{
					ncfg->VideoScale -= 2;
					if (ncfg->VideoScale < 40 || ncfg->VideoScale > 120)
						ncfg->VideoScale = 0; //auto
				}
				ReconfigVideo(rmode);
				ctx->redraw = true;
				break;
			case NIN_SETTINGS_SCREEN_POSITION:
				// Screen position.
				ncfg->VideoOffset--;
				if (ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20)
					ncfg->VideoOffset = 20;
				ReconfigVideo(rmode);
				ctx->redraw = true;
				break;
			default:
				break;
			}
		}
	}
	else if (FPAD_Right_Repeat(ctx))
	{
		if (ctx->pages.selected == PAGE_SETTINGS)
		{
			ctx->saveSettings = true;
			switch (ctx->settings.posX) {
			case NIN_SETTINGS_VIDEO_WIDTH:
				// Video width.
				if (ncfg->VideoScale == 0)
				{
					ncfg->VideoScale = 40;
				}
				else 
				{
					ncfg->VideoScale += 2;
					if (ncfg->VideoScale < 40 || ncfg->VideoScale > 120)
						ncfg->VideoScale = 0; //auto
				}
				ReconfigVideo(rmode);
				ctx->redraw = true;
				break;
			case NIN_SETTINGS_SCREEN_POSITION:
				// Screen position.
				ncfg->VideoOffset++;
				if (ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20)
					ncfg->VideoOffset = -20;
				ReconfigVideo(rmode);
				ctx->redraw = true;
				break;
			default:
				break;
			}
		}
	}

	if (FPAD_OK(0))
	{
		if (ctx->pages.selected == PAGE_SETTINGS)
		{
			// Left column.
			ctx->saveSettings = true;
			if (ctx->settings.posX < NIN_CFG_BIT_LAST)
			{
				// NOTE: Disable memcard emulation for now
				if (ctx->settings.posX == NIN_CFG_BIT_MEMCARDEMU)
					ncfg->Config &= ~NIN_CFG_BIT_MEMCARDEMU;
				else
					ncfg->Config ^= (1 << ctx->settings.posX);
			}
			else switch (ctx->settings.posX) {
			case NIN_SETTINGS_LANGUAGE:
				ncfg->Language++;
				if (ncfg->Language > NIN_LAN_DUTCH)
					ncfg->Language = NIN_LAN_AUTO;
				break;
			case NIN_SETTINGS_VIDEO:
			{
				u32 Video = (ncfg->VideoMode & NIN_VID_MASK);
				switch (Video) {
				case NIN_VID_AUTO:
					Video = NIN_VID_FORCE;
					break;
				case NIN_VID_FORCE:
					Video = NIN_VID_FORCE | NIN_VID_FORCE_DF;
					break;
				case NIN_VID_FORCE | NIN_VID_FORCE_DF:
					Video = NIN_VID_NONE;
					break;
				default:
				case NIN_VID_NONE:
					Video = NIN_VID_AUTO;
					break;
				}
				ncfg->VideoMode &= ~NIN_VID_MASK;
				ncfg->VideoMode |= Video;
				break;
			}
			case NIN_SETTINGS_VIDEOMODE:
			{
				u32 Video = (ncfg->VideoMode & NIN_VID_FORCE_MASK);
				Video = Video << 1;
				if (Video > NIN_VID_FORCE_MPAL) {
					Video = NIN_VID_FORCE_PAL50;
				}
				ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
				ncfg->VideoMode |= Video;
				break;
			}

			//case NIN_SETTINGS_MEMCARDBLOCKS:
			//	ncfg->MemCardBlocks++;
			//	if (ncfg->MemCardBlocks > MEM_CARD_MAX) {
			//		ncfg->MemCardBlocks = 0;
			//	}
			//	break;
			//case NIN_SETTINGS_MEMCARDMULTI:
			//	ncfg->Config ^= (NIN_CFG_MC_MULTI);
			//	break;

			case NIN_SETTINGS_PAL50_PATCH:
				// PAL50 patch.
				//ctx->saveSettings = true;
				ncfg->VideoMode ^= (NIN_VID_PATCH_PAL50);
				//ctx->redraw = true;
				break;

			//case NIN_SETTINGS_SKIP_IPL:
			//	// Skip IPL
			//	//ctx->saveSettings = true;
			//	ncfg->Config ^= (NIN_CFG_SKIP_IPL);
			//	//ctx->redraw = true;
			//	break;

			default:
				break;
			}

			// Blank out the memory card options if MCEMU is disabled.
			if (!(ncfg->Config & NIN_CFG_MEMCARDEMU))
			{
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDBLOCKS), "%29s", "");
				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDMULTI), "%29s", "");
			}
			ctx->redraw = true;
		}
		else if (ctx->pages.selected == PAGE_SLIPPI_SETTINGS)
		{
			ctx->saveSettings = true;
			ctx->redraw = true;

			// Slippi page settings
			switch (ctx->settings.posX) {
			case NIN_SLIPPI_NETWORKING:
				ncfg->Config ^= (NIN_CFG_NETWORK);
				break;
			case NIN_SLIPPI_FILE_WRITE:
				ncfg->Config ^= (NIN_CFG_SLIPPI_FILE_WRITE);
				break;
			case NIN_SLIPPI_PORT_A:
				ncfg->Config ^= (NIN_CFG_SLIPPI_PORT_A);
				break;
			case NIN_SLIPPI_CUSTOM_CODES:
				ncfg->Config ^= (NIN_CFG_CHEATS);
				break;
			default: ; // need semicolon to declare variable on next line
				const MeleeCodeConfig *codeConfig = GetMeleeCodeConfig();
				int index = ctx->settings.posX - NIN_SLIPPI_DYNAMIC_CODES_START;
				if (index < 0 || index > codeConfig->lineItemCount) {
					// If outside of the range for codes, do nothing
					ctx->saveSettings = false;
					ctx->redraw = false;
					break;
				}

				const MeleeCodeLineItem *item = codeConfig->items[index];

				meleeCodeSelectionIndices[index]++;
				if (meleeCodeSelectionIndices[index] >= item->optionCount) {
					meleeCodeSelectionIndices[index] = 0;
				}

				const MeleeCodeOption *option = item->options[meleeCodeSelectionIndices[index]];

				// Set the value of the option in the config
				ncfg->MeleeCodeOptions[item->identifier] = option->value;
				break;
			}
		}
	}
}

/* Menu_Settings_Redraw()
 * Redraw the settings menu.
 */
static void Menu_Settings_Redraw(MenuCtx *ctx)
{
	u32 ListLoopIndex = 0;

	if (ctx->pages.selected == PAGE_SETTINGS)
	{
		// Standard boolean settings.
		for (; ListLoopIndex < NIN_CFG_BIT_LAST; ListLoopIndex++)
		{
			u32 item_color = BLACK;

			// NOTE: Gray out memcard emulation for now
			if (ListLoopIndex == NIN_CFG_BIT_MEMCARDEMU)
				item_color = GRAY;

			PrintFormat(MENU_SIZE, item_color, MENU_POS_X+50,
				SettingY(ListLoopIndex), "%-18s:%s",
				OptionStrings[ListLoopIndex],
				(ncfg->Config & (1 << ListLoopIndex)) ? "On " : "Off" );
		}

		// Language setting.
		u32 LanIndex = ncfg->Language;
		if (LanIndex < NIN_LAN_FIRST || LanIndex >= NIN_LAN_LAST) {
			LanIndex = NIN_LAN_LAST; //Auto
		}
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),
				"%-18s:%-4s", OptionStrings[ListLoopIndex], LanguageStrings[LanIndex] );
		ListLoopIndex++;

		// Video mode forcing.
		u32 VideoModeIndex;
		u32 VideoModeVal = ncfg->VideoMode & NIN_VID_MASK;
		switch (VideoModeVal) {
		case NIN_VID_AUTO:
			VideoModeIndex = NIN_VID_INDEX_AUTO;
			break;
		case NIN_VID_FORCE:
			VideoModeIndex = NIN_VID_INDEX_FORCE;
			break;
		case NIN_VID_FORCE | NIN_VID_FORCE_DF:
			VideoModeIndex = NIN_VID_INDEX_FORCE_DF;
			break;
		case NIN_VID_NONE:
			VideoModeIndex = NIN_VID_INDEX_NONE;
			break;
		default:
			ncfg->VideoMode &= ~NIN_VID_MASK;
			ncfg->VideoMode |= NIN_VID_AUTO;
			VideoModeIndex = NIN_VID_INDEX_AUTO;
			break;
		}
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),
				"%-18s:%-18s", OptionStrings[ListLoopIndex], VideoStrings[VideoModeIndex] );
		ListLoopIndex++;

		if( ncfg->VideoMode & NIN_VID_FORCE )
		{
			// Video mode selection.
			// Only available if video mode is Force or Force (Deflicker).
			VideoModeVal = ncfg->VideoMode & NIN_VID_FORCE_MASK;
			u32 VideoTestVal = NIN_VID_FORCE_PAL50;
			for (VideoModeIndex = 0; (VideoTestVal <= NIN_VID_FORCE_MPAL) && (VideoModeVal != VideoTestVal); VideoModeIndex++) {
				VideoTestVal <<= 1;
			}

			if ( VideoModeVal < VideoTestVal )
			{
				ncfg->VideoMode &= ~NIN_VID_FORCE_MASK;
				ncfg->VideoMode |= NIN_VID_FORCE_NTSC;
				VideoModeIndex = NIN_VID_INDEX_FORCE_NTSC;
			}
			PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),
					"%-18s:%-5s", OptionStrings[ListLoopIndex], VideoModeStrings[VideoModeIndex] );
		}
		ListLoopIndex++;

		// Memory card emulation.
		if ((ncfg->Config & NIN_CFG_MEMCARDEMU) == NIN_CFG_MEMCARDEMU)
		{
			u32 MemCardBlocksVal = ncfg->MemCardBlocks;
			if (MemCardBlocksVal > MEM_CARD_MAX) {
				MemCardBlocksVal = 0;
			}
			PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
					"%-18s:%-4d%s", OptionStrings[ListLoopIndex], MEM_CARD_BLOCKS(MemCardBlocksVal), MemCardBlocksVal > 2 ? "Unstable" : "");
			PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex+1),
					"%-18s:%-4s", OptionStrings[ListLoopIndex+1], (ncfg->Config & (NIN_CFG_MC_MULTI)) ? "On " : "Off");
		}
		ListLoopIndex+=2;

		// Patch PAL50.
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Patch PAL50", (ncfg->VideoMode & (NIN_VID_PATCH_PAL50)) ? "On " : "Off");
		ListLoopIndex++;

		// Skip GameCube IPL
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Skip IPL", (ncfg->Config & (NIN_CFG_SKIP_IPL)) ? "Yes" : "No ");
		ListLoopIndex++;

		// Video width.
		char vidWidth[10];
		if (ncfg->VideoScale < 40 || ncfg->VideoScale > 120) {
			ncfg->VideoScale = 0;
			snprintf(vidWidth, sizeof(vidWidth), "Auto");
		} else {
			snprintf(vidWidth, sizeof(vidWidth), "%i", ncfg->VideoScale + 600);
		}

		char vidOffset[10];
		if (ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20) {
			ncfg->VideoOffset = 0;
		}
		snprintf(vidOffset, sizeof(vidOffset), "%i", ncfg->VideoOffset);

		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Video Width", vidWidth);
		ListLoopIndex++;
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Screen Position", vidOffset);
		ListLoopIndex++;
	} 
	else 
	{
		// Networking
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Slippi Networking", (ncfg->Config & (NIN_CFG_NETWORK)) ? "Yes" : "No ");
		ListLoopIndex++;

		// Slippi USB
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Slippi File Write", (ncfg->Config & (NIN_CFG_SLIPPI_FILE_WRITE)) ? "Yes" : "No ");
		ListLoopIndex++;

		// Slippi Port A
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Slippi on Port A", (ncfg->Config & (NIN_CFG_SLIPPI_PORT_A)) ? "Yes" : "No ");
		ListLoopIndex++;

		// Custom Cheats
		PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
				"%-18s:%-4s", "Custom Cheats/GCT", (ncfg->Config & (NIN_CFG_CHEATS)) ? "Yes" : "No ");
		ListLoopIndex += 2;

		PrintFormat(14, DARK_BLUE, MENU_POS_X + 50, SettingY(ListLoopIndex), "MELEE CODES");
		ListLoopIndex++;

		const MeleeCodeConfig *codeConfig = GetMeleeCodeConfig();
		
		int i;
		for (i = 0; i < codeConfig->lineItemCount; i++) {
				const MeleeCodeLineItem *item = codeConfig->items[i];
				const MeleeCodeOption *option = item->options[meleeCodeSelectionIndices[i]];

				PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
						"%-15s:%s", item->name, option->name);
				ListLoopIndex++;
		}
	}

	// Draw the cursor
	u32 cursor_color = BLACK;
	PrintFormat(MENU_SIZE, cursor_color, MENU_POS_X + 30, SettingY(ctx->settings.posX), ARROW_RIGHT);

	// Print a description for the selected option.
	// `desc` contains pointers to lines and is terminated with NULL.
	const char *const *desc = GetSettingsDescription(ctx);
	if (desc != NULL)
	{
		int descFontSize = 14;
		int line_num = 0;
		do {
			if (**desc != 0)
			{
				PrintFormat(descFontSize, BLACK, MENU_POS_X + 300, 127 + (descFontSize * line_num), *desc);
			}
			line_num++;
		} while (*(++desc) != NULL);
	}
}


/**
 * Update the Settings menu.
 * @param ctx	[in] Menu context.
 * @return True to exit; false to continue.
 */
static bool Menu_Settings_Handler(MenuCtx *ctx)
{
	// Handle inputs on the settings page
	Menu_Settings_InputHandler(ctx);

	if (ctx->redraw) 
		Menu_Settings_Redraw(ctx);

	return false;
}

static void PrintDevInfo(void)
{
	// Device type.
	const char *s_devType = (UseSD ? "SD" : "USB");

	// Device state.
	// NOTE: If this is showing a message, the game list
	// will be moved down by 1 row, which usually isn't
	// a problem, since it will either be empty or showing
	// "Boot GC Disc in Drive".
	switch (devState) {
	case DEV_NO_OPEN:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
			"WARNING: %s FAT device could not be opened.", s_devType);
		break;
	case DEV_NO_GAMES:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
			"WARNING: %s:/games/ was not found.", GetRootDevice());
		break;
	case DEV_NO_TITLES:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
			"WARNING: %s:/games/ contains no GC titles.", GetRootDevice());
		break;
	default:
		break;
	}
}


/**
 * Select a game from the specified device.
 * @return Bitfield indicating the user's selection:
 * - 0 == go back
 * - 1 == game selected
 * - 2 == go back and save settings (UNUSED)
 * - 3 == game selected and save settings
 */
static int Menu_GameSelection(void)
{
	ShowLoadingScreen();

	// Load the list of games for this device
	u32 gamecount = 0;
	gameinfo gi[MAX_GAMES];
	devState = LoadGameList(&gi[0], MAX_GAMES, &gamecount);

	switch (devState) {

	// Game list loaded successfully
	case DEV_OK:
		break;

	// No "games" directory was found
	case DEV_NO_GAMES:
		gprintf("WARNING: %s:/games/ was not found.\n", GetRootDevice());
		break;

	// "games" directory appears to be empty.
	case DEV_NO_TITLES:
		gprintf("WARNING: %s:/games/ contains no GC titles.\n", GetRootDevice());
		break;

	// Couldn't open device
	case DEV_NO_OPEN:
	default: {
		const char *s_devType = (UseSD ? "SD" : "USB");
		gprintf("No %s FAT device found.\n", s_devType);
		break;
		}
	}

	// Initialize the menu context.
	MenuCtx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.menuMode = 0;	// Start in the games list.
	ctx.redraw = true;	// Redraw initially.
	ctx.selected = false;	// Set to TRUE if the user selected a game.
	ctx.saveSettings = false;

	// Initialize ctx.games with the entries from the selected device
	ctx.games.listMax = gamecount;
	if (ctx.games.listMax > 12)
		ctx.games.listMax = 12;
	ctx.games.gi = gi;
	ctx.games.gamecount = gamecount;

	// Initialize ctx.pages
	ctx.pages.index = 0;
	ctx.pages.selected = 0;
	ctx.pages.count = 2;
	ctx.pages.pages = allPages;

	// Set the default game
	u32 i;
	for (i = 0; i < gamecount; ++i)
	{
		if (strcasecmp(strchr(gi[i].Path,':')+1, ncfg->GamePath) == 0)
		{
			// Need to adjust the scroll position.
			if (i >= ctx.games.listMax)
			{
				ctx.games.posX    = ctx.games.listMax - 1;
				ctx.games.scrollX = i - ctx.games.listMax + 1;
			} 
			// No scroll position adjustment is required.
			else 
			{
				ctx.games.posX = i;
			}
			break;
		}
	}

	// Video loop
	while(1)
	{
		VIDEO_WaitVSync();
		FPAD_Update();
		if(Shutdown) LoaderShutdown();

		// Go back to the Device Selection menu
		if (FPAD_Start(0))
		{
			ctx.selected = false;
			break;
		}

		// Handle a request to move into the settings menu
		if (FPAD_Cancel(0))
		{
			ctx.menuMode = !ctx.menuMode;
			memset(&ctx.held, 0, sizeof(ctx.held));

			// Reset settings menu structure
			if (ctx.menuMode == 1)
			{
				ctx.settings.posX = 0;
				ctx.pages.index = 0;
				ctx.settings.settingPart = 0;
			}
			ctx.redraw = 1;
		}

		if (ctx.menuMode != 0 && (FPAD_RTrigger(0) || FPAD_X(0)))
		{
			ctx.pages.index = (ctx.pages.index + 1) % ctx.pages.count;
		}

		if (ctx.menuMode != 0 && (FPAD_LTrigger(0) || FPAD_Y(0)))
		{
			ctx.pages.index = (ctx.pages.index - 1 + ctx.pages.count) % ctx.pages.count;
		}

		u32 newSelected = ctx.pages.pages[ctx.pages.index].ID;
		if (ctx.pages.selected != newSelected)
		{
			ctx.pages.selected = newSelected;
			ctx.settings.posX = 0;
			ctx.settings.settingPart = 0;
			ctx.redraw = 1;
		}
		
		bool ret = false;
		if (ctx.menuMode == 0)
			ret = Menu_GameSelection_Handler(&ctx);
		else 
			ret = Menu_Settings_Handler(&ctx);

		// User has exited the menu.
		if (ret) break;


		if (ctx.redraw)
		{
			// Print version/build info on all menus
			PrintBuildInfo();

			// Draw header text for game selection menu
			if (ctx.menuMode == 0)
			{
				PrintButtonActions("Go Back", NULL, "Settings", NULL);

				// If the selected game bootable, enable "Select".
				u32 color = ((ctx.games.canBeBooted) ? BLACK : DARK_GRAY);
				PrintFormat(DEFAULT_SIZE, color, MENU_POS_X + 410, MENU_POS_Y + 20*1, "A   : Select");

				// If the selected game is not DISC01, enable "Game Info".
				color = ((ctx.games.canShowInfo) ? BLACK : DARK_GRAY);
				PrintFormat(DEFAULT_SIZE, color, MENU_POS_X + 410, MENU_POS_Y + 20*3, "X   : Game Info");
			}
			// Draw header text for the settings menu
			else
			{
				PrintButtonActions("Go Back", "Select", "Game List", NULL);

				// Print Tab Information
				int leftOffset = 140;
				int rightOffset = 440;
				int charWidth = 10;

				PrintFormat(DEFAULT_SIZE, DARK_BLUE, MENU_POS_X + leftOffset, MENU_POS_Y + 20*3 + 10, "%sL", CHEVRON_LEFT);
				PrintFormat(DEFAULT_SIZE, DARK_BLUE, MENU_POS_X + rightOffset, MENU_POS_Y + 20*3 + 10, "R%s", CHEVRON_RIGHT);

				char *tabName = ctx.pages.pages[ctx.pages.index].Name;
				int tabNameLen = strlen(tabName);
				int emptySpace = ((2 * charWidth) + rightOffset - leftOffset) - (tabNameLen * charWidth);
				int nameOffset = leftOffset + (emptySpace / 2);

				PrintFormat(DEFAULT_SIZE, DARK_BLUE, MENU_POS_X + nameOffset, MENU_POS_Y + 20*3 + 10, tabName);

				// Print tab count
				if (ctx.pages.count > 1)
				{
					PrintFormat(14, DARK_BLUE, MENU_POS_X + 280, MENU_POS_Y + 20*2 + 14, "%d / %d", ctx.pages.index + 1, ctx.pages.count);
				}
			}

			// FIXME: If devState != DEV_OK,
			// the device info overlaps with the settings menu.
			if (ctx.menuMode == 0 || (ctx.menuMode == 1 && devState == DEV_OK))
				PrintDevInfo();

			// Render the screen.
			GRRLIB_Render();
			ClearScreen();
			ctx.redraw = false;
		}
	}

	// Save the selected game to the configuration
	if(ctx.games.canBeBooted)
	{
		u32 SelectedGame = ctx.games.posX + ctx.games.scrollX;
		const char* StartChar = gi[SelectedGame].Path + 3;
		if (StartChar[0] == ':')
			StartChar++;
		strncpy(ncfg->GamePath, StartChar, sizeof(ncfg->GamePath));
		ncfg->GamePath[sizeof(ncfg->GamePath)-1] = 0;
		memcpy(&(ncfg->GameID), gi[SelectedGame].ID, 4);
		DCFlushRange((void*)ncfg, sizeof(NIN_CFG));
	}

	// Free allocated memory in the game list
	for (i = 0; i < gamecount; ++i)
	{
		if (gi[i].Flags & GIFLAG_NAME_ALLOC)
			free(gi[i].Name);
		free(gi[i].Path);
	}

	// Send user back to the device selection menu
	if (ctx.selected == false) return 0;

	// Game is selected.
	return (ctx.saveSettings ? 3 : 1);
}

/**
 * Select the source device and game.
 * @return TRUE to save settings; FALSE if no settings have been changed.
 */
bool Menu_DeviceSelection(void)
{
	// Need to draw the menu the first time.
	bool res = false;
	bool redraw = true;
	while (1)
	{
		// Align to a frame, update controllers
		VIDEO_WaitVSync();
		FPAD_Update();
		if(Shutdown) LoaderShutdown();

		// Draw the device selection (SD/USB) menu
		if (redraw)
		{
			UseSD = (ncfg->UseUSB) == 0;
			PrintBuildInfo();
			PrintButtonActions("Exit", "Select", NULL, NULL);
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 6, UseSD ? ARROW_LEFT : "");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 7, UseSD ? "" : ARROW_LEFT);
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 6, " SD  ");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 7, "USB  ");

			redraw = false;

			// Render the screen here to prevent a blank frame
			// when returning from Menu_GameSelection().
			GRRLIB_Render();
			ClearScreen();
		}

		// Select a device and move into game selection menu
		if (FPAD_OK(0))
		{
			int ret = Menu_GameSelection();
			if (ret & 2) res = true;
			if (ret & 1) break;
			redraw = true;
		}
		// Return to the loader
		else if (FPAD_Start(0))
		{
			ShowMessageScreenAndExit("Returning to loader...", 0);
		}
		// Move cursor down (don't wraparound)
		else if (FPAD_Down(0))
		{
			ncfg->UseUSB = 1;
			redraw = true;
		}
		// Move cursor up (don't wraparound)
		else if (FPAD_Up(0))
		{
			ncfg->UseUSB = 0;
			redraw = true;
		}
	}
	return res;
}


/**
 * Show a single message screen.
 * @param msg Message.
 */
void ShowMessageScreen(const char *msg)
{
	const int len = strlen(msg);
	const int x = (640 - (len*10)) / 2;

	ClearScreen();
	PrintBuildInfo();
	PrintFormat(DEFAULT_SIZE, BLACK, x, 232, "%s", msg);
	GRRLIB_Render();
	ClearScreen();
}

/**
 * Show a single message screen and then exit to loader..
 * @param msg Message.
 * @param ret Return value. If non-zero, text will be printed in red.
 */
void ShowMessageScreenAndExit(const char *msg, int ret)
{
	const int len = strlen(msg);
	const int x = (640 - (len*10)) / 2;
	const u32 color = (ret == 0 ? BLACK : MAROON);

	ClearScreen();
	PrintBuildInfo();
	PrintFormat(DEFAULT_SIZE, color, x, 232, "%s", msg);
	ExitToLoader(ret);
}

/**
 * Print Nintendont version and system hardware information.
 */
void PrintBuildInfo(void)
{
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*0,
		"[%s][IOS%u v%u]", 
		NIN_GIT_VERSION, *(vu16*)0x80003140, *(vu16*)0x80003142);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*1,
		"Built   : " __DATE__ " " __TIME__);
}

/**
 * Print button actions.
 * Call this function after PrintBuildInfo().
 *
 * If any button action is NULL, that button won't be displayed.
 *
 * @param btn_home	[in,opt] Home button action.
 * @param btn_a		[in,opt] A button action.
 * @param btn_b		[in,opt] B button action.
 * @param btn_x1	[in,opt] X button action.
 */
void PrintButtonActions(const char *btn_home, const char *btn_a, const char *btn_b, const char *btn_x1)
{
	if (btn_home)
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 410, MENU_POS_Y + 20*0, "Home: %s", btn_home);
	if (btn_a)
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 410, MENU_POS_Y + 20*1, "A   : %s", btn_a);
	if (btn_b)
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 410, MENU_POS_Y + 20*2, "B   : %s", btn_b);
	if (btn_x1)
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 410, MENU_POS_Y + 20*3, "X : %s", btn_x1);
}


void ReconfigVideo(GXRModeObj *vidmode)
{
	if(ncfg->VideoScale >= 40 && ncfg->VideoScale <= 120)
		vidmode->viWidth = ncfg->VideoScale + 600;
	else
		vidmode->viWidth = 640;
	vidmode->viXOrigin = (720 - vidmode->viWidth) / 2;

	if(ncfg->VideoOffset >= -20 && ncfg->VideoOffset <= 20)
	{
		if((vidmode->viXOrigin + ncfg->VideoOffset) < 0)
			vidmode->viXOrigin = 0;
		else if((vidmode->viXOrigin + ncfg->VideoOffset) > 80)
			vidmode->viXOrigin = 80;
		else
			vidmode->viXOrigin += ncfg->VideoOffset;
	}
	VIDEO_Configure(vidmode);
}

/**
 * Print a LoadKernel() error message.
 *
 * This function does NOT force a return to loader;
 * that must be handled by the caller.
 * Caller must also call UpdateScreen().
 *
 * @param iosErr IOS loading error ID.
 * @param err Return value from the IOS function.
 */
void PrintLoadKernelError(LoadKernelError_t iosErr, int err)
{
	ClearScreen();
	PrintBuildInfo();
	PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4, "Failed to load IOS58 from NAND:");

	switch (iosErr) {
	case LKERR_UNKNOWN:
	default:
		// TODO: Add descriptions of more LoadKernel() errors.
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "LoadKernel() error %d occurred, returning %d.", iosErr, err);
		break;

	case LKERR_ES_GetStoredTMDSize:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "ES_GetStoredTMDSize() returned %d.", err);
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "This usually means IOS58 is not installed.");
		// TODO: Check if we're using System 4.3.
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*9, "Please update to Wii System 4.3 and try running");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "Nintendont again.");
		break;

	case LKERR_TMD_malloc:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to allocate memory for the IOS58 TMD.");
		break;

	case LKERR_ES_GetStoredTMD:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "ES_GetStoredTMD() returned %d.", err);
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
		break;

	case LKERR_IOS_Open_shared1_content_map:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Open(\"/shared1/content.map\") returned %d.", err);
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "This usually means Nintendont was not started with");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*8, "AHB access permissions.");
		// FIXME: Create meta.xml if it isn't there?
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "Please ensure that meta.xml is present in your Nintendont");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*11, "application directory and that it contains a line");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*12, "with the tag <ahb_access/> .");
		break;

	case LKERR_IOS_Open_IOS58_kernel:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Open(IOS58 kernel) returned %d.", err);
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
		break;

	case LKERR_IOS_Read_IOS58_kernel:
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Read(IOS58 kernel) returned %d.", err);
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
		break;
	}
}
