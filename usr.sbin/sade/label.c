/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: label.c,v 1.4 1995/05/17 15:41:52 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <ctype.h>
#include <sys/disklabel.h>

/*
 * Everything to do with editing the contents of disk labels.
 */

/* A nice message we use a lot in the disklabel editor */
#define MSG_NOT_APPLICABLE	"That option is not applicable here"

/*
 * I make some pretty gross assumptions about having a max of 50 chunks
 * total - 8 slices and 42 partitions.  I can't easily display many more
 * than that on the screen at once!
 *
 * For 2.1 I'll revisit this and try to make it more dynamic, but since
 * this will catch 99.99% of all possible cases, I'm not too worried.
 */
#define MAX_CHUNKS	50

/* Where to start printing the freebsd slices */
#define CHUNK_SLICE_START_ROW		2
#define CHUNK_PART_START_ROW		10

/* The smallest filesystem we're willing to create */
#define FS_MIN_SIZE			2048


/* All the chunks currently displayed on the screen */
static struct {
    struct disk *d;
    struct chunk *c;
    PartType type;
} label_chunk_info[MAX_CHUNKS + 1];
static int here;

/* See if we're already using a desired partition name */
static Boolean
check_conflict(char *name)
{
    int i;

    for (i = 0; label_chunk_info[i].d; i++)
	if (label_chunk_info[i].type == PART_FILESYSTEM
	    && label_chunk_info[i].c->private
	    && !strcmp(((PartInfo *)label_chunk_info[i].c->private)->mountpoint, name))
	    return TRUE;
    return FALSE;
}

/* How much space is in this FreeBSD slice? */
static int
space_free(struct chunk *c)
{
    struct chunk *c1 = c->part;
    int sz = c->size;

    while (c1) {
	if (c1->type != unused)
	    sz -= c1->size;
	c1 = c1->next;
    }
    if (sz < 0)
	msgFatal("Partitions are larger than actual chunk??");
    return sz;
}

/* Snapshot the current situation into the displayed chunks structure */
static void
record_label_chunks()
{
    int i, j, p;
    struct chunk *c1, *c2;
    Device **devs;
    Disk *d;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	return;
    }

    j = p = 0;
    /* First buzz through and pick up the FreeBSD slices */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	d = (Disk *)devs[i]->private;
	if (!d->chunks)
	    msgFatal("No chunk list found for %s!", d->name);

	/* Put the slice entries first */
	for (c1 = d->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		label_chunk_info[j].type = PART_SLICE;
		label_chunk_info[j].d = d;
		label_chunk_info[j].c = c1;
		++j;
	    }
	}
    }
    /* Now run through again and get the FreeBSD partition entries */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	d = (Disk *)devs[i]->private;
	/* Then buzz through and pick up the partitions */
	for (c1 = d->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part) {
			if (c2->subtype == FS_SWAP)
			    label_chunk_info[j].type = PART_SWAP;
			else
			    label_chunk_info[j].type = PART_FILESYSTEM;
			label_chunk_info[j].d = d;
			label_chunk_info[j].c = c2;
			++j;
		    }
		}
	    }
	    else if (c1->type == fat) {
		label_chunk_info[j].type = PART_FAT;
		label_chunk_info[j].d = d;
		label_chunk_info[j].c = c1;
	    }
	}
    }
    label_chunk_info[j].d = NULL;
    label_chunk_info[j].c = NULL;
    if (here >= j)
	here = j  ? j - 1 : 0;
}

/* A new partition entry */
static PartInfo *
new_part(char *mpoint, Boolean newfs)
{
    PartInfo *ret;

    ret = (PartInfo *)safe_malloc(sizeof(PartInfo));
    strncpy(ret->mountpoint, mpoint, FILENAME_MAX);
    strcpy(ret->newfs_cmd, "newfs");
    ret->newfs = newfs;
    return ret;
}

