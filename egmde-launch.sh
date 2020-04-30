#! /bin/sh
set -e

bindir=$(dirname $0)
if [ "${bindir}" != "" ]; then bindir="${bindir}/"; fi

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

if [ ! -d "${XDG_RUNTIME_DIR}" ]
then
  echo "Error: XDG_RUNTIME_DIR '${XDG_RUNTIME_DIR}' does not exists"
  exit 1
fi

if [ ! -z "${WAYLAND_DISPLAY}" ] && [ -e "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}" ]
then
  echo "Error: wayland endpoint '${WAYLAND_DISPLAY}' already exists"
  exit 1
fi

keymap_index=$(gsettings get org.gnome.desktop.input-sources current | cut -d\  -f 2)
keymap=$(gsettings get org.gnome.desktop.input-sources sources | grep -Po "'[[:alpha:]]+'\)" | sed -ne "s/['|)]//g;$(($keymap_index+1))p")

export MIR_SERVER_KEYMAP=${keymap}
export MIR_SERVER_ENABLE_X11=1
exec ${bindir}egmde "$@"