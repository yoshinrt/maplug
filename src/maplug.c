/****************************************************************************/
// $Id$

#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <pspctrl.h>
#include <string.h>
#include <stdio.h>

#include "dds.h"

PSP_MODULE_INFO( "ddsMaplug", 0x1000, 1, 1 );
PSP_MAIN_THREAD_ATTR( 0 );

/****************************************************************************/

#define THREAD_PRIORITY 		100
#define MAIN_THREAD_DELAY		100000	// 10Hz

/*** new type ***************************************************************/

struct SyscallHeader {
	void *unk;
	unsigned int basenum;
	unsigned int topnum;
	unsigned int size;
};

/*** hook proc assist *******************************************************/

#define HOOK_PROC( type, func, arg, mod, nid )	typedef type ( *func##_t ) arg;
#include "hookproc.h"

#define HOOK_PROC( type, func, arg, mod, nid )	func##_t func##_Real;
#include "hookproc.h"

/*** Audio hook proc. *******************************************************/

USHORT	g_uAudioOutputCnt	SEC_BSS_WORD;

int sceAudioOutputBlocking_Hook( int channel, int vol, void *buf ){
	int iRet = sceAudioOutputBlocking_Real( channel, vol, buf );
	
	++g_uAudioOutputCnt;
	//DebugMsg( "@%d\n", g_uAudioOutputCnt );
	return iRet;
}

/*** パッドフック関数 *******************************************************/

USHORT	g_uCtrlReadCnt		SEC_BSS_WORD;

#define	READ_CNT_GPS_START			0x2000
#define READ_CNT_FINISH				0xFFF0
#define	READ_CNT_GPS_COMPLETE		( READ_CNT_FINISH - 20 )

int sceCtrlReadBufferPositive_Hook( SceCtrlData *pad_data, int count ){
	
	//DebugMsg( "@RP:%d", count );
	int iRet = sceCtrlReadBufferPositive_Real( pad_data, count );
	
	/******/
	
	// ボタンが押されたら，または GPS 捕捉開始したら，自動キー入力停止
	if( g_uCtrlReadCnt < READ_CNT_FINISH ){
		if( pad_data->Buttons & PSP_CTRL_CIRCLE ){
			g_uCtrlReadCnt = READ_CNT_FINISH;
		}
		
		/************************************************************************/
		
		//DebugMsg( "@%d\n", g_uCtrlReadCnt );
		/*
		g_uAudioOutputCnt の値:
		346 op 再生終了
		376 警告画面の○を押したときの音が終了
		406 GPS捕捉中画面の音が終了
		436 ↑閉じたときの音が終了
		466 GPS捕捉完了の音が終了
		??? ↑閉じたときの音が終了
		*/
		
		if( g_uAudioOutputCnt <= 350 ){
			// オープニング・警告画面のキー入力
			if( g_uCtrlReadCnt & 1 ) pad_data->Buttons = PSP_CTRL_CIRCLE | PSP_CTRL_START;
		}else if( g_uCtrlReadCnt < READ_CNT_GPS_START ){
			if( g_uAudioOutputCnt == 406 ){
				// GPS 捕捉開始ダイアログ
				pad_data->Buttons = PSP_CTRL_CIRCLE;
				g_uCtrlReadCnt = READ_CNT_GPS_START;
			}
		}else if( g_uCtrlReadCnt < READ_CNT_GPS_COMPLETE ){
			if( g_uAudioOutputCnt == 466 ){
				// GPS 捕捉完了ダイアログがでた
				// 音が鳴るまで○連打
				if( g_uCtrlReadCnt & 1 ) pad_data->Buttons = PSP_CTRL_CIRCLE;
			}else if( g_uAudioOutputCnt > 466 ){
				g_uCtrlReadCnt = READ_CNT_GPS_COMPLETE;
			}
		}else{
			// 現在位置に移動
			if( g_uCtrlReadCnt & 1 ) pad_data->Buttons = PSP_CTRL_CROSS;
		}
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
		
		//DebugMsg( "FindNID:modname:%s\n", tmpmod->modname );
		
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

//Keep our module running
int main_thread( SceSize args, void *argp ) {
	
	// maplus ロード待ち
	while( !sceKernelFindModuleByName( "maplus" ))
		sceKernelDelayThread( MAIN_THREAD_DELAY );
	
	// GPS API フック開始
	DebugMsg( "API hook start\n" );
	
	#define HOOK_PROC( type, func, arg, mod, nid ) \
		func ## _Real = ( void* )PatchNID2( mod, nid, func ## _Hook );
	#include "hookproc.h"
	
	while( 1 ) sceKernelSleepThread();
	
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
