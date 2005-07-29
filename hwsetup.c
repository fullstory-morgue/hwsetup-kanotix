/****************************************************************************\
* HWSETUP - non-interactive hardware detection and configuration             *
* loads modules, generates /dev links, no isapnp autoconfiguration (yet)     *
* needs kudzu-kanotix-dev (ver. 1.1.90 and up)                               *
* Author: Klaus Knopper <knopper@knopper.net>                                *
\****************************************************************************/

/* Changes by: 2004, 2005: Stefan Lippers-Hollmann <s.l-h@gmx.de>
 * 2004 derive changes by Alexander de Landgraaf <alex@delandgraaf.com>
 * 2005 import changes by Klaus Knopper for hwsetup 1.1.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "hwsetup.h"

#undef _i_wanna_build_this_crap_
/* #define _i_wanna_build_this_crap_ 1 *//* Necessary for ISAPNP */
#include "kudzu.h"
#if defined(_i_wanna_build_this_crap_)
#include "isapnp.h"
#endif /* _i_wanna_build_this_crap_ */

#define VERSION "HWSETUP 1.1, an automatic hardware configuration tool\n" \
                "(C) 2002 Klaus Knopper <knoppix@knopper.net>\n" \
		"derived for Kanotix (http://www.kanotix.com).\n" \
		"The original author should not be bothered with problems of this version.\n\n"

#ifdef BLACKLIST
/* Do not, under any circumstances, load these modules automatically, */
/* even if in pcitable. (libkudzu may ignore this, and the KNOPPIX    */
/* autoconfig scripts may probe them, too)  */
const char *blacklist[] =
{
	"apm", "agpgart", "yenta_socket", "i82092", "i82365", "tcic",
	"pcmcia_core", "ds", "ohci1394"
	/* Winmodems, unusable, can block sound slot */
	"snd-atiixp-modem", "snd-intel8x0m","snd-via82xx-modem"
};
#endif	/* BLACKLIST */

#ifdef CHECK_CONFLICT
/* If a conflicting module in a row is already loaded, the new module will not be probed anymore */
#define CONFLICT_SET 2
struct conflict { char *name; int loaded; } conflicts [][CONFLICT_SET] =
{
	{{ "ad1848", 0},		{"snd-nm256", 0}},
	{{ "ali5455", 0},		{"intel8x0", 0}},
	{{ "cmpci", 0},			{"snd-cmpci", 0}},
	{{ "cs46xx", 0},		{"snd-cs46xx", 0}},
	{{ "cs4281", 0},		{"snd-cs4281", 0}},
	{{ "emu10k1", 0},		{"snd-emu10k1", 0}},
	{{ "es1370", 0},		{"snd-ens1370", 0}},
	{{ "es1371", 0},		{"snd-ens1371", 0}},
	{{ "esssolo1", 0},		{"snd-es1938", 0}},
	{{ "forte", 0},			{"snd-fm801", 0}},
	{{ "i810_audio", 0},		{"snd-intel8x0", 0}},
	{{ "maestro", 0},		{"snd-es1960", 0}},
	{{ "maestro3", 0},		{"snd-maestro3", 0}},
	{{ "nm256_audio", 0},		{"snd-nm256", 0}},
	{{ "rme96xx", 0},		{"snd-rme9652", 0}},
	{{ "sonicvibes", 0},		{"snd-sonicvibes", 0}},
	{{ "trident", 0},		{"snd-trident", 0}},
	{{ "via82cxxx_audio", 0},	{"snd-via82xx", 0}},
	{{ "ymfpci", 0},		{"snd-ymfpci", 0}}
};

