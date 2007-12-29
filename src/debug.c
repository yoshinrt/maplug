// $Id$
/*** デバッグメッセージ シリアル出力 ****************************************/
// なぜか 3.30 では Kprintf が動かないので

#include <pspkernel.h>
#include <stdarg.h>
#include "dds.h"

#define putchar	DebugPutchar
#define puts	DebugPuts

char g_szPrintfBuf[ 16 ];

// 文字列
void DebugPutchar( char c ){
	if( c == '\n' ) pspDebugSioPutchar( '\r' );
	pspDebugSioPutchar( c );
}

void DebugPuts( char *s ) {
	while( *s ) DebugPutchar( *s++ );
}

// decimal
void DebugPutInt( int i ){
	int	iSign	= 0;
	int	iLen	= 0;
	
	char *p = &g_szPrintfBuf[ sizeof( g_szPrintfBuf ) - 1 ];
	*p = '\0';
	
	if ( i < 0 ){
		i		= -i;
		iSign	= 1;
	}
	
	do{
		*( --p ) = i % 10 + '0';
		++iLen;
	}while( i /= 10 );
	
	if( iSign ){
		*( --p ) = '-';
	}
	puts( p );
}

// Hex
void DebugPutHex( UINT u ){
	char *p = &g_szPrintfBuf[ sizeof( g_szPrintfBuf ) - 1 ];
	*p = '\0';
	
	int	i;
	
	for( i = 0; i < 8; ++i  ){
		*( --p ) = ( u & 0xF ) + (( u & 0xF ) >= 0xA ? 'A' - 10 : '0' );
		u >>= 4;
	}
	
	puts( p );
}

// 簡易 printf
void DebugMsg( char *fmt, ... ){
	va_list argptr;
	
	va_start( argptr, fmt );
	
	for( ; *fmt; fmt++ ){
		if( *fmt == '%' ){
			++fmt;
			while( '0' <= *fmt && *fmt <= '9' ) ++fmt;
			switch( *fmt ){
				case 'd':	DebugPutInt( va_arg( argptr, int ));
				Case 'X':	DebugPutHex( va_arg( argptr, UINT ));
				Case 's':	puts( va_arg( argptr, char * ));
			}
		}else{
			putchar( *fmt );
		}
	}
	va_end( argptr );
}
