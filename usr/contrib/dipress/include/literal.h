/*
 *  Interpress utilities
 *
 * Copyright (c) 1984, 1985 Xerox Corp.
 *
 *  Written for Xerox Corporation by William LeFebvre
 *  30-May-1984
 *
 */

/*
 *  Subroutines to help build interpress files:
 *
 *  literal interface level - these routines produce interpress output at
 *			      the token level.
 */

/*
 *  This file contains the macro definitions for some of the literal
 *  operations.  This is done for efficiency reasons.
 */

# define    append_short_number(number)	\
		append_word((unsigned short)(number + INTEGER_ZERO))

# define    AppendIdentifier(string)	\
		append_Sequence(sequenceIdentifier, strlen(string), (unsigned char *)string)

# define    AppendString(string)	\
		append_Sequence(sequenceString, strlen(string), (unsigned char *)string)

# define    AppendComment(string)	\
		append_Sequence(sequenceComment, strlen(string), (unsigned char *)string)

# define    AppendInsertFile(string)	\
		append_Sequence(sequenceInsertFile, strlen(string), (unsigned char *)string)

/* An abbreviation for AppendOp: */

#ifndef lint
# define    Op(string)		AppendOp((unsigned)OP_/**/string)
#else
# define    Op(string)		AppendOp(1)  /* is this the right thing? */
#endif
