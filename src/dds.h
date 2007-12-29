/*****************************************************************************
$Id$

		dds.h -- Deyu Deyu Software's standard include file
		Copyright(C) 1997 - 2001 by Deyu Deyu Software

*****************************************************************************/

#ifndef _DDS_H
#define _DDS_H

#define	Case		break; case
#define Default		break; default
#define until( x )	while( !( x ))
#define INLINE		static __inline

#define FALSE		0
#define TRUE		1

/*** new type ***************************************************************/

typedef unsigned short	USHORT;
typedef unsigned		UINT;
typedef unsigned char	UCHAR;
typedef UCHAR			BOOL;

#define SEC_DATA_BYTE	__attribute__(( section( ".data.byte" )))
#define SEC_BSS_BYTE	__attribute__(( section( ".bss.byte" )))
#define SEC_BSS_WORD	__attribute__(( section( ".bss.word" )))

/*** debug macros ***********************************************************/

#if defined _DEBUG && !defined DEBUG
# define DEBUG
#endif

#ifdef DEBUG
# define IfDebug			if( 1 )
# define IfNoDebug			if( 0 )
# define DebugCmd( x )		x
# define NoDebugCmd( x )
# define DebugParam( x, y )	( x )
//# define DebugMsg			Kprintf

void DebugMsg( char *format, ... );

/*** no debug macros ********************************************************/

#else	/* _DEBUG */
# define IfDebug			if( 0 )
# define IfNoDebug			if( 1 )
# define DebugCmd( x )
# define NoDebugCmd( x )	x
# define DebugParam( x, y )	( y )
# define DebugMsg( ... )
#endif	/* _DEBUG */

#endif	/* !def _DDS_H */
