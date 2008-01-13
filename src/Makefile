# .tab=4
# $Id$

TARGET = maplug
OBJS = \
	maplug.o \

BUILD_PRX=1

#USE_PSPSDK_LIBC=1
#USE_PSPSDK_LIBS=1
USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

PRX_EXPORTS=exports.exp

INCDIR =
CFLAGS = -Os -G0 -Wall -fno-strict-aliasing -fno-builtin-printf
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

ifdef RELEASE
	CFLAGS += -DRELEASE
endif

LIBDIR =
LDFLAGS =
LIBS = 

ifdef DEBUG
	OBJS += sceHprm_driver.o debug.o
	CFLAGS += -DDEBUG
endif

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

VERSION=090
release:
	make allclean
	
	( \
		cd ..; \
		rm -rf maplug[0-9][0-9][0-9]*; \
		mkdir maplug$(VERSION); \
		cp -r src maplug$(VERSION); \
	)
	
	make RELEASE=1
	
	( \
		cd ..; \
		cp -r MS_ROOT maplug$(VERSION); \
		cp -r src/maplug*.prx maplug$(VERSION)/MS_ROOT/seplugins; \
	)
	
	( cd ../maplug$(VERSION); \
	rm -rf `find . -name CVS`; \
	lha a -d ../maplug$(VERSION).lzh * )

debug:
	make DEBUG=1

allclean:
	make DEBUG=1 clean
	-rm *.prx *.elf