/* Get the mountpoint for a partition and save it away */
PartInfo *
get_mountpoint(struct chunk *parent, struct chunk *me)
{
    char *val;
    PartInfo *tmp;

    val = msgGetInput(me && me->private ? ((PartInfo *)me->private)->mountpoint : NULL,
		      "Please specify a mount point for the partition");
    if (val) {
	/* Is it just the same value? */
	if (me && me->private && !strcmp(((PartInfo *)me->private)->mountpoint, val))
	    return NULL;
	if (check_conflict(val)) {
	    msgConfirm("You already have a mount point for %s assigned!", val);
	    return NULL;
	}
	else if (*val != '/') {
	    msgConfirm("Mount point must start with a / character");
	    return NULL;
	}
	else if (!strcmp(val, "/")) {
#if 0
	    if (parent) {
	    	if (parent->flags & CHUNK_PAST_1024) {
		    msgConfirm("This region cannot be used for your root partition as\nit is past the 1024'th cylinder mark and the system would not be\nable to boot from it.  Please pick another location for your\nroot partition and try again!");
		    return NULL;
		}
		else if (!(parent->flags & CHUNK_BSD_COMPAT)) {
		    msgConfirm("This region cannot be used for your root partition as\nthe FreeBSD boot code cannot deal with a root partition created in\nsuch a region.  Please choose another partition for this.");
		    return NULL;
		}
	    }
#endif
	    if (me)
		me->flags |= CHUNK_IS_ROOT;
	}
	else if (me)
	    me->flags &= ~CHUNK_IS_ROOT;
	safe_free(me ? me->private : NULL);
	tmp = new_part(val, TRUE);
	if (me) {
	    me->private = tmp;
	    me->private_free = safe_free;
	}
	return tmp;
    }
    return NULL;
}

/* Get the type of the new partiton */
static PartType
get_partition_type(void)
{
    char selection[20];
    static unsigned char *fs_types[] = {
	"FS",
	"A file system",
	"Swap",
	"A swap partition.",
    };

    if (!dialog_menu("Please choose a partition type",
		    "If you want to use this partition for swap space, select Swap.\nIf you want to put a filesystem on it, choose FS.", -1, -1, 2, 2, fs_types, selection, NULL, NULL)) {
	if (!strcmp(selection, "FS"))
	    return PART_FILESYSTEM;
	else if (!strcmp(selection, "Swap"))
	    return PART_SWAP;
    }
    return PART_NONE;
}

/* If the user wants a special newfs command for this, set it */
static void
getNewfsCmd(PartInfo *p)
{
    char *val;

    val = msgGetInput(p->newfs_cmd,
		      "Please enter the newfs command and options you'd like to use in\ncreating this file system.");
    if (val)
	strncpy(p->newfs_cmd, val, NEWFS_CMD_MAX);
}


#define MAX_MOUNT_NAME	12

#define PART_PART_COL	0
#define PART_MOUNT_COL	8
#define PART_SIZE_COL	(PART_MOUNT_COL + MAX_MOUNT_NAME + 3)
#define PART_NEWFS_COL	(PART_SIZE_COL + 7)
#define PART_OFF	38

/* How many mounted partitions to display in column before going to next */
#define CHUNK_COLUMN_MAX	6

