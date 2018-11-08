#! /bin/bash
set -e

socket=${XDG_RUNTIME_DIR}/egmde_socket
wayland_display=egmde_wayland
bindir=$(dirname $0)

while [ $# -gt 0 ]
do
  if [ "$1" == "--help" -o "$1" == "-h" ]
  then
    echo "$(basename $0) - Handy launch script for egmde on the desktop"
    echo "Usage: $(basename $0) [shell options]"
    exit 0
  else break
  fi
  shift
done

if [ "${bindir}" != "" ]; then bindir="${bindir}/"; fi

if [ -e "${socket}" ]; then echo "Error: session endpoint '${socket}' already exists"; exit 1 ;fi
if [ -e "${XDG_RUNTIME_DIR}/${wayland_display}" ]; then echo "Error: wayland endpoint '${wayland_display}' already exists"; exit 1 ;fi
if [ ! -d "${XDG_RUNTIME_DIR}" ]; then echo "Error: XDG_RUNTIME_DIR '${XDG_RUNTIME_DIR}' does not exists"; exit 1 ;fi

keymap_index=$(gsettings get org.gnome.desktop.input-sources current | cut -d\  -f 2)
keymap=$(gsettings get org.gnome.desktop.input-sources sources | grep -Po "'[[:alpha:]]+'\)" | sed -ne "s/['|)]//g;$(($keymap_index+1))p")

export MIR_SERVER_WAYLAND_SOCKET_NAME=${wayland_display}
export MIR_SERVER_FILE=${socket}
export MIR_SERVER_KEYMAP=${keymap}
exec ${bindir}egmde "$@"