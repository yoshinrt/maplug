/****************************************************************************/
// $Id$

#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <pspctrl.h>
#include <string.h>
#include <stdio.h>
#include "pspusbgps.h"

#include "dds.h"

PSP_MODULE_INFO( "ddsMaplug", 0x1000, 1, 1 );
PSP_MAIN_THREAD_ATTR( 0 );

/****************************************************************************/

#define THREAD_PRIORITY 		100
#define MAIN_THREAD_DELAY		100000	// 10Hz

/*** 機能選択 ***************************************************************/

/*** new type ***************************************************************/

struct SyscallHeader {
	void *unk;
	unsigned int basenum;
	unsigned int topnum;
	unsigned int size;
};

typedef int ( *SCE_USB_GPS_OPEN )( void );		// Open the GPS device
typedef int ( *SCE_USB_GPS_CLOSE )( void );		// Close the GPS device
typedef int ( *SCE_USB_GPS_GET_DATA )( gpsdata *buffer, satdata *satellites );	// Get data from GPS ( size of buffer = 0x30 u32 ? )

typedef int ( *SCE_CTRL_READ_BUFFER )( SceCtrlData *pad_data, int count );
typedef int ( *SCE_IO_OPEN )( const char *file, int flags, SceMode mode );

typedef int ( *SCE_AUDIO_OUTPUT )( int channel, int vol, void *buf );
typedef int ( *SCE_AUDIO_OUTPUT_BLOCKING )( int channel, int vol, void *buf );
typedef int ( *SCE_AUDIO_OUTPUT_PANNED )( int channel, int leftvol, int rightvol, void *buffer );
typedef int ( *SCE_AUDIO_OUTPUT_PANNED_BLOCKING )( int channel, int leftvol, int rightvol, void *buffer );

/*** GPS フック関数 *********************************************************/

SCE_USB_GPS_GET_DATA	sceUsbGpsGetData_Real;

UCHAR	g_cGPSValid	SEC_BSS_BYTE;

int sceUsbGpsGetData_Hook( gpsdata *buffer, satdata *satellites ){
	
	int iRet = sceUsbGpsGetData_Real( buffer, satellites );
	g_cGPSValid = buffer->valid;
	return iRet;
}

/*** Audio hook proc. *******************************************************/

#define NID_sceAudioOutput					0x8C1009B2
#define NID_sceAudioOutputBlocking			0x136CAF51
#define NID_sceAudioOutputPanned			0xE2D56B2D
#define NID_sceAudioOutputPannedBlocking	0x13F592BC

SCE_AUDIO_OUTPUT					sceAudioOutput_Real;
SCE_AUDIO_OUTPUT_BLOCKING			sceAudioOutputBlocking_Real;
SCE_AUDIO_OUTPUT_PANNED				sceAudioOutputPanned_Real;
SCE_AUDIO_OUTPUT_PANNED_BLOCKING	sceAudioOutputPannedBlocking_Real;

int sceAudioOutput_Hook( int channel, int vol, void *buf ){
	DebugMsg( "O" );
	return sceAudioOutput_Real( channel, vol, buf );
}

int sceAudioOutputBlocking_Hook( int channel, int vol, void *buf ){
	DebugMsg( "B" );
	return sceAudioOutputBlocking_Real( channel, vol, buf );
}

int sceAudioOutputPanned_Hook( int channel, int leftvol, int rightvol, void *buffer ){
	DebugMsg( "P" );
	return sceAudioOutputPanned_Real( channel, leftvol, rightvol, buffer );
}

int sceAudioOutputPannedBlocking_Hook( int channel, int leftvol, int rightvol, void *buffer ){
	DebugMsg( "p" );
	return sceAudioOutputPannedBlocking_Real( channel, leftvol, rightvol, buffer );
}


SCE_IO_OPEN sceIoOpen_Real;
int sceIoOpen_Hook( const char *file, int flags, SceMode mode ){
	
	int iRet = sceIoOpen_Real( file, flags, mode );
	
	DebugMsg( "snd:%s\n", file );
	
	return iRet;
}

/*** パッドフック関数 *******************************************************/

USHORT	g_uCtrlReadCnt		SEC_BSS_WORD;

#define READ_CNT_FINISH		0xFFF0
#define	READ_CNT_CURPOS		( READ_CNT_FINISH - 1 )
#define	READ_CNT_CONFIRM	( READ_CNT_CURPOS - 20 )
#define	READ_CNT_GPS_VALID	( READ_CNT_CONFIRM - 700 )

