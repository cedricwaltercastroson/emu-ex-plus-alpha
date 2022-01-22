#ifndef SYSTEM_H
#define SYSTEM_H

#include "common/Types.h"

#define winlog log

#include <zlib.h>

class SoundDriver;

namespace EmuEx
{
class EmuVideo;
class EmuAudio;
class EmuSystemTaskContext;
}

struct EmulatedSystem {
  // main emulation function
	void (*emuMain)(int);
  // reset emulator
  void (*emuReset)();
  // clean up memory
  void (*emuCleanUp)();
  // load battery file
  bool (*emuReadBattery)(const char *);
  // write battery file
  bool (*emuWriteBattery)(const char *);
#ifdef __LIBRETRO__
  // load state
  bool (*emuReadState)(const uint8_t *, unsigned);
  // load state
  unsigned (*emuWriteState)(uint8_t *, unsigned);
#else
  // load state
  bool (*emuReadState)(const char *);
  // save state
  bool (*emuWriteState)(const char *);
#endif
  // load memory state (rewind)
  bool (*emuReadMemState)(char *, int);
  // write memory state (rewind)
  bool (*emuWriteMemState)(char *, int, long &);
  // write PNG file
  bool (*emuWritePNG)(const char *);
  // write BMP file
  bool (*emuWriteBMP)(const char *);
  // emulator update CPSR (ARM only)
  void (*emuUpdateCPSR)();
  // emulator has debugger
  bool emuHasDebugger;
  // clock ticks to emulate
  int emuCount;
};

extern void log(const char *, ...);

extern bool systemPauseOnFrame();
extern void systemGbPrint(uint8_t *, int, int, int, int, int);
extern void systemScreenCapture(int);
extern void systemDrawScreen(EmuEx::EmuSystemTaskContext, EmuEx::EmuVideo &);
// updates the joystick data
extern bool systemReadJoypads();
// return information about the given joystick, -1 for default joystick
extern uint32_t systemReadJoypad(int);
extern uint32_t systemGetClock();
#ifndef NDEBUG
extern void systemMessage(int, const char *, ...);
#else
#define systemMessage(i, s, ...) ({ })
#endif
extern void systemSetTitle(const char *);
extern SoundDriver *systemSoundInit();
extern void systemOnWriteDataToSoundBuffer(EmuEx::EmuAudio *audio, const uint16_t * finalWave, int length);
extern void systemOnSoundShutdown();
extern void systemScreenMessage(const char *);
extern void systemUpdateMotionSensor();
extern int  systemGetSensorX();
extern int  systemGetSensorY();
extern int systemGetSensorZ();
extern uint8_t systemGetSensorDarkness();
extern void systemCartridgeRumble(bool);
extern void systemPossibleCartridgeRumble(bool);
extern void updateRumbleFrame();
extern bool systemCanChangeSoundQuality();
extern void systemShowSpeed(int);
extern void system10Frames(int);
extern void systemFrame();
extern void systemGbBorderOn();

extern void Sm60FPS_Init();
extern bool Sm60FPS_CanSkipFrame();
extern void Sm60FPS_Sleep();
extern void DbgMsg(const char *msg, ...);

extern void (*dbgOutput)(const char *s, uint32_t addr);
extern void (*dbgSignal)(int sig, int number);

union SystemColorMap
{
	uint32_t map32[0x10000];
	uint16_t map16[0x10000];
};
extern SystemColorMap systemColorMap;
extern uint16_t systemGbPalette[24];
extern int systemFrameSkip;
extern int systemDebug;
static const int systemVerbose = 0;
extern int systemSaveUpdateCounter;
extern int systemSpeed;
#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0
#endif // SYSTEM_H
