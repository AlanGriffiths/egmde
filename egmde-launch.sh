#! /bin/sh
set -e

bindir=$(dirname "$0")
if [ "${bindir}" != "" ]; then bindir="${bindir}/"; fi

if [ ! -d "${XDG_RUNTIME_DIR}" ]
then
  echo "Error: XDG_RUNTIME_DIR '${XDG_RUNTIME_DIR}' does not exists"
  exit 1
fi

if [ -n "${WAYLAND_DISPLAY}" ] && [ -O "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}" ]
then
  echo "Info: wayland endpoint '${WAYLAND_DISPLAY}' already exists, using it as host"
  export MIR_SERVER_WAYLAND_HOST=${WAYLAND_DISPLAY}
  unset WAYLAND_DISPLAY
fi

keymap_index=$(gsettings get org.gnome.desktop.input-sources current | cut -d\  -f 2)
keymap=$(gsettings get org.gnome.desktop.input-sources sources | grep -Po "'[[:alpha:]]+'\)" | sed -ne "s/['|)]//g;$((keymap_index+1))p")

export MIR_SERVER_KEYMAP=${keymap}
export MIR_SERVER_ENABLE_X11=1
exec "${bindir}"egmde "$@"