#include "common.hpp"
#include "download.hpp"
#include "dumpdsp.h"
#include "inifile.h"
#include "init.hpp"
#include "sound.h"
#include "thread.h"

#include <3ds.h>
#include <dirent.h>
#include <unistd.h>

#define CONFIG_3D_SLIDERSTATE (*(float *)0x1FF81080)

static bool dspfirmfound = false;
static bool musicPlaying = false;

// Music and sound effects.
static sound *mus_settings = NULL;

C2D_SpriteSheet sprites;

static inline void Play_Music(void)
{
	if(dspfirmfound && !musicPlaying)
	{
		mus_settings->play();
		musicPlaying = true;
	}
}

Result Init::Initialize() {
	romfsInit();
	Result res = cfguInit();
	u8 consoleModel;
	if (R_SUCCEEDED(res)) {
		CFGU_GetSystemModel(&consoleModel);
		cfguExit();
	}
	gfxInitDefault();
	gfxSetWide(consoleModel != 3);
	Gui::init();
	amInit();
	acInit();
	Gui::loadSheet("romfs:/gfx/sprites.t3x", sprites);
	fadein = true;
	fadealpha = 255;
	osSetSpeedupEnable(true);	// Enable speed-up for New 3DS users
	hidSetRepeatParameters(25, 5);

	dspfirmfound = access("sdmc:/3ds/dspfirm.cdc", F_OK) == 0;
	// Load the sound effects if DSP is available.
	if(dspfirmfound)
	{
		ndspInit();
		mus_settings = new sound("romfs:/sounds/settings.wav", 1, true);
	}

	return 0;
}

Result Init::MainLoop() {
	// Initialize everything.
	Initialize();

	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(Top, BLACK);
	C2D_TargetClear(TopRight, BLACK);
	C2D_TargetClear(Bottom, BLACK);
	Play_Music();

	downloadBoxart();

	// Exit all services and exit the app.
	Exit();
	return 0;
}

Result Init::Exit()
{
	if(dspfirmfound)
	{
		if(musicPlaying)
			mus_settings->stop ();

		ndspExit();
		delete mus_settings;
	}

	Gui::exit();
	Gui::unloadSheet(sprites);

	romfsExit();
	amExit();
	acExit();
	return 0;
}