/* stick this all up on the screen */
static void
print_label_chunks(void)
{
    int i, j, srow, prow, pcol;
    int sz;

    dialog_clear();
    attrset(A_REVERSE);
    mvaddstr(0, 25, "FreeBSD Disklabel Editor");
    attrset(A_NORMAL);

    for (i = 0; i < 2; i++) {
	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_PART_COL + (i * PART_OFF),
		 "Part");
	attrset(A_NORMAL);

	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_MOUNT_COL + (i * PART_OFF),
		 "Mount");
	attrset(A_NORMAL);

	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_SIZE_COL + (i * PART_OFF) + 2,
		 "Size");
	attrset(A_NORMAL);

	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_NEWFS_COL + (i * PART_OFF),
		 "Newfs");
	attrset(A_NORMAL);
    }

    srow = CHUNK_SLICE_START_ROW;
    prow = CHUNK_PART_START_ROW;
    pcol = 0;

    for (i = 0; label_chunk_info[i].d; i++) {
	if (i == here)
	    attrset(A_REVERSE);
	/* Is it a slice entry displayed at the top? */
	if (label_chunk_info[i].type == PART_SLICE) {
	    sz = space_free(label_chunk_info[i].c);
	    mvprintw(srow++, 0,
		     "Disk: %s\tPartition name: %s\tFree: %d blocks (%dMB)",
		     label_chunk_info[i].d->name,
		     label_chunk_info[i].c->name, sz, (sz / 2048));
	}
	/* Otherwise it's a DOS, swap or filesystem entry, at the bottom */
	else {
	    char onestr[PART_OFF], num[10], *mountpoint, *newfs;

	    /*
	     * We copy this into a blank-padded string so that it looks like
	     * a solid bar in reverse-video
	     */
	    memset(onestr, ' ', PART_OFF - 1);
	    onestr[PART_OFF - 1] = '\0';
	    /* Go for two columns */
	    if (prow == (CHUNK_PART_START_ROW + CHUNK_COLUMN_MAX)) {
		pcol = PART_OFF;
		prow = CHUNK_PART_START_ROW;
	    }
	    memcpy(onestr + PART_PART_COL, label_chunk_info[i].c->name,
		   strlen(label_chunk_info[i].c->name));
	    /* If it's a filesystem, display the mountpoint */
	    if (label_chunk_info[i].type == PART_FILESYSTEM) {
		if (label_chunk_info[i].c->private == NULL) {
		    static int mnt = 0;
		    char foo[10];

		    /*
		     * Hmm!  A partition that must have already been here.
		     * Fill in a fake mountpoint and register it
		     */
		    sprintf(foo, "/mnt%d", mnt++);
		    label_chunk_info[i].c->private = new_part(foo, FALSE);
		    label_chunk_info[i].c->private_free = safe_free;
		}
		mountpoint = ((PartInfo *)label_chunk_info[i].c->private)->mountpoint;
		newfs = ((PartInfo *)label_chunk_info[i].c->private)->newfs ? "Y" : "N";
	    }
	    else if (label_chunk_info[i].type == PART_SWAP) {
		mountpoint = "swap";
		newfs = " ";
	    }
	    else if (label_chunk_info[i].type == PART_FAT) {
		mountpoint = "DOS FAT";
		newfs = "*";
	    }
	    else {
		mountpoint = "<unknown>";
		newfs = "*";
	    }
	    for (j = 0; j < MAX_MOUNT_NAME && mountpoint[j]; j++)
		onestr[PART_MOUNT_COL + j] = mountpoint[j];
	    snprintf(num, 10, "%4ldMB", label_chunk_info[i].c->size ?
		    label_chunk_info[i].c->size / 2048 : 0);
	    memcpy(onestr + PART_SIZE_COL, num, strlen(num));
	    memcpy(onestr + PART_NEWFS_COL, newfs, strlen(newfs));
	    onestr[PART_NEWFS_COL + strlen(newfs)] = '\0';
	    mvaddstr(prow, pcol, onestr);
	    ++prow;
	}
	if (i == here)
	    attrset(A_NORMAL);
    }
}

static void
print_command_summary()
{
    mvprintw(17, 0,
	     "The following commands are valid here (upper or lower case):");
    mvprintw(19, 0, "C = Create Partition   D = Delete Partition   M = Mount Partition");
    mvprintw(20, 0, "N = Newfs Options      T = Toggle Newfs       ESC = Exit this screen");
    mvprintw(21, 0, "The default target will be displayed in ");

    attrset(A_REVERSE);
    addstr("reverse video.");
    attrset(A_NORMAL);
    mvprintw(22, 0, "Use F1 or ? to get more help, arrow keys to move.");
    move(0, 0);
}