#define KEY_WAIT( n )	(( uCnt += ( n )) == g_uCtrlReadCnt )

SCE_CTRL_READ_BUFFER sceCtrlReadBufferPositive_Real;
int sceCtrlReadBufferPositive_Hook( SceCtrlData *pad_data, int count ){
	
	//DebugMsg( "@RP:%d", count );
	int iRet = sceCtrlReadBufferPositive_Real( pad_data, count );
	
	/******/
	
	// ボタンが押されたら，または GPS 捕捉開始したら，自動キー入力停止
	if( g_uCtrlReadCnt < READ_CNT_FINISH ){
		if( pad_data->Buttons & (
			PSP_CTRL_UP			|
			PSP_CTRL_DOWN		|
			PSP_CTRL_LEFT		|
			PSP_CTRL_RIGHT		|
			PSP_CTRL_CIRCLE		|
			PSP_CTRL_CROSS		|
			PSP_CTRL_TRIANGLE	|
			PSP_CTRL_SQUARE		|
			PSP_CTRL_SELECT		|
			PSP_CTRL_START		|
			PSP_CTRL_LTRIGGER	|
			PSP_CTRL_RTRIGGER
		)){
			g_uCtrlReadCnt = READ_CNT_FINISH;
		}
		
		/************************************************************************/
		
		DebugMsg( "@%d\n", g_uCtrlReadCnt );
		
		UINT uCnt = 0;
		if( g_cGPSValid && g_uCtrlReadCnt < READ_CNT_GPS_VALID )
			g_uCtrlReadCnt = READ_CNT_GPS_VALID;
		
		// オープニングのキー入力
		if     ( KEY_WAIT( 600 )) pad_data->Buttons = PSP_CTRL_START;	// スタート
		else if(
			KEY_WAIT( 100 ) ||	// 警告
			KEY_WAIT( 200 ) ||	// GPS 捕捉開始
			g_uCtrlReadCnt == READ_CNT_CONFIRM
		) pad_data->Buttons = PSP_CTRL_CIRCLE;	// GPS 捕捉完了
		
		else if( g_uCtrlReadCnt == READ_CNT_CURPOS )
			pad_data->Buttons = PSP_CTRL_CROSS;	// 現在位置に移動
		
		++g_uCtrlReadCnt;
	}
	return iRet;
}

/*** API フック用 ***********************************************************/

INLINE u32 NIDByName( const char *name ){
	u8 digest[20];
	u32 nid;
	
	if( sceKernelUtilsSha1Digest(( u8 * ) name, strlen( name ), digest ) >= 0 ){
		nid = digest[0] | ( digest[1] << 8 ) | ( digest[2] << 16 ) | ( digest[3] << 24 );
		DebugMsg( "NID:%08X:%s\n", nid, name );
		return nid;
	}
	
	return 0;
}

INLINE u32 FindNID( char modname[27], u32 nid ){
	struct SceLibraryEntryTable *entry;
	void *entTab;
	int entLen;
	
	SceModule *tmpmod, *pMod = NULL;
	SceUID ids[100];
	int count = 0;
	int p;
	
	memset( ids, 0, 100 * sizeof( SceUID ));
	
	sceKernelGetModuleIdList( ids, 100 * sizeof( SceUID ), &count );
	
	for( p = 0; p < count; p++ ){
		tmpmod = sceKernelFindModuleByUID( ids[p] );
		
		DebugMsg( "FindNID:modname:%s\n", tmpmod->modname );
		
		if( strcmp( tmpmod->modname, modname ) == 0 ){
			pMod = tmpmod;
		}
	}
	
	if( pMod != NULL ){
		int i = 0;
		
		entTab = pMod->ent_top;
		entLen = pMod->ent_size;
		while( i < entLen ){
			int count;
			int total;
			unsigned int *vars;
			
			entry = ( struct SceLibraryEntryTable * ) ( entTab + i );
			
			total = entry->stubcount + entry->vstubcount;
			vars = entry->entrytable;
			
			if( entry->stubcount > 0 ){
				for( count = 0; count < entry->stubcount; count++ ){
					if( vars[count] == nid ){
						DebugMsg( "FindNID:%X\n", vars[count+total] );
						return vars[count+total];
					}
				}
			}
			i += ( entry->len * 4 );
		}
	}
	
	DebugMsg( "FindNID:0\n" );
	return 0;
}

