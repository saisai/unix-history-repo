/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)csh.h	5.13 (Berkeley) %G%
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#ifdef NLS
#include <locale.h>
#endif
#include <time.h>
#include <limits.h>
#include <termios.h>
#include <errno.h>
#include <setjmp.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "pathnames.h"

#ifdef SHORT_STRINGS
typedef short Char;

#define SAVE(a) (Strsave(str2short(a)))
#else
typedef char Char;

#define SAVE(a) (strsave(a))
#endif

typedef void *ioctl_t;		/* Third arg of ioctl */
typedef long sigmask_t;		/* What a signal mask is */

typedef void *ptr_t;

#include "tc.const.h"
#include "local.h"
#include "char.h"
#include "err.h"

#ifdef SYSMALLOC
#define xmalloc(i)	Malloc(i)
#define xrealloc(p, i)	Realloc(p, i)
#define xcalloc(n, s)	Calloc(n, s)
#define xfree(p)	Free(p)
#else
#define xmalloc(i)	malloc(i)
#define xrealloc(p, i)	realloc(p, i)
#define xcalloc(n, s)	calloc(n, s)
#define xfree(p)	free(p)
#endif				/* SYSMALLOC */

#define	isdir(d)	((d.st_mode & S_IFMT) == S_IFDIR)

#define SIGN_EXTEND_CHAR(a) \
	((a) & 0x80 ? ((int) (a)) | 0xffffff00 : ((int) a) & 0x000000ff)


typedef int bool;

#define	eq(a, b)	(Strcmp(a, b) == 0)

/* globone() flags */
#define G_ERROR		0	/* default action: error if multiple words */
#define G_IGNORE	1	/* ignore the rest of the words */
#define G_APPEND	2	/* make a sentence by cat'ing the words */

/*
 * Global flags
 */
bool    chkstop;		/* Warned of stopped jobs... allow exit */
bool    didfds;			/* Have setup i/o fd's for child */
bool    doneinp;		/* EOF indicator after reset from readc */
bool    exiterr;		/* Exit if error or non-zero exit status */
bool    child;			/* Child shell ... errors cause exit */
bool    haderr;			/* Reset was because of an error */
bool    intty;			/* Input is a tty */
bool    intact;			/* We are interactive... therefore prompt */
bool    justpr;			/* Just print because of :p hist mod */
bool    loginsh;		/* We are a loginsh -> .login/.logout */
bool    neednote;		/* Need to pnotify() */
bool    noexec;			/* Don't execute, just syntax check */
bool    pjobs;			/* want to print jobs if interrupted */
bool    setintr;		/* Set interrupts on/off -> Wait intr... */
bool    timflg;			/* Time the next waited for command */
bool    havhash;		/* path hashing is available */

#ifdef FILEC
bool    filec;			/* doing filename expansion */

#endif

/*
 * Global i/o info
 */
Char   *arginp;			/* Argument input for sh -c and internal `xx` */
int     onelflg;		/* 2 -> need line for -t, 1 -> exit on read */
Char   *ffile;			/* Name of shell file for $0 */

char   *seterr;			/* Error message from scanner/parser */
Char   *shtemp;			/* Temp name for << shell files in /tmp */

struct timeval time0;		/* Time at which the shell started */
struct rusage ru0;

/*
 * Miscellany
 */
Char   *doldol;			/* Character pid for $$ */
int     uid;			/* Invokers uid */
int     gid;			/* Invokers gid */
time_t  chktim;			/* Time mail last checked */
int     shpgrp;			/* Pgrp of shell */
int     tpgrp;			/* Terminal process group */

/* If tpgrp is -1, leave tty alone! */
int     opgrp;			/* Initial pgrp and tty pgrp */

/*
 * These are declared here because they want to be
 * initialized in sh.init.c (to allow them to be made readonly)
 */

extern struct biltins {
    char   *bname;
    void    (*bfunct) ();
    short   minargs, maxargs;
}       bfunc[];
extern int nbfunc;

extern struct srch {
    char   *s_name;
    short   s_value;
}       srchn[];
extern int nsrchn;

