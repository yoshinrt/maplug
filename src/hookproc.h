/*****************************************************************************
$Id$

		hookproc.h -- hook procedure definition

*****************************************************************************/

HOOK_PROC( \
	int, sceCtrlReadBufferPositive, ( SceCtrlData *pad_data, int count ), \
	"sceController_Service", 0x1F803938 )

HOOK_PROC( \
	int, sceAudioOutputBlocking, ( int channel, int vol, void *buf ), \
	"sceAudio_Driver", 0x136CAF51 )

#undef HOOK_PROC
