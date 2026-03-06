/*
SI.c for Nintendont (Kernel)

Copyright (C) 2014 FIX94

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
#include "SI.h"
#include "Config.h"
#include "debug.h"
#include "string.h"

#define SI_GC_CONTROLLER 0x09000000
#define SI_ERROR_NO_RESPONSE 0x08
#define SI_EXTRA_REQUEST  0x13003500  // ARM-side mirror of 0x93003500

#define SI_HW_BASE       0x0D006400
#define SI_HW_CONTROL    (SI_HW_BASE + 0x34)
#define SI_HW_STATUS     (SI_HW_BASE + 0x38)
#define SI_HW_IO_BUF    (SI_HW_BASE + 0x80)

#define EXTRA_DATA_ADDR  0x13003520
#define EXTRA_DATA_MAX   1024

#define SI_CMD_EXTRA_DATA   0xF0
#define SI_CMD_EXTRA_CHUNK  0xF1

u32 SI_IRQ = 0;
static bool complete = true;
static u32 cur_control = 0;
void SIInit()
{
	memset((void*)SI_BASE, 0, 0x120);
	sync_after_write((void*)SI_BASE, 0x120);

	sync_before_read((void*)PAD_BUFF, 0x40);
	memset((void*)PAD_BUFF, 0, 0x30); //For Triforce to not instantly reset
	sync_after_write((void*)PAD_BUFF, 0x40);

	SI_IRQ = 0;
	complete = true;
	cur_control = 0;
}

void SIInterrupt()
{
	//sync_before_read((void*)0x14, 0x4);
	//if (read32(0x14) != 0)
	//	return;
	if (SI_IRQ & 0x1)
	{
		if (complete)
		{
			cur_control &= ~(1<<28); //no read interrupt requested anymore, completed transfer
		}
		else
		{
			cur_control |= (1<<28); //read status interupt request, might need more if we wanna use that
		}
		//dbgprintf("Read SI Control: 0x%08X 0x%08X\r\n", cur_control);
		write32(SI_CONTROL, cur_control);
		sync_after_write((void*)SI_CONTROL, 4);
	}
	if (SI_IRQ & 0x2)
	{
		sync_before_read((void*)SI_CONTROL, 0x4);
		cur_control = read32(SI_CONTROL);
		if ((cur_control & (1 << 31)) == 0)
			SI_IRQ &= ~0x2; //one-shot interrupt skipped
		if ((cur_control & (1 << 30)) == 0)
			return;  // interrupts not enabled
		SI_IRQ &= ~0x2; //one-shot interrupt complete
		//dbgprintf("SI Done Transfer %d, 0x%08X\r\n", SI_IRQ, cur_control);
	}

	write32( SI_INT, 0x8 );		// SI IRQ
	sync_after_write( (void*)SI_INT, 0x20 );
	write32( HW_IPC_ARMCTRL, 8 ); //throw irq

	complete ^= 1;
}

// One-shot SI transfer using the IO buffer (doesn't touch channel regs)
static s32 SIRawTransfer(u32 chan, u32 cmdWord, u32 cmdLen, u32 respLen)
{
    // Write command into IO buffer
    write32(SI_HW_IO_BUF, cmdWord);

    // Build control word:
    // bit 0: start transfer
    // bits 1-2: channel
    // bits 8-14: output length (bytes to send)
    // bits 16-22: input length (bytes to receive)
    u32 control = (1 << 0)
                | ((chan & 3) << 1)
                | ((cmdLen & 0x7F) << 8)
                | ((respLen & 0x7F) << 16);

    write32(SI_HW_CONTROL, control);

    // Wait for transfer complete
    u32 timeout = 50000;
    while (timeout--)
    {
        u32 ctrl = read32(SI_HW_CONTROL);
        if (ctrl & (1 << 31))  // TC bit
        {
            // Clear TC
            write32(SI_HW_CONTROL, ctrl & ~1);
            if (ctrl & (1 << 29))  // NOREP — no response
                return -1;
            return 0;
        }
        udelay(1);
    }
    return -2;
}

static void SIReadExtraData(u32 chan)
{
    u8 *dest = (u8*)EXTRA_DATA_ADDR + (chan * EXTRA_DATA_MAX);

    // Step 1: Ask RP2040 how much data it has
    s32 ret = SIRawTransfer(chan, (SI_CMD_EXTRA_DATA << 24), 1, 4);
    if (ret < 0) return;

    u32 response = read32(SI_HW_IO_BUF);
    u32 totalSize = response & 0xFFFF;
    if (totalSize == 0 || totalSize > EXTRA_DATA_MAX) return;

    dbgprintf("SI:Chan%u: Reading %u bytes extra data\r\n", chan, totalSize);

    // Step 2: Read in 60-byte chunks (leave room for cmd overhead)
    u32 totalRead = 0;
    u32 chunkIdx = 0;
    while (totalRead < totalSize)
    {
        u32 thisChunk = totalSize - totalRead;
        if (thisChunk > 60) thisChunk = 60;

        u32 cmd = (SI_CMD_EXTRA_CHUNK << 24) | (chunkIdx << 16);
        ret = SIRawTransfer(chan, cmd, 3, thisChunk);
        if (ret < 0)
        {
            dbgprintf("SI:Chan%u: Chunk %u failed\r\n", chan, chunkIdx);
            break;
        }

        // Copy from IO buffer
        u32 i;
        for (i = 0; i < thisChunk; i += 4)
        {
            u32 word = read32(SI_HW_IO_BUF + i);
            dest[totalRead + i + 0] = (word >> 24) & 0xFF;
            dest[totalRead + i + 1] = (word >> 16) & 0xFF;
            dest[totalRead + i + 2] = (word >>  8) & 0xFF;
            dest[totalRead + i + 3] = (word >>  0) & 0xFF;
        }

        totalRead += thisChunk;
        chunkIdx++;
    }

    dest[totalRead] = 0;
    sync_after_write(dest, (EXTRA_DATA_MAX + 31) & ~31);
    dbgprintf("SI:Chan%u: Extra data: \"%s\"\r\n", chan, dest);
}

void SIUpdateRegisters()
{
    // === Check if PPC signaled a new controller ===
    sync_before_read((void*)SI_EXTRA_REQUEST, 0x20);
    u32 chan;
    for (chan = 0; chan < 4; chan++)
    {
        u32 request = read32(SI_EXTRA_REQUEST + (chan * 4));
        if (request == 1)
        {
            // PPC detected a new controller — read extra data now
            SIReadExtraData(chan);

            // Signal back to PPC that we're done
            write32(SI_EXTRA_REQUEST + (chan * 4), 2);
            sync_after_write((void*)SI_EXTRA_REQUEST, 0x20);
        }
    }

	sync_before_read((void*)SI_BASE, 0x100);
	cur_control = read32(SI_CONTROL);
	u32 cur_status = read32(SI_STATUS);
	//u32 cur_poll = read32(SI_POLL);
	//dbgprintf("Read SI Control: 0x%08X\r\n", cur_control);
	if (cur_control & (1 << 0)) // transfer
	{
		//dbgprintf("SI Buffer Cmd: %08x\r\n", read32(SI_IO_BUF));
		cur_control |= (1<<29); //we normally always have some communication error?
		//cur_control &= ~(1 << 29); //we normally always have some communication error?
		u32 chan = (cur_control >> 1) & 0x3;
		u32 ChanBuff = PAD_BUFF + (chan * 0xC);
		bool PadGood = !!(read32(0x13003024) & (1<<chan));
		switch ((read32(SI_IO_BUF) >> 24) & 0xFF)
		{
			case 0x00: // Get Type
				{
					//dbgprintf("SI GetType\r\n");
					write32(SI_IO_BUF, PadGood ? SI_GC_CONTROLLER : SI_ERROR_NO_RESPONSE);
					sync_after_write((void*)SI_IO_BUF, 4);
				} break;
			case 0x40: // Direct Cmd
				{
					//dbgprintf("SI Direct\r\n");
					//Might need to fix this (multiple modes)
					write32(SI_IO_BUF + 0, read32(ChanBuff + 0));
					write32(SI_IO_BUF + 4, read32(ChanBuff + 4));
					sync_after_write((void*)SI_IO_BUF, 8);
				} break;
			case 0x41: // Origin
			case 0x42: // Calibrate
				{
					//dbgprintf("SI Origin/Cal\r\n");
					//Might need to fix this (multiple modes)
					write32(SI_IO_BUF + 0, 0x41008080);
					write32(SI_IO_BUF + 4, 0x80801F1F);
					write32(SI_IO_BUF + 8, 0x00000000);
					sync_after_write((void*)SI_IO_BUF, 12);
				} break;
		}
		if (!PadGood)
		{
			//cur_control |= (1 << 29); //we normally always have some communication error?
			//cur_status |= (0x08000000 >> (0x8 * chan));
			//write32(SI_STATUS, cur_status);
			//sync_after_write((void*)SI_STATUS, 4);
		}
		cur_control &= ~1;
		//cur_control |= (1 << 28); //read status interupt request, might need more if we wanna use that
		cur_control |= (1 << 31); //tc status interupt request, might need more if we wanna use that
		/* set controller responses, not needed with patched PADRead
		if((cur_control & 7) == 3) //chan 1
			cur_status |= (1<<19);
		else if((cur_control & 7) == 5) //chan 2
			cur_status |= (1<<11);
		else if((cur_control & 7) == 7) //chan 3
			cur_status |= (1<<3);
		//dbgprintf("New SI Control: %08x\r\n", cur_control);
		write32(SI_STATUS, cur_status);
		sync_after_write((void*)SI_STATUS, 4);*/
		write32(SI_CONTROL, cur_control);
		sync_after_write((void*)SI_CONTROL, 4);
		SI_IRQ |= 0x2; //we will give the game one interrupt
		//dbgprintf("Read SI mod Control: 0x%08X\r\n", cur_control);
		complete = false;
		//if((cur_control & 0x7F00) != 0)
		//	complete = false; //requested some bytes
		//u8 cur_chan = 0, prev_chan = 0;
		//cur_chan = (cur_control & 6) >> 1;
		//if(cur_chan != prev_chan) //force repeat
		//{
		//	complete = false;
		//	prev_chan = cur_chan;
		//}
	}
	if (cur_status & 0x80000000)
	{
		//dbgprintf("Read SI Status: %08x\r\n", cur_status);
		u32 ChanIdx;
		for (ChanIdx = 0; ChanIdx < 4; ChanIdx++)
		{
			bool PadGood = true;
			//bool PadGood = read32(PAD_BUFF + 8 + (ChanIdx * 0xC)) == 0;
			//if (ChanIdx >= ConfigGetMaxPads())
			//	PadGood = false;
			//cur_status = (cur_status << 0x8) | (PadGood ? 0x0 : 0x20);
			u32 ChanAdd = SI_CHAN_0 + (0xC * ChanIdx);
			write32(ChanAdd + 4, PadGood ? 0x00808080 : 0);//read32(0x1300280C+0));
			write32(ChanAdd + 8, PadGood ? 0x80800000 : 0);//read32(0x1300280C+4));
		}
		cur_status = 0x20202020;
		//cur_status = 0x0;
		sync_after_write((void*)SI_CHAN_0, 0x30);
		//dbgprintf("Read SI Cmd 0-3: 0x%08X, 0x%08X, 0x%08X, 0x%08X,\r\n", read32(SI_CHAN_0), read32(SI_CHAN_1), read32(SI_CHAN_2), read32(SI_CHAN_3));
		write32(SI_STATUS, cur_status);
		sync_after_write((void*)SI_STATUS, 4);
		//dbgprintf("Wrote SI Status: %08x\r\n", cur_status);
	}

	// This should use cur_poll.  Something's still not right.
	//if (((cur_poll & 0xF0) != 0) && (cur_control & (1 << 27)) != 0)
	if ((cur_control & (1 << 27)) != 0)
	{
		cur_control |= (1<<29); //we normally always have some communication error?
		//if ((SI_IRQ & 0x1) == 0x0)
		//	dbgprintf("Enabled polling SI IRQ %d\r\n", SI_IRQ);
		SI_IRQ |= 0x1; //give the game regular updates
	}
	else
	{
		//if ((SI_IRQ & 0x1) == 0x1)
		//	dbgprintf("Disabled polling SI IRQ %d\r\n", SI_IRQ);
		SI_IRQ &= ~(0x1); //give the game regular updates
	}
}