int check_conflict(char *name)
{
	int i;
	
	if(!name)
		return 0;

	for(i=0; i<(sizeof(conflicts)/sizeof(struct conflict)/CONFLICT_SET); i++)
	{
		int	j,
			found_name=0,
			found_loaded=0;
		
		for(j=0; j<CONFLICT_SET; j++)
		{
			if(!strcmp(name, conflicts[i][j].name))
				found_name=1;
			else if(conflicts[i][j].loaded)
				found_loaded=1;
		}

		if(found_name && found_loaded)
			return 1; /* found a conflict */
	}
	
	return 0;
}

void set_conflict(char *name)
{
	int i;

	if(!name)
		return;
	
	for(i=0; i<(sizeof(conflicts)/sizeof(struct conflict)/CONFLICT_SET); i++)
	{
		int j;

		for(j=0; j<CONFLICT_SET; j++)
		{
			if(!strcmp(name, conflicts[i][j].name))
				conflicts[i][j].loaded=1;
		}
	}
}
#endif /* ifdef CHECK_CONFLICT */						

/* These need to be global, so we can kill them in case of problems */
pid_t wpid = 0;		/* fixme */


/*
 * int syntax(char *option)
 *
 * print out syntax info.
 */
int syntax(char *option)
{
	printf(VERSION);
	
	if(option)
		fprintf(stderr, "hwsetup: Unknown option '%s'\n\n", option);
		
	printf("Usage: hwsetup\n"
		"\t-v\tbe verbose\n"
		"\t-p\tprint rotating prompt\n"
		"\t-a\tignore audio devices\n"
		"\t-s\tignore scsi controllers\n"
		"\t-n\tprobe only, don't configure anything.\n");

	return option ? 1 : 0;
}

/*
 * pid_t startwheel(void)
 * 
 * Feedback while detecting hardware
 */