INLINE void *find_syscall_addr( u32 addr ){
	struct SyscallHeader *head;
	u32 *syscalls;
	void **ptr;
	int size;
	int i;
	
	asm( 
		"cfc0 %0, $12\n"
		: "=r"( ptr )
	 );
	
	if( !ptr ){
		DebugMsg( "find_syscall_addr:NULL\n" );
		return NULL;
	}
	
	head = ( struct SyscallHeader * ) *ptr;
	syscalls = ( u32* ) ( *ptr + 0x10 );
	size = ( head->size - 0x10 );
	
	for( i = 0; i < size; i++ ){
		if( syscalls[i] == addr ){
			DebugMsg( "find_syscall_addr:%X\n", ( u32 )&syscalls[i] );
			return &syscalls[i];
		}
	}
	
	DebugMsg( "find_syscall_addr:NULL\n" );
	return NULL;
}


INLINE void *apiHookAddr( u32 *addr, void *func ){
	if( !addr ){
		return NULL;
	}
	*addr = ( u32 ) func;
	sceKernelDcacheWritebackInvalidateRange( addr, sizeof( addr ));
	sceKernelIcacheInvalidateRange( addr, sizeof( addr ));
	
	DebugMsg( "apiHookAddr:%X\n", ( u32 )addr );
	return addr;
}

/*
INLINE u32 PatchNID( char modname[27], const char *funcname, void *func ){
	u32 nidaddr = FindNID( modname, NIDByName( funcname ));
	
	if( nidaddr > 0x80000000 ){
		if( !apiHookAddr( find_syscall_addr( nidaddr ), func )){
			nidaddr = 0;
		}
	}
	
	DebugMsg( "PatchNID:%X\n", nidaddr );
	return nidaddr;
}
*/

u32 PatchNID2( char modname[27], u32 uNID, void *func ){
	u32 nidaddr = FindNID( modname, uNID );
	
	if( nidaddr > 0x80000000 ){
		if( !apiHookAddr( find_syscall_addr( nidaddr ), func )){
			nidaddr = 0;
		}
	}
	
	DebugMsg( "PatchNID:%X\n", nidaddr );
	return nidaddr;
}

/*** メイン *****************************************************************/

#define NID_sceUsbGpsGetData			0x934EC2B2
#define NID_sceCtrlReadBufferPositive	0x1F803938

#define NID_sceIoOpen	0x109F50BC

#define HOOK_API( mod, func )	func ## _Real = ( void* )PatchNID2( mod, NID_ ## func, func ## _Hook )
#define UNHOOK_API( mod, func )	PatchNID2( mod, NID_ ## func , func ## _Real )

//Keep our module running
int main_thread( SceSize args, void *argp ) {
	
	// GPS モジュール ロード待ち
	while( !sceKernelFindModuleByName( "sceUSBGps_Driver" ))
		sceKernelDelayThread( MAIN_THREAD_DELAY );
	
	// GPS API フック開始
	DebugMsg( "USB GPS hook start\n" );
	
	HOOK_API( "sceUSBGps_Driver",		sceUsbGpsGetData );
	HOOK_API( "sceController_Service",	sceCtrlReadBufferPositive );
	
	HOOK_API( "sceAudio_Driver", sceAudioOutput );
	HOOK_API( "sceAudio_Driver", sceAudioOutputBlocking );
	HOOK_API( "sceAudio_Driver", sceAudioOutputPanned );
	HOOK_API( "sceAudio_Driver", sceAudioOutputPannedBlocking );
	HOOK_API( "sceIOFileManager", sceIoOpen );
	
	while( 1 ) sceKernelSleepThread();
	
	/*
	while( g_uCtrlReadCnt < READ_CNT_FINISH ){
		sceKernelDelayThread( MAIN_THREAD_DELAY );
	}
	
	// フック解除
	UNHOOK_API( "sceUSBGps_Driver",		sceUsbGpsGetData );
	UNHOOK_API( "sceController_Service",sceCtrlReadBufferPositive );
	
	DebugMsg( "exit thread\n" );
	*/
	return 0;
}

int module_start( SceSize args, void *argp ) __attribute__(( alias( "_start" )));
int _start( SceSize args, void *argp ){
	SceUID thid;
	
	DebugCmd(
		pspDebugSioInit();
		pspDebugSioSetBaud( 115200 );
		pspDebugSioInstallKprintf();
	)
	
	DebugMsg( "Loading maplug\n" );
	
	thid = sceKernelCreateThread( "GpsMain", main_thread, THREAD_PRIORITY, 0x1000, 0, NULL );
	if( thid >= 0 ) sceKernelStartThread( thid, args, argp );
	
	DebugMsg( "exit main\n" );
	return 0;
}
