/* MiSTer's video settings, explained: made by tools/gen_video_opts.js from apps/windows/web/video-options.js.
 * Don't edit: change video-options.js and run it again. */
#ifndef PHOS_VIDEO_OPTS_H
#define PHOS_VIDEO_OPTS_H

typedef struct { const char *v, *label; } vopt_val_t;
typedef struct {
	const char *key, *name, *group, *def, *risk, *text, *warn;
	int nv;
	const vopt_val_t *vals;
} vopt_t;

static const vopt_val_t VOPT_V0[] = { { "", "Auto (the TV's preferred mode)" }, { "0", "1280x720 60 Hz (720p)" }, { "8", "1920x1080 60 Hz (1080p)" }, { "7", "1280x720 50 Hz (PAL)" }, { "9", "1920x1080 50 Hz (PAL)" }, { "2", "720x480 60 Hz (480p)" }, { "3", "720x576 50 Hz (576p)" }, { "6", "640x480 60 Hz (VGA)" }, { "5", "800x600 60 Hz" }, { "1", "1024x768 60 Hz" }, { "4", "1280x1024 60 Hz" }, { "10", "1366x768 60 Hz" }, { "11", "1024x600 60 Hz" }, { "12", "1920x1440 60 Hz" }, { "13", "2048x1536 60 Hz" }, { "14", "2560x1440 60 Hz" } };
static const vopt_val_t VOPT_V1[] = { { "", "Not set (same as HDMI resolution)" }, { "0", "1280x720 60 Hz" }, { "8", "1920x1080 60 Hz" } };
static const vopt_val_t VOPT_V2[] = { { "", "Not set (same as HDMI resolution)" }, { "7", "1280x720 50 Hz" }, { "9", "1920x1080 50 Hz" } };
static const vopt_val_t VOPT_V3[] = { { "0", "0 - Most compatible (fixed rate, slight judder)" }, { "1", "1 - Match the game's refresh rate (smooth)" }, { "2", "2 - Lowest lag (match refresh, no buffering)" } };
static const vopt_val_t VOPT_V4[] = { { "0", "0 - Fill the screen height" }, { "1", "1 - Whole-number (integer) scaling, sharpest" }, { "2", "2 - Half steps" }, { "3", "3 - Quarter steps" }, { "4", "4 - Integer resolution, core aspect ratio (advanced)" }, { "5", "5 - Integer resolution, display aspect ratio (advanced)" } };
static const vopt_val_t VOPT_V5[] = { { "", "Not set (no border)" }, { "0", "0 - No border" }, { "16", "16" }, { "24", "24" }, { "32", "32" }, { "48", "48" }, { "64", "64" } };
static const vopt_val_t VOPT_V6[] = { { "0", "0 - Full (0-255)" }, { "1", "1 - Limited (16-235)" }, { "2", "2 - 16-255" } };
static const vopt_val_t VOPT_V7[] = { { "0", "0 - Off" }, { "1", "1 - On" } };
static const vopt_val_t VOPT_V8[] = { { "0", "0 - Off" }, { "1", "1 - HLG (recommended for most)" }, { "2", "2 - DCI-P3" } };
static const vopt_val_t VOPT_V9[] = { { "0", "0 - HDMI (with sound)" }, { "1", "1 - DVI (no sound)" } };
static const vopt_val_t VOPT_V10[] = { { "0", "0 - Off" }, { "1", "1 - Automatic" }, { "2", "2 - AMD FreeSync" }, { "3", "3 - VESA (HDMI VRR)" }, { "4", "4 - MiSTer VRR" } };
static const vopt_val_t VOPT_V11[] = { { "0", "0 - 48 kHz" }, { "1", "1 - 96 kHz" } };
static const vopt_val_t VOPT_V12[] = { { "0", "0 - Off" }, { "1", "1 - On" }, { "2", "2 - Automatic (known adapters)" } };
static const vopt_val_t VOPT_V13[] = { { "0", "0 - Off" }, { "1", "1 - On" } };
static const vopt_val_t VOPT_V14[] = { { "1", "1 - On" }, { "0", "0 - Off" } };
static const vopt_val_t VOPT_V15[] = { { "1", "1 - On" }, { "0", "0 - Off" } };
static const vopt_val_t VOPT_V16[] = { { "0", "0 - Off" }, { "1", "1 - On" } };
static const vopt_val_t VOPT_V17[] = { { "0", "0 - Off" }, { "1", "1 - On" } };
static const vopt_val_t VOPT_V18[] = { { "0", "0 - Detect automatically" }, { "12", "12 MHz" }, { "50", "50 MHz" } };
static const vopt_val_t VOPT_V19[] = { { "0", "0 - Never" }, { "10", "10" }, { "15", "15" }, { "30", "30" }, { "60", "60" }, { "120", "120" } };
static const vopt_val_t VOPT_V20[] = { { "0", "0 - Never" }, { "60", "60" }, { "300", "300" }, { "600", "600" } };
static const vopt_val_t VOPT_V21[] = { { "0", "0 - Off" }, { "1", "1 - On" } };
static const vopt_val_t VOPT_V22[] = { { "rgb", "rgb - RGB (SCART, VGA, PVM)" }, { "ypbpr", "ypbpr - Component" }, { "svideo", "svideo - S-Video and composite, FPGA encoder" }, { "cvbs", "cvbs - Composite, FPGA encoder" }, { "subcarrier", "subcarrier - RGB plus S-Video/composite from the Sony encoder (SS1 default)" } };
static const vopt_val_t VOPT_V23[] = { { "1", "1 - Composite sync (SCART, PVM, component)" }, { "0", "0 - Separate H and V sync (VGA monitors, RGBHV)" } };
static const vopt_val_t VOPT_V24[] = { { "0", "0 - Off" }, { "1", "1 - On" } };
static const vopt_val_t VOPT_V25[] = { { "0", "0 - Off (15 kHz, TVs)" }, { "1", "1 - On (PC monitors)" } };
static const vopt_val_t VOPT_V26[] = { { "0", "0 - Off" }, { "1", "1 - On" } };
static const vopt_val_t VOPT_V27[] = { { "0", "0 - NTSC" }, { "1", "1 - PAL" } };
static const vopt_val_t VOPT_V28[] = { { "0", "0 - NTSC" }, { "1", "1 - PAL-60" }, { "2", "2 - PAL-M" } };
static const vopt_val_t VOPT_V29[] = { { "0", "0 - Off" }, { "2", "2 seconds" }, { "5", "5 seconds" }, { "10", "10 seconds" } };
static const vopt_val_t VOPT_V30[] = { { "1", "1 - On" }, { "0", "0 - Off" } };