/*
 * To be able to redirect i/o for builtins easily, the shell moves the i/o
 * descriptors it uses away from 0,1,2.
 * Ideally these should be in units which are closed across exec's
 * (this saves work) but for version 6, this is not usually possible.
 * The desired initial values for these descriptors are defined in
 * local.h.
 */
short   SHIN;			/* Current shell input (script) */
short   SHOUT;			/* Shell output */
short   SHDIAG;			/* Diagnostic output... shell errs go here */
short   OLDSTD;			/* Old standard input (def for cmds) */

/*
 * Error control
 *
 * Errors in scanning and parsing set up an error message to be printed
 * at the end and complete.  Other errors always cause a reset.
 * Because of source commands and .cshrc we need nested error catches.
 */

jmp_buf reslab;

#define	setexit()	(setjmp(reslab))
#define	reset()		longjmp(reslab, 1)
 /* Should use structure assignment here */
#define	getexit(a)	bcopy((char *)reslab, ((char *)(a)), sizeof reslab)
#define	resexit(a)	bcopy((char *)(a), (char *)reslab, sizeof reslab)

Char   *gointr;			/* Label for an onintr transfer */

sig_t parintr;			/* Parents interrupt catch */
sig_t parterm;			/* Parents terminate catch */

/*
 * Lexical definitions.
 *
 * All lexical space is allocated dynamically.
 * The eighth/sizteenth bit of characters is used to prevent recognition,
 * and eventually stripped.
 */
#define	META		0200
#define	ASCII		0177
#ifdef SHORT_STRINGS
#define	CHAR		0377
#define	QUOTE 		0100000	/* 16nth char bit used for 'ing */
#define	TRIM		0077777	/* Mask to strip quote bit */
#else
#define	CHAR		0177
#define	QUOTE 		0200	/* Eighth char bit used for 'ing */
#define	TRIM		0177	/* Mask to strip quote bit */
#endif

int     AsciiOnly;		/* If set only 7 bits is expected in characters */

/*
 * Each level of input has a buffered input structure.
 * There are one or more blocks of buffered input for each level,
 * exactly one if the input is seekable and tell is available.
 * In other cases, the shell buffers enough blocks to keep all loops
 * in the buffer.
 */
struct Bin {
    off_t   Bfseekp;		/* Seek pointer */
    off_t   Bfbobp;		/* Seekp of beginning of buffers */
    off_t   Bfeobp;		/* Seekp of end of buffers */
    short   Bfblocks;		/* Number of buffer blocks */
    Char  **Bfbuf;		/* The array of buffer blocks */
}       B;

#define	fseekp	B.Bfseekp
#define	fbobp	B.Bfbobp
#define	feobp	B.Bfeobp
#define	fblocks	B.Bfblocks
#define	fbuf	B.Bfbuf

/*
 * The shell finds commands in loops by reseeking the input
 * For whiles, in particular, it reseeks to the beginning of the
 * line the while was on; hence the while placement restrictions.
 */
off_t   lineloc;

bool    cantell;		/* Is current source tellable ? */

/*
 * Input lines are parsed into doubly linked circular
 * lists of words of the following form.
 */
struct wordent {
    Char   *word;
    struct wordent *prev;
    struct wordent *next;
};

/*
 * During word building, both in the initial lexical phase and
 * when expanding $ variable substitutions, expansion by `!' and `$'
 * must be inhibited when reading ahead in routines which are themselves
 * processing `!' and `$' expansion or after characters such as `\' or in
 * quotations.  The following flags are passed to the getC routines
 * telling them which of these substitutions are appropriate for the
 * next character to be returned.
 */
#define	DODOL	1
#define	DOEXCL	2
#define	DOALL	DODOL|DOEXCL

/*
 * Labuf implements a general buffer for lookahead during lexical operations.
 * Text which is to be placed in the input stream can be stuck here.
 * We stick parsed ahead $ constructs during initial input,
 * process id's from `$$', and modified variable values (from qualifiers
 * during expansion in sh.dol.c) here.
 */
Char   *lap;

/*
 * Parser structure
 *
 * Each command is parsed to a tree of command structures and
 * flags are set bottom up during this process, to be propagated down
 * as needed during the semantics/exeuction pass (sh.sem.c).
 */
