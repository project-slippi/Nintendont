#ifndef __KERNEL_BOOT_H
#define __KERNEL_BOOT_H

/* Constants for kernel boot stages. These are shared across kernel/main.c and 
 * loader/source/main.c - used to co-ordinate kernel boot between ARM and PPC.
 */

#define	ES_INIT		0
#define	LOADER_SIGNAL	1
#define STORAGE_INIT	2
#define STORAGE_MOUNT	3
#define BOOT_STATUS_4	4
#define STORAGE_CHECK	5
#define NETWORK_INIT	6
#define CONFIG_INIT	7
#define BOOT_STATUS_8	8
#define DI_INIT		9
#define CARD_INIT	10
#define BOOT_STATUS_11	11
#define KERNEL_RUNNING	0xdeadbeef

#endif
