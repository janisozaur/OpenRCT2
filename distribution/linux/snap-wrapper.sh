#!/bin/sh
set -x

env

echo ======================================
echo
echo
echo
echo ======================================
echo
echo
echo
echo ======================================
echo
echo
echo
echo ======================================

WITH_RUNTIME=no
if [ -z "$RUNTIME" ]; then
  RUNTIME=$SNAP
else
  # add general paths not added by snapcraft due to runtime snap
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$RUNTIME/lib/$ARCH:$RUNTIME/usr/lib/$ARCH
  export PATH=$PATH:$RUNTIME/usr/bin
  WITH_RUNTIME=yes
fi

# XKB config
export XKB_CONFIG_ROOT=$RUNTIME/usr/share/X11/xkb

# Give XOpenIM a chance to locate locale data.
# This is required for text input to work in SDL2 games.
export XLOCALEDIR=$RUNTIME/usr/share/X11/locale

# Mesa Libs for OpenGL support
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$RUNTIME/usr/lib/$ARCH/mesa
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$RUNTIME/usr/lib/$ARCH/mesa-egl

# Tell libGL where to find the drivers
export LIBGL_DRIVERS_PATH=$SNAP/usr/lib/x86_64-linux-gnu/dri
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LIBGL_DRIVERS_PATH

# Workaround in snapd for proprietary nVidia drivers mounts the drivers in
# /var/lib/snapd/lib/gl that needs to be in LD_LIBRARY_PATH
# Without that OpenGL using apps do not work with the nVidia drivers.
# Ref.: https://bugs.launchpad.net/snappy/+bug/1588192
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/var/lib/snapd/lib/gl
export LIBGL_DEBUG=verbose
glxinfo
echo "$@"
exec "$@"
