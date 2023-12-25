/*  Copyright 2006 Guillaume Duhamel
    Copyright 2006 Fabien Coulon
    Copyright 2005 Joost Peters

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <assert.h>
#include <unistd.h>
#include <sys/time.h>

#include "platform.h"

#include "yabause.h"
#include "gameinfo.h"
#include "yui.h"
#include "peripheral.h"
#include "sh2core.h"
#include "sh2int.h"
#ifdef HAVE_LIBGL
#include "ygl.h"
#endif
#include "vidcs.h"
#include "cs0.h"
#include "cs2.h"
#include "cdbase.h"
#include "scsp.h"
#include "sndsdl.h"
#include "sndal.h"
#include "persdljoy.h"
#ifdef ARCH_IS_LINUX
#include "perlinuxjoy.h"
#endif
#include "debug.h"
#include "m68kcore.h"
#include "vdp1.h"
#include "vdp2.h"
#include "cdbase.h"
#include "peripheral.h"
#include "sh2int_kronos.h"

#define AR (4.0f/3.0f)
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT ((int)((float)WINDOW_WIDTH/AR))

#define  WINDOW_WIDTH_LOW 600
#define WINDOW_HEIGHT_LOW ((int)((float)WINDOW_WIDTH_LOW/AR))

static int Wwidth;
static int Wheight;

M68K_struct * M68KCoreList[] = {
&M68KDummy,
&M68KMusashi,
NULL
};

SH2Interface_struct *SH2CoreList[] = {
&SH2Interpreter,
&SH2DebugInterpreter,
&SH2KronosInterpreter,
NULL
};

PerInterface_struct *PERCoreList[] = {
&PERDummy,
#ifdef ARCH_IS_LINUX
&PERLinuxJoy,
#endif
NULL
};

CDInterface *CDCoreList[] = {
&DummyCD,
&ISOCD,
#ifndef UNKNOWN_ARCH
&ArchCD,
#endif
NULL
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
#ifdef HAVE_LIBSDL
&SNDSDL,
#endif
#ifdef HAVE_LIBAL
&SNDAL,
#endif
NULL
};

VideoInterface_struct *VIDCoreList[] = {
&VIDSoft,
NULL
};

#ifdef YAB_PORT_OSD
#include "nanovg_osdcore.h"
OSD_struct *OSDCoreList[] = {
&OSDNnovg,
NULL
};
#endif

static int fullscreen = 0;
static int scanline = 0;
static int lowres_mode = 0;

static char biospath[256] = "\0";
static char cdpath[256] = "\0";
static char stvgamepath[256] = "\0";
static char stvbiospath[256] = "\0";

yabauseinit_struct yinit;

char* toLower(char* s) {
  for(char *p=s; *p; p++) *p=tolower(*p);
  return s;
}

void YuiMsg(const char *format, ...) {
  va_list arglist;
  va_start( arglist, format );
  vprintf( format, arglist );
  va_end( arglist );
}

void YuiErrorMsg(const char *error_text)
{
   YuiMsg("\n\nError: %s\n", error_text);
}

void YuiEndOfFrame()
{

}

int YuiRevokeOGLOnThisThread(){
  platform_YuiRevokeOGLOnThisThread();
  return 0;
}

int YuiUseOGLOnThisThread(){
  platform_YuiUseOGLOnThisThread();
  return 0;
}

void YuiSwapBuffers(void) {

   platform_swapBuffers();
   SetOSDToggle(1);
}

int YuiGetFB(void) {
  return 0;
}

void YuiInit() {
	yinit.m68kcoretype = M68KCORE_MUSASHI;
	yinit.percoretype = PERCORE_LINUXJOY;
        #if defined(DYNAREC_KRONOS)
          printf("Use kronos specific core emulation\n");
          yinit.sh2coretype = 8;
        #else
	  yinit.sh2coretype = 0;
        #endif
	yinit.vidcoretype = VIDCORE_CS;
#ifdef HAVE_LIBSDL
	yinit.sndcoretype = SNDCORE_SDL;
#else
	yinit.sndcoretype = 0;
#endif
	yinit.cdcoretype = CDCORE_DEFAULT;
	yinit.carttype = CART_DRAM32MBIT;
	yinit.regionid = REGION_EUROPE;
	yinit.languageid = 0;
	yinit.biospath = NULL;
	yinit.cdpath = NULL;
	yinit.buppath = "./bup.ram";
        yinit.extend_backup = 1;
	yinit.mpegpath = NULL;
	yinit.cartpath = "./backup32Mb.ram";
	yinit.osdcoretype = OSDCORE_DEFAULT;
	yinit.skip_load = 0;

	yinit.usethreads = 1;
	yinit.numthreads = 4;
        yinit.usecache = 0;
}

static int SetupOpenGL() {
  int w = (lowres_mode == 0)?WINDOW_WIDTH:WINDOW_WIDTH_LOW;
  int h = (lowres_mode == 0)?WINDOW_HEIGHT:WINDOW_HEIGHT_LOW;
  Wwidth = w;
  Wheight = h;
  if (!platform_SetupOpenGL(w,h,fullscreen))
    exit(EXIT_FAILURE);
}

void displayGameInfo(char *filename) {
  GameInfo info;
  if (! GameInfoFromPath(filename, &info))
  {
    return;
  }

  printf("Game Info:\n\tSystem: %s\n\tCompany: %s\n\tItemNum:%s\n\tVersion:%s\n\tDate:%s\n\tCDInfo:%s\n\tRegion:%s\n\tPeripheral:%s\n\tGamename:%s\n", info.system, info.company, info.itemnum, info.version, info.date, info.cdinfo, info.region, info.peripheral, info.gamename);
}

void initEmulation() {
   YuiInit();
   SetupOpenGL();
   if (YabauseSh2Init(&yinit) != 0) {
    printf("YabauseSh2Init error \n\r");
    return;
  }
}
#ifndef TEST_MODE
int main(int argc, char *argv[]) {
	int i;

	YuiInit();

        yinit.stvbiospath = NULL;
        yinit.stvgamepath = NULL;
        yinit.vsyncon = 1;
//handle command line arguments
  for (i = 1; i < argc; ++i) {
    if (argv[i]) {
      //show usage
      if (0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "-?") || 0 == strcmp(argv[i], "--help")) {
        print_usage(argv[0]);
        return 0;
      }

      //set bios
      if (0 == strcmp(argv[i], "-b") && argv[i + 1]) {
        strncpy(biospath, argv[i + 1], 256);
        yinit.biospath = biospath;
      } else if (strstr(argv[i], "--bios=")) {
        strncpy(biospath, argv[i] + strlen("--bios="), 256);
        yinit.biospath = biospath;
      }
      //set iso
      if (0 == strcmp(argv[i], "-i") && argv[i + 1]) {
        strncpy(cdpath, argv[i + 1], 256);
        yinit.cdcoretype = 1;
        yinit.cdpath = cdpath;
        displayGameInfo(cdpath);
      } else if (strstr(argv[i], "--iso=")) {
        strncpy(cdpath, argv[i] + strlen("--iso="), 256);
        yinit.cdcoretype = 1;
        yinit.cdpath = cdpath;
      }
      //set cdrom
      else if (0 == strcmp(argv[i], "-c") && argv[i + 1]) {
        strncpy(cdpath, argv[i + 1], 256);
        yinit.cdcoretype = 2;
        yinit.cdpath = cdpath;
      } else if (0 == strcmp(argv[i], "-c") && argv[i + 1]) {
        strncpy(cdpath, argv[i + 1], 256);
        yinit.cdcoretype = 2;
        yinit.cdpath = cdpath;
      } else if (strstr(argv[i], "--cache")) {
        yinit.usecache = 1;
      }
      // Set sound
      else if (strcmp(argv[i], "-ns") == 0 || strcmp(argv[i], "--nosound") == 0) {
        yinit.sndcoretype = 0;
      }
      else if (strstr(argv[i], "--stvbios=")) {
        strncpy(biospath, argv[i] + strlen("--stvbios="), 256);
        yinit.stvbiospath = biospath;
        yinit.extend_backup = 0;
        yinit.buppath = "./bupstv.ram";
      }
      else if (strstr(argv[i], "--stvgame=")) {
        strncpy(stvgamepath, argv[i] + strlen("--stvgame="), 256);
        yinit.carttype = CART_ROMSTV;
        yinit.stvgamepath = stvgamepath;
        yinit.extend_backup = 0;
        yinit.buppath = "./bupstv.ram";
      }
      // Set sound
      else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fullscreen") == 0) {
        fullscreen = 1;
      }
      else if (strcmp(argv[i], "-sl") == 0 || strcmp(argv[i], "--scanline") == 0) {
        scanline = 1;
      }
      // Low resolution mode
      else if (strcmp(argv[i], "-lr") == 0 || strcmp(argv[i], "--lowres") == 0) {
        lowres_mode = 1;
      }
      else if (strcmp(argv[i], "-ci") == 0 ) {
        yinit.sh2coretype = 1;
      }
      else if (strcmp(argv[i], "-cd") == 0 ) {
      #if defined(DYNAREC_DEVMIYAX)
          printf("Use new dynarec core emulation\n");
          yinit.sh2coretype = 3;
      #else
        #if defined(DYNAREC_KRONOS)
          printf("Use kronos specific core emulation\n");
          yinit.sh2coretype = 8;
        #else
          printf("No dynarec core emulation: fallback on SW core emulation\n");
          yinit.sh2coretype = 0;
        #endif
      #endif
      }

      // Auto frame skip
      else if (strstr(argv[i], "--vsyncoff")) {
        yinit.vsyncon = 0;
      }
    }
  }
  SetupOpenGL();

	YabauseDeInit();

  if (YabauseInit(&yinit) != 0) {
    printf("YabauseInit error \n\r");
    return 1;
  }

  if (lowres_mode == 0){
    VIDCore->Resize(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 1);
  } else {
    VIDCore->Resize(0, 0, WINDOW_WIDTH_LOW, WINDOW_HEIGHT_LOW, 1);
  }

  platform_SetKeyCallback(PERCore->onKeyEvent);

  while (!platform_shouldClose())
  {
        int height;
        int width;
        platform_getFBSize(&width, &height);
        if ((Wwidth != width) || (Wheight != height)) {
          Wwidth = width;
          Wheight = height;
          VIDCore->Resize(0, 0, Wwidth, Wheight, 1);
        }
        if (YabauseExec() == -1) platform_Close();
        platform_HandleEvent();
  }

	YabauseDeInit();
  platform_Deinit();

	return 0;
}
#endif
