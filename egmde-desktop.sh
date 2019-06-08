#! /bin/bash
set -e

wayland_display=egmde_wayland
launcher=weston-terminal
bindir=$(dirname $0)

while [ $# -gt 0 ]
do
  if [ "$1" == "--help" -o "$1" == "-h" ]
  then
    echo "$(basename $0) - Handy launch script for a egmde \"desktop session\""
    echo "Usage: $(basename $0) [options] [shell options]"
    echo "Options are:"
    echo "    --startup <launcher>          use <launcher> instead of '${launcher}'"
    exit 0
  elif [ "$1" == "--startup" ];           then shift; launcher=$1
  else break
  fi
  shift
done

if [ "${bindir}" != "" ]; then bindir="${bindir}/"; fi

if [ -e "${XDG_RUNTIME_DIR}/${wayland_display}" ]; then echo "Error: wayland endpoint '${wayland_display}' already exists"; exit 1 ;fi

if ! which ${launcher} &>/dev/null
then
    echo "Error: Need ${launcher}"
    echo "On Ubuntu run \"sudo apt install weston\"";
    echo "On Fedora run \"sudo dnf install weston\"";
    exit 1
fi

exec $(realpath ${bindir}egmde) --wayland-socket-name ${wayland_display} --no-file --startup ${launcher}