int
diskLabelEditor(char *str)
{
    int sz, key = 0;
    Boolean labeling;
    char *msg = NULL;
    PartInfo *p;
    PartType type;

    labeling = TRUE;
    keypad(stdscr, TRUE);
    record_label_chunks();

    if (!getenv(DISK_PARTITIONED)) {
	msgConfirm("You need to partition your disk(s) before you can assign disk labels.");
	return 0;
    }
    while (labeling) {
	print_label_chunks();
	print_command_summary();
	if (msg) {
	    attrset(A_REVERSE); mvprintw(23, 0, msg); attrset(A_NORMAL);
	    beep();
	    msg = NULL;
	}
	refresh();
	key = toupper(getch());
	switch (key) {

	case KEY_UP:
	case '-':
	    if (here != 0)
		--here;
	    break;

	case KEY_DOWN:
	case '+':
	case '\r':
	case '\n':
	    if (label_chunk_info[here + 1].d)
		++here;
	    break;

	case KEY_HOME:
	    here = 0;
	    break;

	case KEY_END:
	    while (label_chunk_info[here + 1].d)
		++here;
	    break;

	case KEY_F(1):
	case '?':
	    systemDisplayFile("disklabel.hlp");
	    break;

	case 'C':
	    if (label_chunk_info[here].type != PART_SLICE) {
		msg = "You can only do this in a master partition (see top of screen)";
		break;
	    }
	    sz = space_free(label_chunk_info[here].c);
	    if (sz <= FS_MIN_SIZE)
		msg = "Not enough space to create additional FreeBSD partition";
	    else {
		char *val, *cp, tmp[20];
		int size;

		snprintf(tmp, 20, "%d", sz);
		val = msgGetInput(tmp, "Please specify the size for new FreeBSD partition in blocks, or append\na trailing `M' for megabytes (e.g. 20M).");
		if (val && (size = strtol(val, &cp, 0)) > 0) {
		    struct chunk *tmp;
		    u_long flags = 0;

		    if (*cp && toupper(*cp) == 'M')
			size *= 2048;
		    
		    type = get_partition_type();
		    if (type == PART_NONE)
			break;
		    else if (type == PART_FILESYSTEM) {
			if ((p = get_mountpoint(label_chunk_info[here].c, NULL)) == NULL)
			    break;
			else if (!strcmp(p->mountpoint, "/"))
			    flags |= CHUNK_IS_ROOT;
			else
			    flags &= ~CHUNK_IS_ROOT;
		    }
		    else
			p = NULL;

		    tmp = Create_Chunk_DWIM(label_chunk_info[here].d,
					    label_chunk_info[here].c,
					    size, part,
					    (type == PART_SWAP) ? FS_SWAP : FS_BSDFFS,
					    flags);
		    if (!tmp)
			msgConfirm("Unable to create the partition. Too big?");
		    else {
			tmp->private = p;
			tmp->private_free = safe_free;
			record_label_chunks();
		    }
		}
	    }
	    break;

	case 'D':	/* delete */
	    if (label_chunk_info[here].type == PART_SLICE) {
		msg = MSG_NOT_APPLICABLE;
		break;
	    }
	    else if (label_chunk_info[here].type == PART_FAT) {
		msg = "Use the Disk Partition Editor to delete this";
		break;
	    }
	    Delete_Chunk(label_chunk_info[here].d, label_chunk_info[here].c);
	    record_label_chunks();
	    break;

	case 'M':	/* mount */
	    switch(label_chunk_info[here].type) {
	    case PART_SLICE:
		msg = MSG_NOT_APPLICABLE;
		break;

	    case PART_SWAP:
		msg = "You don't need to specify a mountpoint for a swap partition.";
		break;

	    case PART_FAT:
	    case PART_FILESYSTEM:
		p = get_mountpoint(NULL, label_chunk_info[here].c);
		if (p) {
		    p->newfs = FALSE;
		    record_label_chunks();
		}
		break;

	    default:
		msgFatal("Bogus partition under cursor???");
		break;
	    }
	    break;

	case 'N':	/* Set newfs options */
	    if (label_chunk_info[here].c->private &&
		((PartInfo *)label_chunk_info[here].c->private)->newfs)
		getNewfsCmd(label_chunk_info[here].c->private);
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'T':	/* Toggle newfs state */
	    if (label_chunk_info[here].type == PART_FILESYSTEM &&
		label_chunk_info[here].c->private)
		((PartInfo *)label_chunk_info[here].c->private)->newfs =
		    !((PartInfo *)label_chunk_info[here].c->private)->newfs;
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'W':
	    if (!msgYesNo("Are you sure you want to go into Wizard mode?\n\nThis is an entirely undocumented feature which you are not\nexpected to understand!")) {
		int i;
		Device **devs;

		dialog_clear();
		end_dialog();
		DialogActive = FALSE;
		devs = deviceFind(NULL, DEVICE_TYPE_DISK);
		if (!devs) {
		    msgConfirm("Can't find any disk devicse!");
		    break;
		}
		for (i = 0; ((Disk *)devs[i]->private); i++)
		    slice_wizard(((Disk *)devs[i]->private));
		dialog_clear();
		DialogActive = TRUE;
		record_label_chunks();
	    }
	    else
		msg = "A most prudent choice!";
	    break;

	case 27:	/* ESC */
	    labeling = FALSE;
	    break;

	default:
	    beep();
	    msg = "Type F1 or ? for help";
	    break;
	}
    }
    variable_set2(DISK_LABELLED, "yes");
    dialog_clear();
    refresh();
    return 0;
}



