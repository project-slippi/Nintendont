/* SlippiDebug.c
 * Miscellaneous functions to help debug things in PPC-world.
 */

#include "SlippiDebug.h"
#include "global.h"
#include "syscalls.h"
#include "net.h"

#define THREAD_CYCLE_TIME_MS	5000

// Thread variables
static u32 SlippiDebug_Thread;
extern char __slippi_debug_stack_addr;
extern char __slippi_debug_stack_size;
static u32 SlippiDebugThread(void *arg);

// From kernel/net.c
extern s32 top_fd;

// Socket structures for this thread
s32 debug_sock ALIGNED(32);
static struct sockaddr_in debug_sockaddr ALIGNED(32) = {
	.sin_family	= AF_INET,
	.sin_port	= 20000,
	{
		.s_addr	= 0xffffffff,
	},
};

void SlippiDebugShutdown() { thread_cancel(SlippiDebug_Thread, 0); }
s32 SlippiDebugInit()
{
	SlippiDebug_Thread = do_thread_create(
		SlippiDebugThread,
		((u32 *)&__slippi_debug_stack_addr),
		((u32)(&__slippi_debug_stack_size)),
		0x78);
	thread_continue(SlippiDebug_Thread);
	return 0;
}

/* checkCrash()
 * Reach out into (PPC-addressible) MEM1 and check Melee's frame counter. If
 * the frame counter hasn't been incremented after a certain period of time,
 * this gives some good indication that Melee has crashed.
 */
#define MELEE_CRASH_TIMEOUT	15
static u32 crash_ts = 0;
static u32 last_frame = 0;
static u8 crashmsg[] = "CRASHED\x00";

int checkCrash(int socket)
{
	if (TimerDiffSeconds(crash_ts) < MELEE_CRASH_TIMEOUT)
		return 0;

	u32 current_frame = read32(MELEE_FRAME_CTR);
	if (current_frame != last_frame)
	{
		last_frame = current_frame;
		crash_ts = read32(HW_TIMER);
		return 0;
	}

	// When we detect a crash, send a packet to the connected client
	sendto(top_fd, debug_sock, crashmsg, sizeof(crashmsg), 0);
	return 1;
}


static u32 SlippiDebugThread(void *arg)
{
	s32 res;

	// Initialize socket on the broadcast address
	debug_sock = socket(top_fd, AF_INET, SOCK_DGRAM, IPPROTO_IP);
	res = connect(top_fd, debug_sock, (struct sockaddr *)&debug_sockaddr);

	// Main loop
	while (1)
	{
		mdelay(THREAD_CYCLE_TIME_MS);
	}
	return 0;
}