static const vopt_t VOPTS[] = {
	{ "video_mode", "HDMI resolution", "hdmi", "Auto", "blank", "The resolution sent over HDMI. When it isn't set, MiSTer asks the TV for its preferred mode, which through a switch, AV receiver or capture card can be one that doesn't show. 720p and 1080p are the safest fixed choices.", "Not every TV or monitor accepts every mode.", 16, VOPT_V0 },
	{ "video_mode_ntsc", "HDMI resolution for 60 Hz games", "hdmi", "same as HDMI resolution", "blank", "Optional: a separate resolution while a 60 Hz (NTSC) game runs. Used together with the PAL one below.", NULL, 3, VOPT_V1 },
	{ "video_mode_pal", "HDMI resolution for 50 Hz games", "hdmi", "same as HDMI resolution", "blank", "Optional: a separate resolution while a 50 Hz (PAL) game runs, so PAL games scroll smoothly.", NULL, 3, VOPT_V2 },
	{ "vsync_adjust", "Refresh rate and lag", "hdmi", "0", "blank", "How the HDMI picture follows the game's refresh rate. 1 and 2 change the refresh rate per game for smoother scrolling and less lag.", "With 1 or 2 some TVs, and most capture cards, blank for a few seconds when a game starts, or don't show the picture at all.", 3, VOPT_V3 },
	{ "vscale_mode", "Scaling", "hdmi", "0", "safe", "How the game is scaled up. Filling the screen can make pixels uneven (shimmering when scrolling); integer scaling keeps every pixel the same size, with borders.", NULL, 6, VOPT_V4 },
	{ "vscale_border", "Border (picture cut off at the edges)", "hdmi", "0", "safe", "Adds a border in pixels for TVs that cut off the top and bottom of the picture (overscan). Turning off overscan in the TV's picture settings (often called Just Scan, Screen Fit or 1:1) is better when it has one.", NULL, 7, VOPT_V5 },
	{ "hdmi_limited", "HDMI colour range", "hdmi", "0", "safe", "Washed-out greys instead of black, or crushed dark areas, usually mean the TV expects the other range. Most monitors want full, many TVs want limited.", "2 can cause problems with some HDMI-to-analog adapters.", 3, VOPT_V6 },
	{ "hdmi_game_mode", "Tell the TV this is a game", "hdmi", "0", "safe", "Marks the HDMI signal as game content, so TVs that support it switch to their low-latency game mode by themselves. TVs that don't support it ignore it.", NULL, 2, VOPT_V7 },
	{ "hdr", "HDR", "hdmi", "0", "blank", "Only for HDR TVs. MiSTer's docs suggest a lower colour saturation (80) with DCI-P3.", "On a TV without HDR the colours look wrong.", 3, VOPT_V8 },
	{ "dvi_mode", "DVI mode", "hdmi", "0", "audio", "For DVI monitors and HDMI-to-DVI adapters. DVI has no sound, so leave it off on a TV.", NULL, 2, VOPT_V9 },
	{ "vrr_mode", "Variable refresh rate (VRR)", "hdmi", "0", "blank", "For TVs and monitors with VRR or FreeSync. Not used in the menu.", NULL, 5, VOPT_V10 },
	{ "hdmi_audio_96k", "HDMI sound sample rate", "hdmi", "0", "audio", "48 kHz works everywhere. Some TVs and receivers have no sound at 96 kHz.", NULL, 2, VOPT_V11 },
	{ "direct_video", "Direct Video (HDMI to analog adapter)", "hdmi", "0", "blank", "Sends the game's own timing over HDMI for HDMI-to-VGA adapters (AG62xx) and scalers that accept it. MiSTer's docs say it only works with AG62xx adapters.", "On a normal TV or monitor there's no picture.", 3, VOPT_V12 },
	{ "hdmi_cec", "HDMI-CEC", "cec", "0", "safe", "Lets the SuperStation and the TV talk over HDMI: the TV remote can work the menu, and the TV can be switched to the SuperStation and put to sleep. The other CEC settings below only work while this is on. The TV must support CEC too (often called Anynet+, Bravia Sync, SimpLink or similar). Games in cores built before May 2026 can't use it while they run; Update All brings newer cores.", NULL, 2, VOPT_V13 },
	{ "hdmi_cec_input_mode", "TV remote works the menu", "cec", "1", "safe", "Passes the TV remote's buttons to the SuperStation.", NULL, 2, VOPT_V14 },
	{ "hdmi_cec_power_on", "Switch the TV to the SuperStation at power-on", "cec", "1", "safe", "When the SuperStation starts, it asks the TV once to switch to its input, and wakes a TV in standby.", NULL, 2, VOPT_V15 },
	{ "hdmi_cec_sleep", "Put the TV to sleep when idle", "cec", "0", "safe", "Sends the TV to standby when the HDMI output switches off after no input (the HDMI off timer below must be set).", NULL, 2, VOPT_V16 },
	{ "hdmi_cec_wake", "Wake the TV on input", "cec", "0", "safe", "After that idle period, pressing a button wakes the TV and switches it back to the SuperStation.", NULL, 2, VOPT_V17 },
	{ "hdmi_cec_clock", "CEC clock", "cec", "0 (detect)", "safe", "MiSTer measures the HDMI chip's CEC clock when it starts (12 or 50 MHz), which is how the same MiSTer works on both the DE10-Nano and the SuperStation. Leave it on detect unless CEC doesn't respond.", NULL, 3, VOPT_V18 },
	{ "hdmi_off", "HDMI off after no input (minutes)", "cec", "0", "safe", "Switches the HDMI output off after this many minutes without controller, keyboard or mouse input, in every core. Works without CEC; with CEC sleep and wake on, it also puts the TV to sleep.", NULL, 6, VOPT_V19 },
	{ "video_off", "Black screen in the menu after (seconds)", "cec", "0", "safe", "Shows a black picture in the menu after this many seconds without use (only while the menu's on-screen display times out).", NULL, 4, VOPT_V20 },
	{ "video_off_logo", "Moving logo on the black screen", "cec", "0", "safe", "Shows a moving MiSTer logo on that black screen, so it's clear the SuperStation is still on.", NULL, 2, VOPT_V21 },
	{ "vga_mode", "Analog output", "analog", "rgb", "crt", "What the analog outputs carry. subcarrier is the SuperStation's default: RGB on VGA, the mini-DIN and the RCA jacks at once, plus S-Video and composite from the Sony encoder (switch 3 down). The FPGA encoder modes need switch 3 up.", NULL, 5, VOPT_V22 },
	{ "composite_sync", "Sync type", "analog", "1", "crt", "SCART, PVMs and component want composite sync; VGA-style monitors want separate sync.", NULL, 2, VOPT_V23 },
	{ "vga_sog", "Sync on green (ini)", "analog", "0", "crt", "On the SuperStation sync on green is added with switch 4 (down = add). The SuperStation's own profiles leave this at 0.", "Sources differ on this one: MiSTer's docs describe it as forcing sync on green, the SuperStation uses its switch.", 2, VOPT_V24 },
	{ "forced_scandoubler", "31 kHz VGA output", "analog", "0", "never15", "Doubles the line rate for VGA PC monitors.", "Never use it with a TV-style (15 kHz) CRT: the higher rate can stress the TV.", 2, VOPT_V25 },
	{ "vga_scaler", "VGA uses the HDMI picture", "analog", "0", "never15", "Sends the scaled HDMI picture to the VGA output, for PC monitors and other 31 kHz screens.", "Not for 240p TVs; never on a TV-style (15 kHz) CRT.", 2, VOPT_V26 },
	{ "menu_pal", "Menu in PAL", "analog", "0", "crt", "Runs the menu in PAL, for PAL TVs.", NULL, 2, VOPT_V27 },
	{ "ntsc_mode", "NTSC colour system", "analog", "0", "crt", "S-Video and composite only. MiSTer's docs note that no cores officially use these yet.", NULL, 3, VOPT_V28 },
	{ "video_info", "Show resolution after a change", "other", "0", "safe", "Shows the resolution and refresh rate in the corner for a few seconds when they change. Handy while trying settings.", NULL, 4, VOPT_V29 },
	{ "fb_terminal", "Linux screen for scripts", "other", "1", "safe", "If scripts misbehave on a CRT or analog display, MiSTer's docs suggest turning this off.", NULL, 2, VOPT_V30 },
};
#define NVOPTS ((int)(sizeof(VOPTS) / sizeof(VOPTS[0])))

/* the groups, in order: id, heading */
static const char *const VGROUPS_[][2] = {
	{ "hdmi", "HDMI" },
	{ "cec", "HDMI-CEC" },
	{ "analog", "Analog outputs" },
	{ "other", "Other" },
};
#define NVGROUPS ((int)(sizeof(VGROUPS_) / sizeof(VGROUPS_[0])))

/* risk id: words for it, 1 when it's a warning */
static const struct { const char *id, *words; int warn; } VRISKS[] = {
	{ "safe", "Safe", 0 },
	{ "blank", "Can blank some screens", 1 },
	{ "hdmi", "HDMI only", 1 },
	{ "crt", "CRT / analog only", 1 },
	{ "never15", "Never on a TV-style CRT", 1 },
	{ "audio", "Can stop HDMI sound", 1 },
};
#define NVRISKS ((int)(sizeof(VRISKS) / sizeof(VRISKS[0])))

#endif
