/* Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
/* MiSTer.ini video settings, explained: SS1 Tool's list (ss1_tool web/index.html, VRISK/VGROUPS/VOPT), from the
   SuperStation wiki and docs and MiSTer's documentation and source. Keep the two in step.
   The SS1's own video hardware (the Sony encoder needs vga_mode=subcarrier) and Taki's settings for
   MiSTer.ini and its four profiles, which Phosphor's installer sets once per card: docs/ss1-video.md
   and install/ss1_video_ini.sh. Read those before changing what a profile defaults to here. */
"use strict";
const VRISK={safe:["good","Safe"],blank:["warn","Can blank some screens"],hdmi:["muted","HDMI only"],crt:["muted","CRT / analog only"],never15:["bad","Never on a TV-style CRT"],audio:["warn","Can stop HDMI sound"]};
const VGROUPS=[["hdmi","HDMI"],["cec","HDMI-CEC (TV remote and power) - needs MiSTer 20260603 or newer"],["analog","Analog outputs (CRT, SCART, component, S-Video, composite, VGA)"],["other","Other"]];
const VOPT={
 video_mode:{n:"HDMI resolution",g:"hdmi",d:"Auto",r:"blank",v:[["","Auto (the TV's preferred mode)"],["0","1280x720 60 Hz (720p)"],["8","1920x1080 60 Hz (1080p)"],["7","1280x720 50 Hz (PAL)"],["9","1920x1080 50 Hz (PAL)"],["2","720x480 60 Hz (480p)"],["3","720x576 50 Hz (576p)"],["6","640x480 60 Hz (VGA)"],["5","800x600 60 Hz"],["1","1024x768 60 Hz"],["4","1280x1024 60 Hz"],["10","1366x768 60 Hz"],["11","1024x600 60 Hz"],["12","1920x1440 60 Hz"],["13","2048x1536 60 Hz"],["14","2560x1440 60 Hz"]],
  t:"The resolution sent over HDMI. When it isn't set, MiSTer asks the TV for its preferred mode, which through a switch, AV receiver or capture card can be one that doesn't show. 720p and 1080p are the safest fixed choices.",w:"Not every TV or monitor accepts every mode."},
 video_mode_ntsc:{n:"HDMI resolution for 60 Hz games",g:"hdmi",d:"same as HDMI resolution",r:"blank",v:[["","Not set (same as HDMI resolution)"],["0","1280x720 60 Hz"],["8","1920x1080 60 Hz"]],t:"Optional: a separate resolution while a 60 Hz (NTSC) game runs. Used together with the PAL one below."},
 video_mode_pal:{n:"HDMI resolution for 50 Hz games",g:"hdmi",d:"same as HDMI resolution",r:"blank",v:[["","Not set (same as HDMI resolution)"],["7","1280x720 50 Hz"],["9","1920x1080 50 Hz"]],t:"Optional: a separate resolution while a 50 Hz (PAL) game runs, so PAL games scroll smoothly."},
 vsync_adjust:{n:"Refresh rate and lag",g:"hdmi",d:"0",r:"blank",v:[["0","0 - Most compatible (fixed rate, slight judder)"],["1","1 - Match the game's refresh rate (smooth)"],["2","2 - Lowest lag (match refresh, no buffering)"]],
  t:"How the HDMI picture follows the game's refresh rate. 1 and 2 change the refresh rate per game for smoother scrolling and less lag.",w:"With 1 or 2 some TVs, and most capture cards, blank for a few seconds when a game starts, or don't show the picture at all."},
 vscale_mode:{n:"Scaling",g:"hdmi",d:"0",r:"safe",v:[["0","0 - Fill the screen height"],["1","1 - Whole-number (integer) scaling, sharpest"],["2","2 - Half steps"],["3","3 - Quarter steps"],["4","4 - Integer resolution, core aspect ratio (advanced)"],["5","5 - Integer resolution, display aspect ratio (advanced)"]],
  t:"How the game is scaled up. Filling the screen can make pixels uneven (shimmering when scrolling); integer scaling keeps every pixel the same size, with borders."},
 vscale_border:{n:"Border (picture cut off at the edges)",g:"hdmi",d:"0",r:"safe",v:[["","Not set (no border)"],["0","0 - No border"],["16","16"],["24","24"],["32","32"],["48","48"],["64","64"]],t:"Adds a border in pixels for TVs that cut off the top and bottom of the picture (overscan). Turning off overscan in the TV's picture settings (often called Just Scan, Screen Fit or 1:1) is better when it has one."},
 hdmi_limited:{n:"HDMI colour range",g:"hdmi",d:"0",r:"safe",v:[["0","0 - Full (0-255)"],["1","1 - Limited (16-235)"],["2","2 - 16-255"]],t:"Washed-out greys instead of black, or crushed dark areas, usually mean the TV expects the other range. Most monitors want full, many TVs want limited.",w:"2 can cause problems with some HDMI-to-analog adapters."},
 hdmi_game_mode:{n:"Tell the TV this is a game",g:"hdmi",d:"0",r:"safe",v:[["0","0 - Off"],["1","1 - On"]],t:"Marks the HDMI signal as game content, so TVs that support it switch to their low-latency game mode by themselves. TVs that don't support it ignore it."},
 hdr:{n:"HDR",g:"hdmi",d:"0",r:"blank",v:[["0","0 - Off"],["1","1 - HLG (recommended for most)"],["2","2 - DCI-P3"]],t:"Only for HDR TVs. MiSTer's docs suggest a lower colour saturation (80) with DCI-P3.",w:"On a TV without HDR the colours look wrong."},
 dvi_mode:{n:"DVI mode",g:"hdmi",d:"0",r:"audio",v:[["0","0 - HDMI (with sound)"],["1","1 - DVI (no sound)"]],t:"For DVI monitors and HDMI-to-DVI adapters. DVI has no sound, so leave it off on a TV."},
 vrr_mode:{n:"Variable refresh rate (VRR)",g:"hdmi",d:"0",r:"blank",v:[["0","0 - Off"],["1","1 - Automatic"],["2","2 - AMD FreeSync"],["3","3 - VESA (HDMI VRR)"],["4","4 - MiSTer VRR"]],t:"For TVs and monitors with VRR or FreeSync. Not used in the menu."},
 hdmi_audio_96k:{n:"HDMI sound sample rate",g:"hdmi",d:"0",r:"audio",v:[["0","0 - 48 kHz"],["1","1 - 96 kHz"]],t:"48 kHz works everywhere. Some TVs and receivers have no sound at 96 kHz."},
 direct_video:{n:"Direct Video (HDMI to analog adapter)",g:"hdmi",d:"0",r:"blank",v:[["0","0 - Off"],["1","1 - On"],["2","2 - Automatic (known adapters)"]],t:"Sends the game's own timing over HDMI for HDMI-to-VGA adapters (AG62xx) and scalers that accept it. MiSTer's docs say it only works with AG62xx adapters.",w:"On a normal TV or monitor there's no picture."},
 hdmi_cec:{n:"HDMI-CEC",g:"cec",d:"0",r:"safe",v:[["0","0 - Off"],["1","1 - On"]],t:"Lets the SuperStation and the TV talk over HDMI: the TV remote can work the menu, and the TV can be switched to the SuperStation and put to sleep. The other CEC settings below only work while this is on. The TV must support CEC too (often called Anynet+, Bravia Sync, SimpLink or similar). Games in cores built before May 2026 can't use it while they run; Update All brings newer cores."},
 hdmi_cec_input_mode:{n:"TV remote works the menu",g:"cec",d:"1",r:"safe",v:[["1","1 - On"],["0","0 - Off"]],t:"Passes the TV remote's buttons to the SuperStation."},
 hdmi_cec_power_on:{n:"Switch the TV to the SuperStation at power-on",g:"cec",d:"1",r:"safe",v:[["1","1 - On"],["0","0 - Off"]],t:"When the SuperStation starts, it asks the TV once to switch to its input, and wakes a TV in standby."},
 hdmi_cec_sleep:{n:"Put the TV to sleep when idle",g:"cec",d:"0",r:"safe",v:[["0","0 - Off"],["1","1 - On"]],t:"Sends the TV to standby when the HDMI output switches off after no input (the HDMI off timer below must be set)."},
 hdmi_cec_wake:{n:"Wake the TV on input",g:"cec",d:"0",r:"safe",v:[["0","0 - Off"],["1","1 - On"]],t:"After that idle period, pressing a button wakes the TV and switches it back to the SuperStation."},
 hdmi_cec_clock:{n:"CEC clock",g:"cec",d:"0 (detect)",r:"safe",v:[["0","0 - Detect automatically"],["12","12 MHz"],["50","50 MHz"]],t:"MiSTer measures the HDMI chip's CEC clock when it starts (12 or 50 MHz), which is how the same MiSTer works on both the DE10-Nano and the SuperStation. Leave it on detect unless CEC doesn't respond."},
 hdmi_off:{n:"HDMI off after no input (minutes)",g:"cec",d:"0",r:"safe",v:[["0","0 - Never"],["10","10"],["15","15"],["30","30"],["60","60"],["120","120"]],t:"Switches the HDMI output off after this many minutes without controller, keyboard or mouse input, in every core. Works without CEC; with CEC sleep and wake on, it also puts the TV to sleep."},
 video_off:{n:"Black screen in the menu after (seconds)",g:"cec",d:"0",r:"safe",v:[["0","0 - Never"],["60","60"],["300","300"],["600","600"]],t:"Shows a black picture in the menu after this many seconds without use (only while the menu's on-screen display times out)."},
 video_off_logo:{n:"Moving logo on the black screen",g:"cec",d:"0",r:"safe",v:[["0","0 - Off"],["1","1 - On"]],t:"Shows a moving MiSTer logo on that black screen, so it's clear the SuperStation is still on."},
 vga_mode:{n:"Analog output",g:"analog",d:"rgb",r:"crt",v:[["rgb","rgb - RGB (SCART, VGA, PVM)"],["ypbpr","ypbpr - Component"],["svideo","svideo - S-Video and composite, FPGA encoder"],["cvbs","cvbs - Composite, FPGA encoder"],["subcarrier","subcarrier - RGB plus S-Video/composite from the Sony encoder (SS1 default)"]],
  t:"What the analog outputs carry. subcarrier is the SuperStation's default: RGB on VGA, the mini-DIN and the RCA jacks at once, plus S-Video and composite from the Sony encoder (switch 3 down). The FPGA encoder modes need switch 3 up."},
 composite_sync:{n:"Sync type",g:"analog",d:"1",r:"crt",v:[["1","1 - Composite sync (SCART, PVM, component)"],["0","0 - Separate H and V sync (VGA monitors, RGBHV)"]],t:"SCART, PVMs and component want composite sync; VGA-style monitors want separate sync."},
 vga_sog:{n:"Sync on green (ini)",g:"analog",d:"0",r:"crt",v:[["0","0 - Off"],["1","1 - On"]],t:"On the SuperStation sync on green is added with switch 4 (down = add). The SuperStation's own profiles leave this at 0.",w:"Sources differ on this one: MiSTer's docs describe it as forcing sync on green, the SuperStation uses its switch."},
 forced_scandoubler:{n:"31 kHz VGA output",g:"analog",d:"0",r:"never15",v:[["0","0 - Off (15 kHz, TVs)"],["1","1 - On (PC monitors)"]],t:"Doubles the line rate for VGA PC monitors.",w:"Never use it with a TV-style (15 kHz) CRT: the higher rate can stress the TV."},
 vga_scaler:{n:"VGA uses the HDMI picture",g:"analog",d:"0",r:"never15",v:[["0","0 - Off"],["1","1 - On"]],t:"Sends the scaled HDMI picture to the VGA output, for PC monitors and other 31 kHz screens.",w:"Not for 240p TVs; never on a TV-style (15 kHz) CRT."},
 menu_pal:{n:"Menu in PAL",g:"analog",d:"0",r:"crt",v:[["0","0 - NTSC"],["1","1 - PAL"]],t:"Runs the menu in PAL, for PAL TVs.",pal:true},
 ntsc_mode:{n:"NTSC colour system",g:"analog",d:"0",r:"crt",v:[["0","0 - NTSC"],["1","1 - PAL-60"],["2","2 - PAL-M"]],t:"S-Video and composite only. MiSTer's docs note that no cores officially use these yet."},
 video_info:{n:"Show resolution after a change",g:"other",d:"0",r:"safe",v:[["0","0 - Off"],["2","2 seconds"],["5","5 seconds"],["10","10 seconds"]],t:"Shows the resolution and refresh rate in the corner for a few seconds when they change. Handy while trying settings."},
 fb_terminal:{n:"Linux screen for scripts",g:"other",d:"1",r:"safe",v:[["1","1 - On"],["0","0 - Off"]],t:"If scripts misbehave on a CRT or analog display, MiSTer's docs suggest turning this off."}
};

