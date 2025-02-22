/*  This file is part of Saturn.emu.

	Saturn.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Saturn.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Saturn.emu.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "main"
#include <emuframework/EmuSystemInlines.hh>
#include <emuframework/EmuAppInlines.hh>
#include <imagine/fs/FS.hh>
#include <imagine/util/format.hh>
#include <imagine/util/string.h>

extern "C"
{
	#include <yabause/yabause.h>
	#include <yabause/m68kcore.h>
	#include <yabause/peripheral.h>
	#include <yabause/sh2core.h>
	#include <yabause/sh2int.h>
	#include <yabause/vidsoft.h>
	#include <yabause/scsp.h>
	#include <yabause/cdbase.h>
	#include <yabause/cs0.h>
	#include <yabause/cs2.h>
}

// from sh2_dynarec.c
#define SH2CORE_DYNAREC 2
extern const int defaultSH2CoreID =
#ifdef SH2_DYNAREC
SH2CORE_DYNAREC;
#else
SH2CORE_INTERPRETER;
#endif

PerInterface_struct *PERCoreList[] =
{
	&PERDummy,
	nullptr
};

CDInterface *CDCoreList[] =
{
	&DummyCD,
	&ISOCD,
	nullptr
};

#define SNDCORE_IMAGINE 1

// EmuFramework is in charge of audio setup & parameters
static int SNDImagineInit() { logMsg("called sound core init"); return 0; }
static void SNDImagineDeInit() {}
static int SNDImagineReset() { return 0; }
static void SNDImagineMuteAudio() {}
static void SNDImagineUnMuteAudio() {}
static void SNDImagineSetVolume(int volume) {}
static int SNDImagineChangeVideoFormat(int vertfreq) { return 0; }

static void mergeSamplesToStereo(s32 srcL, s32 srcR, s16 *dst)
{
	// Left Channel
	if (srcL > 0x7FFF) *dst = 0x7FFF;
	else if (srcL < -0x8000) *dst = -0x8000;
	else *dst = srcL;
	dst++;
	// Right Channel
	if (srcR > 0x7FFF) *dst = 0x7FFF;
	else if (srcR < -0x8000) *dst = -0x8000;
	else *dst = srcR;
}

static void SNDImagineUpdateAudioNull(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 frames) {}

static void SNDImagineUpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 frames);

static u32 SNDImagineGetAudioSpace()
{
	return 1024; // always render all samples available
}

static SoundInterface_struct SNDImagine
{
	SNDCORE_IMAGINE,
	"Imagine Engine Sound Interface",
	SNDImagineInit,
	SNDImagineDeInit,
	SNDImagineReset,
	SNDImagineChangeVideoFormat,
	SNDImagineUpdateAudio,
	SNDImagineGetAudioSpace,
	SNDImagineMuteAudio,
	SNDImagineUnMuteAudio,
	SNDImagineSetVolume
};

SoundInterface_struct *SNDCoreList[] =
{
	//&SNDDummy,
	&SNDImagine,
	nullptr
};

VideoInterface_struct *VIDCoreList[] =
{
	//&VIDDummy,
	&VIDSoft,
	nullptr
};

#if !defined HAVE_Q68 && !defined HAVE_C68K
#warning No 68K cores compiled in
#endif

M68K_struct *M68KCoreList[] =
{
	//&M68KDummy,
	#ifdef HAVE_C68K
	&M68KC68K,
	#endif
	#ifdef HAVE_Q68
	&M68KQ68,
	#endif
	nullptr
};

static void SNDImagineUpdateAudioNull(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 frames);
static void SNDImagineUpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 frames);

namespace EmuEx
{

const char *EmuSystem::creditsViewStr = CREDITS_INFO_STRING "(c) 2012-2023\nRobert Broglia\nwww.explusalpha.com\n\nPortions (c) the\nYabause Team\nyabause.org";
bool EmuSystem::handlesGenericIO = false;
bool EmuSystem::hasRectangularPixels = true;
static EmuSystemTaskContext emuSysTask{};
static EmuAudio *emuAudio{};
static EmuVideo *emuVideo{};
PerPad_struct *pad[2];

SaturnApp::SaturnApp(ApplicationInitParams initParams, ApplicationContext &ctx):
	EmuApp{initParams, ctx}, saturnSystem{ctx} {}

static bool hasCDExtension(std::string_view name)
{
	return IG::endsWithAnyCaseless(name, ".cue", ".iso", ".bin");
}

bool hasBIOSExtension(std::string_view name)
{
	return IG::endsWithAnyCaseless(name, ".bin");
}

static FS::PathString bupPath{};
static char mpegPath[] = "";
static char cartPath[] = "";

FS::PathString biosPath{};

yabauseinit_struct yinit
{
	PERCORE_DUMMY,
	defaultSH2CoreID,
	VIDCORE_SOFT,
	SNDCORE_IMAGINE,
	#if defined(HAVE_Q68)
	M68KCORE_Q68,
	#else
	M68KCORE_C68K,
	#endif
	CDCORE_ISO,
	CART_NONE,
	REGION_AUTODETECT,
	biosPath.data(),
	{},
	bupPath.data(),
	mpegPath,
	cartPath,
	nullptr,
	VIDEOFORMATTYPE_NTSC,
	0,
	0,
	0,
	0,
	0
};

const char *EmuSystem::shortSystemName() const
{
	return "SS";
}

const char *EmuSystem::systemName() const
{
	return "Sega Saturn";
}

EmuSystem::NameFilterFunc EmuSystem::defaultFsFilter = hasCDExtension;

static constexpr auto pixFmt = IG::PIXEL_FMT_RGBA8888;

CLINK void YuiSwapBuffers()
{
	//logMsg("YuiSwapBuffers");
	if(emuVideo) [[likely]]
	{
		int height, width;
		VIDCore->GetGlSize(&width, &height);
		IG::PixmapView srcPix{{{width, height}, pixFmt}, dispbuffer};
		emuVideo->startFrameWithAltFormat(emuSysTask, srcPix);
		emuVideo = {};
		emuSysTask = {};
	}
	else
	{
		//logMsg("skipping render");
	}
}

CLINK void YuiSetVideoAttribute(int type, int val) { }
CLINK int YuiSetVideoMode(int width, int height, int bpp, int fullscreen) { return 0; }

CLINK void YuiErrorMsg(const char *string)
{
	logMsg("%s", string);
}

CLINK int OSDUseBuffer()
{
	return 0;
}

CLINK int OSDChangeCore(int coreid)
{
	return 0;
}

void SaturnSystem::reset(EmuApp &, ResetMode mode)
{
	logMsg("resetting system");
	assert(hasContent());
	//Cs2ChangeCDCore(CDCORE_DUMMY, yinit.cdpath);
	YabauseReset();
}

FS::FileString SaturnSystem::stateFilename(int slot, std::string_view name) const
{
	return IG::format<FS::FileString>("{}.0{}.yss", name, saveSlotCharUpper(slot));
}

size_t SaturnSystem::stateSize() { return 0; }
void SaturnSystem::readState(EmuApp &app, std::span<uint8_t> buff) {}
size_t SaturnSystem::writeState(std::span<uint8_t> buff, SaveStateFlags flags) { return 0; }

void SaturnSystem::onFlushBackupMemory(EmuApp &, BackupMemoryDirtyFlags)
{
	if(hasContent())
	{
		logMsg("saving backup memory");
		T123Save(BupRam, 0x10000, 1, bupPath.data());
	}
}

static bool yabauseIsInit = 0;

void SaturnSystem::closeSystem()
{
	if(yabauseIsInit)
	{
		YabauseDeInit();
		yabauseIsInit = 0;
	}
}

void SaturnSystem::loadContent(IO &, EmuSystemCreateParams, OnLoadProgressDelegate)
{
	bupPath = contentSavePath("bkram.bin");
	if(YabauseInit(&yinit) != 0)
	{
		logErr("YabauseInit failed");
		throw std::runtime_error("Error loading game");
	}
	logMsg("YabauseInit done");
	yabauseIsInit = 1;

	PerPortReset();
	pad[0] = PerPadAdd(&PORTDATA1);
	pad[1] = PerPadAdd(&PORTDATA2);
	ScspSetFrameAccurate(1);
}

void SaturnSystem::configAudioRate(FrameTime outputFrameTime, int outputRate)
{
	// TODO
}

void SaturnSystem::runFrame(EmuSystemTaskContext taskCtx, EmuVideo *video, EmuAudio *audio)
{
	emuSysTask = taskCtx;
	emuVideo = video;
	emuAudio = audio;
	SNDImagine.UpdateAudio = audio ? SNDImagineUpdateAudio : SNDImagineUpdateAudioNull;
	YabauseEmulate();
	emuAudio = {};
}

void EmuApp::onCustomizeNavView(EmuApp::NavView &view)
{
	const Gfx::LGradientStopDesc navViewGrad[] =
	{
		{ .0, Gfx::PackedColor::format.build(.8 * .4, 0., 0., 1.) },
		{ .3, Gfx::PackedColor::format.build(.8 * .4, 0., 0., 1.) },
		{ .97, Gfx::PackedColor::format.build(.2 * .4, 0., 0., 1.) },
		{ 1., view.separatorColor() },
	};
	view.setBackgroundGradient(navViewGrad);
}

}

static void SNDImagineUpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 frames)
{
	//logMsg("got %d audio frames to write", frames);
	s16 sample[frames*2];
	for(auto i : IG::iotaCount(frames))
	{
		mergeSamplesToStereo(leftchanbuffer[i], rightchanbuffer[i], &sample[i*2]);
	}
	if(EmuEx::emuAudio)
	{
		EmuEx::emuAudio->writeFrames(sample, frames);
	}
}

CLINK void DisplayMessage(const char* str) {}
CLINK int OSDInit(int coreid) { return 0; }
CLINK void OSDPushMessage(int msgtype, int ttl, const char * message, ...) {}
CLINK void OSDDisplayMessages(void) {}
