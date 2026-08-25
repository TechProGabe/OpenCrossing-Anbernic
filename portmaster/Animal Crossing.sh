#!/bin/bash
# PortMaster launcher — Animal Crossing (GameCube decomp port)
# Target: Anbernic RG-34XX SP and other armhf PortMaster devices.

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/ac-gc"
CONFDIR="$GAMEDIR/conf"
mkdir -p "$CONFDIR" "$GAMEDIR/rom"

cd "$GAMEDIR"
: > "$GAMEDIR/log.txt"
# Plain redirect: the previous tee process-substitution silently dropped
# stdout on the device shell, truncating log.txt at early init.
exec >> "$GAMEDIR/log.txt" 2>&1

# First-run settings tuned for these handhelds (Mali-G31): fullscreen,
# no MSAA, vsync on, dynamic FPS target. Resolution is intentionally NOT
# set here: the game auto-detects the panel's native mode at startup
# (RG35XX 640x480, RG-34XX SP 720x480, CubeXX 720x720). Add
# window_width/window_height under [Graphics] only to force a resolution.
# The in-game settings menu can change all of these afterwards.
if [ ! -f "$GAMEDIR/settings.ini" ]; then
cat > "$GAMEDIR/settings.ini" <<'EOF'
[Graphics]
fullscreen = 1
vsync = 1
msaa = 0
[Performance]
fps_target = 6
particle_quality = 2
EOF
fi

export XDG_DATA_HOME="$CONFDIR"
export LD_LIBRARY_PATH="/usr/lib32:$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"

# Clear PipeWire vars for 32-bit binary compatibility
unset SPA_PLUGIN_DIR
unset PIPEWIRE_MODULE_DIR

# Set soundcard 0 (audiocodec) explicitly for ALSA
export AUDIODEV=hw:0,0
export ALSA_CARD=audiocodec

# Ensure standard library fallback paths are present in LD_LIBRARY_PATH
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:/usr/lib32:/usr/lib:$LD_LIBRARY_PATH"

# Set a SINGLE valid SDL audio driver (ALSA handles the H700 audio codec directly)
export SDL_AUDIODRIVER=alsa

# Audio diagnostics for log.txt while we chase the silence bug
echo "--- audio diag ---"
ls /usr/lib32 2>/dev/null | grep -iE "spa|pipewire|asound|pulse|SDL2"
ls /usr/lib32/spa-0.2/support 2>/dev/null
cat /proc/asound/cards 2>/dev/null
echo "--- end diag ---"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
chmod +x "$GAMEDIR/AnimalCrossing"

$GPTOKEYB "AnimalCrossing" &
pm_platform_helper "$GAMEDIR/AnimalCrossing"
./AnimalCrossing

pm_finish
