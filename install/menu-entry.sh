#!/bin/bash
# phosphor: menu entry (put here by Phosphor's installer; uninstall.sh removes it)
# Starts Phosphor from MiSTer's menu: System Settings > Scripts > phosphor.
P=/media/fat/phosphor/phosphor.sh
if [ ! -x "$P" ]; then echo "Phosphor isn't installed (no $P)."; exit 1; fi
"$P" start
if [ -S /tmp/phosphor-main.sock ]; then
	echo "Phosphor takes the screen when this closes."
else
	echo "With this MiSTer Main, press F9 on a keyboard to see Phosphor"
	echo "(Phosphor's own Main switches to it by itself)."
fi