struct command {
    short   t_dtyp;		/* Type of node 		 */
#define	NODE_COMMAND	1	/* t_dcom <t_dlef >t_drit	 */
#define	NODE_PAREN	2	/* ( t_dspr ) <t_dlef >t_drit	 */
#define	NODE_PIPE	3	/* t_dlef | t_drit		 */
#define	NODE_LIST	4	/* t_dlef ; t_drit		 */
#define	NODE_OR		5	/* t_dlef || t_drit		 */
#define	NODE_AND	6	/* t_dlef && t_drit		 */
    short   t_dflg;		/* Flags, e.g. F_AMPERSAND|... 	 */
#define	F_SAVE	(F_NICE|F_TIME|F_NOHUP)	/* save these when re-doing 	 */

#define	F_AMPERSAND	(1<<0)	/* executes in background	 */
#define	F_APPEND	(1<<1)	/* output is redirected >>	 */
#define	F_PIPEIN	(1<<2)	/* input is a pipe		 */
#define	F_PIPEOUT	(1<<3)	/* output is a pipe		 */
#define	F_NOFORK	(1<<4)	/* don't fork, last ()ized cmd	 */
#define	F_NOINTERRUPT	(1<<5)	/* should be immune from intr's */
/* spare */
#define	F_STDERR	(1<<7)	/* redirect unit 2 with unit 1	 */
#define	F_OVERWRITE	(1<<8)	/* output was !			 */
#define	F_READ		(1<<9)	/* input redirection is <<	 */
#define	F_REPEAT	(1<<10)	/* reexec aft if, repeat,...	 */
#define	F_NICE		(1<<11)	/* t_nice is meaningful 	 */
#define	F_NOHUP		(1<<12)	/* nohup this command 		 */
#define	F_TIME		(1<<13)	/* time this command 		 */
    union {
	Char   *T_dlef;		/* Input redirect word 		 */
	struct command *T_dcar;	/* Left part of list/pipe 	 */
    }       L;
    union {
	Char   *T_drit;		/* Output redirect word 	 */
	struct command *T_dcdr;	/* Right part of list/pipe 	 */
    }       R;
#define	t_dlef	L.T_dlef
#define	t_dcar	L.T_dcar
#define	t_drit	R.T_drit
#define	t_dcdr	R.T_dcdr
    Char  **t_dcom;		/* Command/argument vector 	 */
    struct command *t_dspr;	/* Pointer to ()'d subtree 	 */
    short   t_nice;
};


/*
 * The keywords for the parser
 */
#define	T_BREAK		0
#define	T_BRKSW		1
#define	T_CASE		2
#define	T_DEFAULT 	3
#define	T_ELSE		4
#define	T_END		5
#define	T_ENDIF		6
#define	T_ENDSW		7
#define	T_EXIT		8
#define	T_FOREACH	9
#define	T_GOTO		10
#define	T_IF		11
#define	T_LABEL		12
#define	T_LET		13
#define	T_SET		14
#define	T_SWITCH	15
#define	T_TEST		16
#define	T_THEN		17
#define	T_WHILE		18

/*
 * Structure defining the existing while/foreach loops at this
 * source level.  Loops are implemented by seeking back in the
 * input.  For foreach (fe), the word list is attached here.
 */
struct whyle {
    off_t   w_start;		/* Point to restart loop */
    off_t   w_end;		/* End of loop (0 if unknown) */
    Char  **w_fe, **w_fe0;	/* Current/initial wordlist for fe */
    Char   *w_fename;		/* Name for fe */
    struct whyle *w_next;	/* Next (more outer) loop */
}      *whyles;

/*
 * Variable structure
 *
 * Aliases and variables are stored in AVL balanced binary trees.
 */
struct varent {
    Char  **vec;		/* Array of words which is the value */
    Char   *v_name;		/* Name of variable/alias */
    struct varent *v_link[3];	/* The links, see below */
    int     v_bal;		/* Balance factor */
}       shvhed, aliases;

#define v_left		v_link[0]
#define v_right		v_link[1]
#define v_parent	v_link[2]

struct varent *adrof1();

#define adrof(v)	adrof1(v, &shvhed)
#define value(v)	value1(v, &shvhed)

/*
 * The following are for interfacing redo substitution in
 * aliases to the lexical routines.
 */
