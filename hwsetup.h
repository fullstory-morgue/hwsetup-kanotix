#ifndef HAVE_HWSETUP_H
#define HAVE_HWSETUP_H

#define NULL_DEV	"/dev/null"
#define DEV_DIR		"/dev/"
#define MODPROBE	"/sbin/modprobe"

#define CARDSDB		"/usr/share/hwdata/Cards"
#define MAIN_CNF	"/etc/sysconfig/knoppix"
#define MOUSE_CNF	"/etc/sysconfig/mouse"
#define X_CNF		"/etc/sysconfig/xserver"
#define SOUND_CNF	"/etc/sysconfig/sound"
#define NET_CNF		"/etc/sysconfig/netcard"
#define FLOPPY_CNF	"/etc/sysconfig/floppy"

#define VERBOSE_PRINT	1
#define VERBOSE_PROMPT	2

#define SKIP_AUDIO	1
#define SKIP_SCSI	2

#define MAX_TIME	120		/* Maximum of seconds to run, total */
#define MAX_TIME_MODULE	4		/* Maximum time in seconds to wait until a module */
					/* is successfully loaded before continuing       */

#define MAX_DMA 	8
#define MAX_IO  	64
#define MAX_IRQ 	16

#endif  /* HAVE_HWSETUP_H */

