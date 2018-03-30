#! /bin/bash

socket=${XDG_RUNTIME_DIR}/egmde_socket
wayland_display=egmde_wayland
launcher=weston-terminal
bindir=$(dirname $0)
vt=4

while [ $# -gt 0 ]
do
  if [ "$1" == "--help" -o "$1" == "-h" ]
  then
    echo "$(basename $0) - Handy launch script for a egmde \"desktop session\""
    echo "Usage: $(basename $0) [options] [shell options]"
    echo "Options are:"
    echo "    --startup <launcher>          use <launcher> instead of '${launcher}'"
    echo "    --vt <termid>                 set the virtual terminal [${vt}]"
    exit 0
  elif [ "$1" == "--startup" ];           then shift; launcher=$1
  elif [ "$1" == "--vt" ];                then shift; vt=$1
  else break
  fi
  shift
done

if [ "${bindir}" != "" ]; then bindir="${bindir}/"; fi

if [ -e "${socket}" ]; then echo "Error: session endpoint '${socket}' already exists"; exit 1 ;fi
if [ -e "${XDG_RUNTIME_DIR}/${wayland_display}" ]; then echo "Error: wayland endpoint '${wayland_display}' already exists"; exit 1 ;fi

vt_login_session=$(who -u | grep tty${vt} | grep ${USER} | wc -l)
if [ "${vt_login_session}" == "0" ]; then echo "Error: please log into tty${vt} first"; exit 1 ;fi

oldvt=$(sudo fgconsole)
sudo --background --preserve-env sh -c "${bindir}egmde --wayland-socket-name ${wayland_display} --vt ${vt} --arw-file --file ${socket} $*; chvt ${oldvt}"
Make background a selectable gradient
while [ ! -e "${socket}" ]; do echo "waiting for ${socket}"; sleep 1 ;done

unset QT_QPA_PLATFORMTHEME
MIR_SOCKET=${socket} XDG_SESSION_TYPE=mir GDK_BACKEND=wayland QT_QPA_PLATFORM=wayland SDL_VIDEODRIVER=wayland WAYLAND_DISPLAY=${wayland_display} NO_AT_BRIDGE=1 dbus-run-session -- ${launcher}
sudo killall ${bindir}egmde