struct wordent *alhistp;	/* Argument list (first) */
struct wordent *alhistt;	/* Node after last in arg list */
Char  **alvec;			/* The (remnants of) alias vector */

/*
 * Filename/command name expansion variables
 */
short   gflag;			/* After tglob -> is globbing needed? */

#define MAXVARLEN 30		/* Maximum number of char in a variable name */

/*
 * Variables for filename expansion
 */
extern Char **gargv;		/* Pointer to the (stack) arglist */
extern long gargc;		/* Number args in gargv */

/*
 * Variables for command expansion.
 */
extern Char **pargv;		/* Pointer to the argv list space */
extern long pargc;		/* Count of arguments in pargv */
Char   *pargs;			/* Pointer to start current word */
long    pnleft;			/* Number of chars left in pargs */
Char   *pargcp;			/* Current index into pargs */

/*
 * History list
 *
 * Each history list entry contains an embedded wordlist
 * from the scanner, a number for the event, and a reference count
 * to aid in discarding old entries.
 *
 * Essentially "invisible" entries are put on the history list
 * when history substitution includes modifiers, and thrown away
 * at the next discarding since their event numbers are very negative.
 */
struct Hist {
    struct wordent Hlex;
    int     Hnum;
    int     Href;
    long    Htime;
    struct Hist *Hnext;
}       Histlist;

struct wordent paraml;		/* Current lexical word list */
int     eventno;		/* Next events number */
int     lastev;			/* Last event reference (default) */

Char    HIST;			/* history invocation character */
Char    HISTSUB;		/* auto-substitute character */

/*
 * strings.h:
 */
#ifndef SHORT_STRINGS
#define Strchr(a, b)		strchr(a, b)
#define Strrchr(a, b)		strrchr(a, b)
#define Strcat(a, b)		strcat(a, b)
#define Strncat(a, b, c) 	strncat(a, b, c)
#define Strcpy(a, b)		strcpy(a, b)
#define Strncpy(a, b, c) 	strncpy(a, b, c)
#define Strlen(a)		strlen(a)
#define Strcmp(a, b)		strcmp(a, b)
#define Strncmp(a, b, c)	strncmp(a, b, c)

#define Strspl(a, b)		strspl(a, b)
#define Strsave(a)		strsave(a)
#define Strend(a)		strend(a)
#define Strstr(a, b)		strstr(a, b)

#define str2short(a) 		(a)
#define blk2short(a) 		saveblk(a)
#define short2blk(a) 		saveblk(a)
#define short2str(a) 		(a)
#define short2qstr(a)		(a)
#else
#define Strchr(a, b)		s_strchr(a, b)
#define Strrchr(a, b) 		s_strrchr(a, b)
#define Strcat(a, b)		s_strcat(a, b)
#define Strncat(a, b, c) 	s_strncat(a, b, c)
#define Strcpy(a, b)		s_strcpy(a, b)
#define Strncpy(a, b, c)	s_strncpy(a, b, c)
#define Strlen(a)		s_strlen(a)
#define Strcmp(a, b)		s_strcmp(a, b)
#define Strncmp(a, b, c)	s_strncmp(a, b, c)

#define Strspl(a, b)		s_strspl(a, b)
#define Strsave(a)		s_strsave(a)
#define Strend(a)		s_strend(a)
#define Strstr(a, b)		s_strstr(a, b)
#endif

/*
 * setname is a macro to save space (see sh.err.c)
 */
char   *bname;

#define	setname(a)	(bname = (a))

Char   *Vsav;
Char   *Vdp;
Char   *Vexpath;
char  **Vt;

Char  **evalvec;
Char   *evalp;

extern struct mesg {
    char   *iname;		/* name from /usr/include */
    char   *pname;		/* print name */
}       mesg[];

/* word_chars is set by default to WORD_CHARS but can be overridden by
   the worchars variable--if unset, reverts to WORD_CHARS */

Char   *word_chars;

#define WORD_CHARS "*?_-.[]~="	/* default chars besides alnums in words */

Char   *STR_SHELLPATH;

#ifdef _PATH_BSHELL
Char   *STR_BSHELL;

#endif
Char   *STR_WORD_CHARS;
Char  **STR_environ;