pid_t startwheel(void)
{
	char		v[] = "Autoconfiguring devices... ",
			r[] = "/-\\|",
			*b = "\b";
	
	pid_t pid;
	
	if((pid = fork()) > 0)
	{
		return pid;						/* return Child PID to Master process */
	}
	else
	{
		if(pid == 0)
		{
			int	i,
				j;
			
			/* Allow killing of process */
			signal(SIGHUP, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			write(2, v, sizeof(v) - 1);
			for(i = j = 0;; i++, j++)
			{
				if(j % 8 == 7)
				write(2, "\033[42;32m \033[0m", 13);	/* a green space */
				write(2, &r[i % 4], 1);
				write(2, b, 1);
				usleep((unsigned long)40000);
			}
			
			exit(0);					/* Exit child process */
		}
	}
	
	return 0;
}

/*
 * int exists(char *filename)
 *
 * checks if *filename ist existent.
 */
int exists(char *filename)
{
	struct stat s;
	
	return !stat(filename, &s);
}

/*
 * struct xinfo
 *
 * structure for information concerning X configuration
 */
struct xinfo
{
	char xserver[16];
	char xmodule[16];
	char xdesc[128];
	char xopts[128];
};


/*
 * struct xinfo *getxinfo(struct device *d)
 *
 * probe *d and get configuration options
 */
struct xinfo *getxinfo(struct device *d)
{
	int 		i,
			rescanned = 0;
			
	const char	*xfree4 = "XFree86",
			*xvesa4 = "vesa";
	
	static struct xinfo xi;
	
	memset(&xi, 0, sizeof(struct xinfo));
	
	if(d->desc)
		strncpy(xi.xdesc, d->desc, sizeof(xi.xdesc));
		
	if(d->driver)
	{
		const char *driver[] = { "3DLabs", "Mach64", "Mach32", "Mach8", "AGX",
					"P9000", "S3 ViRGE", "S3V", "S3", "W32",
					"8514", "I128", "SVGA", xfree4, NULL };
	
		const char *server[] = { driver[0], driver[1], driver[2], driver[3], driver[4],
					driver[5], "S3V", driver[7], driver[8], driver[9],
					driver[10], driver[11], driver[12], xfree4, NULL };
		
		if(!strncasecmp(d->driver, "Card:", 5))
		{
			/* RedHat Cards-DB */
			/* Kudzu "Cards" format */
			
			FILE *cardsdb;
			char xfree3server[128];
		
			memset(xfree3server, 0, sizeof(xfree3server));
		
			if((cardsdb = fopen(CARDSDB, "r")) != NULL)
			{
				/* Try to find Server and Module in /usr/share/kudzu/Cards */
				char buffer[1024];
				char searchfor[128];
				int found = 0;
			
				memset(searchfor, 0, sizeof(searchfor));
				sscanf(&d->driver[5], "%127[^\r\n]", searchfor);
			
				while(!found && !feof(cardsdb) && fgets(buffer, 1024, cardsdb))
				{
					char sfound[128];
			
					memset(sfound, 0, sizeof(sfound));
					if(strncasecmp(buffer, "NAME ", 5) ||
					(sscanf(&buffer[5], "%127[^\r\n]", sfound) != 1) ||
					strcasecmp(sfound, searchfor))
					continue;
					while(!feof(cardsdb) && fgets(buffer, 1024, cardsdb))
					{
							if(buffer[0] < 32)
								break;	/* End-of-line */
						
						if(!strncasecmp(buffer, "SERVER ", 7))
						{
							char x[20] = "";
				
							sscanf(&buffer[7], "%19s", x);
						}
						else if(!strncasecmp(buffer, "DRIVER ", 7))
						{
							char xmodule[32];
				
							sscanf(&buffer[7], "%31s", xmodule);
							strncpy(xi.xmodule, xmodule, sizeof(xi.xmodule));
						}
						else if(!strncasecmp(buffer, "SEE ", 4) && rescanned < 10)
						{
							/* rescan Cards-DB for other server */
							fseek(cardsdb, 0L, SEEK_SET);
							++rescanned;
							memset(searchfor, 0, sizeof(searchfor));
							sscanf(&buffer[4], "%127[^\r\n]", searchfor);
							break;	/* Continue with outer while() */
						}
					}
				}
		
				fclose(cardsdb);
			}
	
			if(*xi.xmodule || *xi.xserver || *xfree3server)	/* (Partial) Success */
			{
				if(!*xi.xserver)
				{
					if(*xfree3server && !*xi.xmodule)
						strncpy(xi.xserver, xfree3server, sizeof(xi.xserver));
					else
						strncpy(xi.xserver, xfree4, sizeof(xi.xserver));
				}
				
				if(!*xi.xmodule)
					strcpy(xi.xmodule, xvesa4);
				
				return &xi;
			}
		}
		
		/* Card not found in Cards database -> Try to guess from description */
		for(i = 0; driver[i] != NULL; i++)
		{
			if(strstr(d->driver, driver[i]))
			{
				char *xpos;
			
				if((xpos = strstr(d->driver, xfree4)) != NULL)	/* Check for XFree 4 */
				{
					char xm[32] = "";
			
					strcpy(xi.xserver, xfree4);
					
					if(sscanf(xpos, "XFree86(%30[^)])", xm) == 1)
						strcpy(xi.xmodule, xm);
					else
						strcpy(xi.xmodule, xvesa4);
				}
				else
				{
					char		xserver[32],
							*xf[2] = { "", "XF86_" };
							
					int j;
			
					for(j = 0; j < 2; j++)
					{
						sprintf(xserver, "%s%.24s", xf[j], server[i]);
						strncpy(xi.xserver, xserver, sizeof(xi.xserver));
						
						break;	/* for */
					}
				}
			}
		}
	}
	
	/* Special options required? */
	if(d->desc)
	{
		strncpy(xi.xdesc, d->desc, sizeof(xi.xdesc) - 1);
		
		/* Handle special cards that require special options */
		if(strstr(d->desc, "Trident") || strstr(d->desc, "TGUI") || strstr(d->desc, "Cirrus") || strstr(d->desc, "clgd"))
		{
			if(!strcmp(xi.xserver, xfree4))
				strncpy(xi.xopts, "-depth 16", sizeof(xi.xopts) - 1);
			else
				strncpy(xi.xopts, "-bpp 16", sizeof(xi.xopts) - 1);
		}
		else if(strstr(d->desc, "Savage 4"))	/* S3 Inc.|Savage 4 */
		{
			if(!strcmp(xi.xserver, xfree4))
				strncpy(xi.xopts, "-depth 32", sizeof(xi.xopts) - 1);
			else
				strncpy(xi.xopts, "-bpp 32", sizeof(xi.xopts) - 1);
		}
	}
	
	/* Fallback values */
	if(!*xi.xserver)
		strcpy(xi.xserver, xfree4);
		
	if(!*xi.xmodule)
		strcpy(xi.xmodule, xvesa4);
	
	return &xi;
}

/*
 * void hw_info(struct device *d)
 *
 * get device class for *d.
 */
void hw_info(struct device *d)
{
	enum deviceClass class = d->type;
	enum deviceBus bus = d->bus;
	
	char *unknown = "UNKNOWN";
	
	/* These used to be much easier when they were still arrays... */
	char *classname =
		class == CLASS_UNSPEC ? "UNSPEC" :
		class == CLASS_OTHER ? "OTHER" : 
		class == CLASS_NETWORK ? "NETWORK" : 
		class == CLASS_SCSI ? "SCSI" :
		class == CLASS_VIDEO ? "VIDEO" :
		class == CLASS_AUDIO ? "AUDIO" :
		class == CLASS_MOUSE ? "MOUSE" :
		class == CLASS_MODEM ? "MODEM" :
		class == CLASS_CDROM ? "CDROM" :
		class == CLASS_TAPE ? "TAPE" :
		class == CLASS_FLOPPY ? "FLOPPY" :
		class == CLASS_SCANNER ? "SCANNER" :
		class == CLASS_HD ? "HD" :
		class == CLASS_RAID ? "RAID" :
		class == CLASS_PRINTER ? "PRINTER" :
		class == CLASS_CAPTURE ? "CAPTURE" :
		class == CLASS_USB ? "USB" :
		class == CLASS_MONITOR ? "MONITOR" :
		class == CLASS_KEYBOARD ? "KEYBOARD" :
		unknown;
	
	char *busname =
		bus == BUS_OTHER ? "OTHER" :
		bus == BUS_PCI ? "PCI" :
		bus == BUS_SBUS ? "SBUS" :
		bus == BUS_PSAUX ? "PSAUX" :
		bus == BUS_SERIAL ? "SERIAL" :
		bus == BUS_PARALLEL ? "PARALLEL" :
		bus == BUS_SCSI ? "SCSI" :
		bus == BUS_IDE ? "IDE" :
		bus == BUS_DDC ? "DDC" :
		bus == BUS_USB ? "USB" :
		bus == BUS_KEYBOARD ? "KEYBOARD" :
#if defined(_i_wanna_build_this_crap_)
		bus == BUS_ISAPNP ? "ISAPNP" :
#endif
		unknown;
	
	printf("---\n"
		"class:  %s\n"
		"bus:    %s\n"
		"device: %s\n"
		"driver: %s\n"
		"desc:   %s\n", classname, busname, d->device ? d->device : "(null)",
		d->driver, d->desc ? d->desc : "(empty)");
}

/*
 * int checkmoveup(char *filename, int oldnum)
 *
 * rename /dev/mouse  -> /dev/mouse1
 *        /dev/mouse1 -> /dev/mouse2
 * recursive
 */
int checkmoveup(char *filename, int oldnum)
{
	int		newnum = oldnum + 1;
	
	char		srcname[64],
			dstname[64];
	
	struct stat buf;
	
	sprintf(srcname, (oldnum > 0) ? "%.32s%d" : "%.32s", filename, oldnum);
	
	if(stat(srcname, &buf))
		return 0;			/* File does not exist, OK. */
	
	sprintf(dstname, "%.32s%d", filename, newnum);
	
	/* recursive if file exists, otherwise just rename it */
	return (!stat(dstname, &buf) && checkmoveup(filename, newnum)) ? errno :
	rename(srcname, dstname);
}

/*
 * int link_dev(struct device *d, char *target, int tnum, int verbose)
 *
 * create device links.
 */
int link_dev(struct device *d, char *target, int tnum, int verbose)
{
	const char devdir[] = DEV_DIR;
	
	if(d && d->device)
	{
		char devname[64], dstname[64];
		
		sprintf(devname, "%s%.32s", devdir, d->device);
		sprintf(dstname, "%s%.32s", devdir, target);
		
		if(checkmoveup(dstname, tnum))
			return -1;		/* Read-only FS?! */
			
		if(tnum > 0)
			sprintf(dstname, "%s%.32s%1d", devdir, target, tnum);
			
		if(verbose & VERBOSE_PRINT)
			printf("symlink(%.32s,%.32s)\n", devname, dstname);
			
		return symlink(devname, dstname);
	}
	return -1;
}

/*
 * void segfault_handler(int dummy)
 *
 * yeah, sh*t happens - so take care...
 */
void segfault_handler(int dummy)
{
	signal(SIGSEGV, SIG_IGN);
	fprintf(stderr, "\nWARNING: Caught signal SEGV while executing modprobe.\n");
	fflush(stderr);
}

/*
 * void alarm_handler(int dummy)
 *
 * check for stalled processes.
 */
void alarm_handler(int dummy)
{
	signal(SIGALRM, SIG_IGN);
	fprintf(stderr, "\nWARNING: Autodetection seems to hang,\nplease check your computers BIOS settings.\n");
	fflush(stderr);
	
	if(wpid)
	{
		kill(wpid, SIGTERM);
		usleep((unsigned long)2500000);
		kill(wpid, SIGKILL);
		wpid = 0;
	}
	
	exit(1);			/* exit program */
}

/*
 * int load_mod(char *m, int verbose)
 *
 * load module *m.
 */
int load_mod(char *m, int verbose)
{
	int	i,
		pstatus;
	time_t	now;
	pid_t	mpid;
	
	if((m == NULL) || (!strcmp("unknown", m)) || (!strcmp("ignore", m)))
		return 0;
		
#ifdef BLACKLIST
	for(i = 0; i < (sizeof(blacklist) / sizeof(char *)); i++)
	{
		if(!strcmp(blacklist[i], m))
		{
			if(verbose & VERBOSE_PRINT)
				printf("not loading module %.32s (is in blacklist)\n", m);
				
			return 0;
		}
	}
#endif /* BLACKLIST */

#ifdef CHECK_CONFLICT
	if(check_conflict(m))
	{
		if(verbose&VERBOSE_PRINT)
			printf("not loading module %.32s (conflicts with other module for same device)\n", m);

		return 0;
	}
#endif /* CHECK_CONFLICT */
	
	if((mpid = fork()) == 0)
	{				/* child process */
		if(verbose & VERBOSE_PRINT)
			printf("modprobe(%.32s)\n", m);
		signal(SIGSEGV, segfault_handler);
		
		/* Send modprobe errors to /dev/null */
		if(!(verbose & VERBOSE_PRINT))
			freopen(NULL_DEV, "w", stderr);
		
		execl(MODPROBE, "modprobe", m, NULL);
		
		exit(1);
	}
	
	now = time(0);
	
	do
	{
		usleep((unsigned long)125000);		/* Delay 1/8s */
		
		/* We SHOULD wait for modprobe to finish! */
		if(waitpid(mpid, &pstatus, WNOHANG))
			break;
	}
	while((time(0) - now) < MAX_TIME_MODULE);
#ifdef CHECK_CONFLICT
	set_conflict(m);
#endif /* CHECK_CONFLICT */
	return pstatus;
}

/*
 * int configure_isapnp(struct device *dev, int verbose)
 *
 * probing ISA device isn't easy, so try to avoid it ;)
 */
#if defined(_i_wanna_build_this_crap_)
int configure_isapnp(struct device *dev, int verbose)
{
	int	io[MAX_IO],
		io_max,
		irq[MAX_IRQ],
		irq_max,
		dma[MAX_DMA],
		dma_max;
		
	struct isapnpDevice *d = (struct isapnpDevice *)dev;
	
	if(d->io)
	{
		if(verbose & VERBOSE_PRINT)
			printf("io:     ");
		
		for(io_max = 0; io_max < MAX_IO && (io[io_max] = d->io[io_max]) != -1; io_max++)
		{
			if(verbose & VERBOSE_PRINT)
				printf("0x%x, ", (int)io[io_max]);
		}
				
		if(verbose & VERBOSE_PRINT)
			printf("\n");
	}
	
	if(d->irq)
	{
		if(verbose & VERBOSE_PRINT)
			printf("irq:    ");
		
		for(irq_max = 0; irq_max < MAX_IRQ && (irq[irq_max] = d->irq[irq_max]) != -1; irq_max++)
		{
			if(verbose & VERBOSE_PRINT)
				printf("0x%d, ", (int)irq[irq_max]);
		}
		
		if(verbose & VERBOSE_PRINT)
			printf("\n");
	}
	
	if(d->dma)
	{
		if(verbose & VERBOSE_PRINT)
			printf("dma:    ");
		
		for(dma_max = 0; dma_max < MAX_DMA && (dma[dma_max] = d->dma[dma_max]) != -1; dma_max++)
		{
			if(verbose & VERBOSE_PRINT)
				printf("%d, ", (int)dma[dma_max]);
		}
		
		if(verbose & VERBOSE_PRINT)
			printf("\n");
	}
	/* no configuration possible (yet) */
	
#if defined(_i_wanna_build_this_crap_)
	/* look for free interrupts/IOs/DMAs AFTER all other drivers are loaded,
	* write the /etc/isapnp.conf afterwards and load the appropriate drivers with
	* isapnp.
	*/
	return modprobe(d->driver, free_io, free_irq, free_dma, verbose);
#endif /* _i_wanna_build_this_crap_ */
	
	return (0);
}
#endif /* _i_wanna_build_this_crap_ */

/*
 * int writeconfig(char *name, struct device *d, int verbose)
 *
 * write config for *d to MAIN_CNF
 */
int writeconfig(char *name, struct device *d, int verbose)
{
	FILE		*f,
			*k;
			
	char		*t1,
			*t2;

	const char	*kconfig = MAIN_CNF,
			*xserver = "XSERVER=\"%s\"\n",
			*xmodule = "XMODULE=\"%s\"\n",
			*xopts = "XOPTIONS=\"%s\"\n",
			*xdesc = "XDESC=\"%s\"\n";

	struct xinfo	*xi = getxinfo(d);
	
	unlink(name);
	
	if((f = fopen(name, "w")) == NULL)
	{				/* Read-only filesystem on /etc ?! */
		fprintf(stderr, "Can't write to '%s': %s", name, strerror(errno));
		return 1;
	}
	
	if((k = fopen(kconfig, "a")) == NULL)
	{
		fclose(f);
		return 1;
	}
	
	if(verbose & VERBOSE_PRINT)
	{
		printf("write  config(%s)\n", name);
		printf("update config(%s)\n", kconfig);
	}
	
	switch (d->type)
	{
		case CLASS_AUDIO:
			
			if(d->desc)
			{
				fprintf(f, "FULLNAME=\"%s\"\n", d->desc);
				fprintf(k, "SOUND_FULLNAME=\"%s\"\n", d->desc);
			}
			
			if(d->driver)
			{
				fprintf(f, "DRIVER=\"%s\"\n", d->driver);
				fprintf(k, "SOUND_DRIVER=\"%s\"\n", d->driver);
			}
			
			break;
		
		case CLASS_MOUSE:
			if(d->bus == BUS_PSAUX)
			{
				t1 = "ps2";
				t2 = "PS/2";
			}
			else if(d->bus == BUS_USB)
			{
				t1 = "imps2";
				t2 = "IMPS/2";
			}
			else
			{
				t1 = "ms";
				t2 = "Microsoft";
			}
			
			fprintf(f, "MOUSETYPE=\"%s\"\nXMOUSETYPE=\"%s\"\n", t1, t2);
			
			if(d->desc)
			{
				fprintf(f, "FULLNAME=\"%s\"\n", d->desc);
				fprintf(k, "MOUSE_FULLNAME=\"%s\"\n", d->desc);
			}
			
			if(d->device)
			{
				fprintf(f, "DEVICE=\"/dev/%s\"\n", d->device);
				fprintf(k, "MOUSE_DEVICE=\"/dev/%s\"\n", d->device);
			}
			
			break;
		
		case CLASS_NETWORK:
			if(d->desc)
			{
				fprintf(f, "FULLNAME=\"%s\"\n", d->desc);
				fprintf(k, "NETCARD_FULLNAME=\"%s\"\n", d->desc);
			}
				
			if(d->driver)
			{
				fprintf(f, "DRIVER=\"%s\"\n", d->driver);
				fprintf(k, "NETCARD_DRIVER=\"%s\"\n", d->driver);
			}
			
			break;
		
		case CLASS_VIDEO:
			if(xi)
			{
				if(*xi->xserver)
				{
					fprintf(f, xserver, xi->xserver);
					fprintf(k, xserver, xi->xserver);
				}
				
				if(*xi->xmodule)
				{
					fprintf(f, xmodule, xi->xmodule);
					fprintf(k, xmodule, xi->xmodule);
				}
				
				if(*xi->xopts)
				{
					fprintf(f, xopts, xi->xopts);
					fprintf(k, xopts, xi->xopts);
				}
				
				if(*xi->xdesc)
				{
					fprintf(f, xdesc, xi->xdesc);
					fprintf(k, xdesc, xi->xdesc);
				}
			}
			
			break;
		
		case CLASS_FLOPPY:
			if(d->desc)
			{
				fprintf(f, "FULLNAME='%s'\n", d->desc);
				fprintf(k, "FLOPPY_FULLNAME='%s'\n", d->desc);
			}
			
			if(d->device)
			{
				fprintf(f, "DEVICE=\"/dev/%s\"\n", d->device),
				fprintf(k, "FLOPPY_DEVICE=\"/dev/%s\"\n", d->device);
			}
			
			if(d->driver)
			{
				fprintf(f, "DRIVER=\"%s\"\n", d->driver);
				fprintf(k, "FLOPPY_DRIVER=\"%s\"\n", d->driver);
			}
			
			break;
		
		default:
			break;
	}
	
	fclose(f);
	fclose(k);
	
	return 0;
}

int hw_setup(enum deviceClass dc, int verbose, int probeonly, int skip)
{
	int		i,
			mouse = 0,
			cdrom = 0,
			modem = 0,
			scanner = 0;
		
	struct device	**currentDevs,
			*d, *serialmouse = NULL,
			*usbmouse = NULL;
	
	if(verbose & VERBOSE_PROMPT)
		wpid = startwheel();
		
	if((currentDevs = probeDevices(dc, BUS_UNSPEC, PROBE_ALL)) == NULL)
		return -1;
		
	if(verbose & VERBOSE_PROMPT && wpid > 0)
	{
		kill(wpid, SIGTERM);
		wpid = 0;
		usleep((unsigned long)160000);
		write(2, "\033[0m Done.\n", 11);
	}
	
	for(i = 0; (d = currentDevs[i]); i++)
	{
		if(verbose & VERBOSE_PRINT)
			hw_info(d);
			
		if(!probeonly)
		{
#if defined(_i_wanna_build_this_crap_)
			if(d->bus == BUS_ISAPNP && configure_isapnp(d, verbose))
				continue;
#endif
			switch (d->type)
			{
				case CLASS_MOUSE:			/* Choose serial over PS2/USB mouse IF present  */
					if(d->bus == BUS_SERIAL)	/* For some reason, PS/2 ALWAYS detects a mouse */
					{
						mouse = 0;
						serialmouse = d;
					}
					else if(d->bus == BUS_USB)	/* Need usbdevfs for */
					{
						mouse = 0;
						usbmouse = d;		/* this to work */
						load_mod(d->driver, verbose);
					}
						
					if(!mouse)
						writeconfig(MOUSE_CNF, d, verbose);
						
					link_dev(d, "mouse", mouse++, verbose);
					break;
					
				case CLASS_CDROM:
					if(d->bus == BUS_USB)
						load_mod(d->driver, verbose);
					
					link_dev(d, "cdrom", cdrom++, verbose);
					break;
					
				case CLASS_MODEM:
					if(d->bus == BUS_USB)
						load_mod(d->driver, verbose);
					link_dev(d, "modem", modem++, verbose);
					break;
					
				case CLASS_SCANNER:
					if(d->bus == BUS_USB)
						load_mod(d->driver, verbose);
						
					link_dev(d, "scanner", scanner++, verbose);
					break;
					
				case CLASS_VIDEO:
					writeconfig(X_CNF, d, verbose);
					break;
					
				case CLASS_AUDIO:
					if(skip & SKIP_AUDIO)
						break;
						
					writeconfig(SOUND_CNF, d, verbose);
					load_mod(d->driver, verbose);
					break;
					
				case CLASS_NETWORK:
					writeconfig(NET_CNF, d, verbose);
					load_mod(d->driver, verbose);
					break;
					
				case CLASS_FLOPPY:
					writeconfig(FLOPPY_CNF, d, verbose);
					load_mod(d->driver, verbose);
					break;
					
				case CLASS_KEYBOARD:
					if(d->bus == BUS_USB)
						load_mod(d->driver, verbose);
					break;
					
				case CLASS_CAPTURE:	/* Just load the module for these */
				
				case CLASS_SCSI:
					if(skip & SKIP_SCSI)
						break;
						
				case CLASS_OTHER:	/* Yet unsupported or "guessed" devices in kudzu. Module probe may hang here. */
				
				case CLASS_RAID:
					load_mod(d->driver, verbose);
					break;
					
				case CLASS_SOCKET:	/* yenta_socket or similar is handled by knoppix-autoconfig */
				
				default:		/* do nothing */
					break;
			}
		}
	}
	
	return 0;
}

/*
 * int main(int argc, char **argv)
 *
 * let the show begin...
 */
int main(int argc, char **argv)
{
	int		i,
			verbose = 0,
			probeonly = 0,
			skip = 0;
			
	enum deviceClass dc = CLASS_UNSPEC;
	
	for(i = 1; i < argc; i++)
	{
		if(!strcasecmp(argv[i], "-v"))
			verbose |= VERBOSE_PRINT;
		else if(!strcasecmp(argv[i], "-p"))
			verbose |= VERBOSE_PROMPT;
		else if(!strcasecmp(argv[i], "-a"))
			skip |= SKIP_AUDIO;
		else if(!strcasecmp(argv[i], "-s"))
			skip |= SKIP_SCSI;
		else if(!strcasecmp(argv[i], "-n"))
			probeonly = 1;
		else
			return syntax(argv[i]);
	}
	
	/* Allow SIGTERM, SIGINT: rmmod depends on this. */
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGALRM, alarm_handler);
	alarm(MAX_TIME);
	
	return hw_setup(dc, verbose, probeonly, skip);
}