/* which DIP switches on the side of the SS1 a set of analog settings needs (SS1 Tool's setup wizard;
   the switch table on the SuperStation wiki): [[switch, position], ...] */
const DIP_NOTE_UNITS = "SuperStation One units made after December 2025.";
function dipFor(v, profileLabel) {
	const mode = v.vga_mode || "rgb", csync = v.composite_sync !== "0", pal = v.menu_pal === "1";
	const reg = pal ? "Up (PAL)" : "Down (NTSC)";
	const lab = (profileLabel || "").toLowerCase();
	if (mode === "subcarrier") return [["3", "Down (Sony encoder)"], ["1", reg], ["2", reg], ["4", "Up (off)"]];
	if (mode === "svideo" || mode === "cvbs") return [["3", "Up (FPGA encoder)"], ["1", reg], ["2", reg], ["4", "Up (off)"]];
	if (mode === "ypbpr") return [["4", "Down (sync on Y)"]];
	if (mode === "rgb" && !csync) return [["4", "Up (off)"]];
	if (lab === "rgsb") return [["4", "Down (sync on green)"]];
	return [["4", "Up for SCART and RGB with separate sync; Down for sync on green"]];
}
const ANALOG_KEYS = new Set(["vga_mode", "composite_sync", "vga_sog", "menu_pal"]);
