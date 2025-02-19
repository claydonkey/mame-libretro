#!/bin/bash
# 
# File:   mame_compile_armhf.sh.bash
# Author: anthony Campbell
#
# Created on 17 May 2022, 09:44:46
#

#!/bin/bash
# force qt5
export QT_SELECT=5
# set paths
export MXTARCH=armv8-rpi4-linux-gnueabihf
export MXTOOLS=${HOME}/raspberry_pi/${MXTARCH}
export MARCHDIR=${HOME}/armhf
export RPIARCH=arm-linux-gnueabihf
#cd ~/src/mame
#make clean
make \
 CROSS_BUILD=1 \
 NOWERROR=1 \
 PLATFORM=arm \
 CFLAGS+="-I ${MARCHDIR}/usr/include" \
 CFLAGS+="-I ${MARCHDIR}/usr/include/${RPIARCH}" \
 CFLAGS+="-L ${MARCHDIR}/usr/lib" \
 CFLAGS+="-L ${MARCHDIR}/usr/lib/${RPIARCH}" \
 CPPFLAGS+="-I ${MARCHDIR}/usr/include" \
 CPPFLAGS+="-I ${MARCHDIR}/usr/include/${RPIARCH}" \
 CPPFLAGS+="-L ${MARCHDIR}/usr/lib" \
 CPPFLAGS+="-L ${MARCHDIR}/usr/lib/${RPIARCH}" \
 LDFLAGS+="-L ${MARCHDIR}/usr/lib" \
 LDFLAGS+="-L ${MARCHDIR}/usr/lib/${RPIARCH}" \
 ARCHOPTS+="-Wl,-R,${MARCHDIR}/usr/lib" \
 ARCHOPTS+="-Wl,-R,${MARCHDIR}/usr/lib/${RPIARCH}" \
 ARCHOPTS+="-Wl,-R,${MARCHDIR}/lib" \
 ARCHOPTS+="-Wl,-R,${MARCHDIR}/lib/${RPIARCH}" \
 ARCHOPTS+="-Wl,-R,${MARCHDIR}/usr/lib/${RPIARCH}/pulseaudio" \
 ARCHOPTS+="-Wl,-R,${MARCHDIR}/opt/vc/lib" \
 ARCHOPTS+="-Wl,-rpath,${MARCHDIR}/usr/lib" \
 ARCHOPTS+="-Wl,-rpath,${MARCHDIR}/usr/lib/${RPIARCH}" \
 ARCHOPTS+="-Wl,-rpath,${MARCHDIR}/lib" \
 ARCHOPTS+="-Wl,-rpath,${MARCHDIR}/lib/${RPIARCH}" \
 ARCHOPTS+="-Wl,-rpath,${MARCHDIR}/usr/lib/${RPIARCH}/pulseaudio" \
 ARCHOPTS+="-Wl,-rpath,${MARCHDIR}/opt/vc/lib" \
 ARCHOPTS+="-Wl,-rpath-link,${MARCHDIR}/usr/lib" \
 ARCHOPTS+="-Wl,-rpath-link,${MARCHDIR}/usr/lib/${RPIARCH}" \
 ARCHOPTS+="-Wl,-rpath-link,${MARCHDIR}/lib" \
 ARCHOPTS+="-Wl,-rpath-link,${MARCHDIR}/lib/${RPIARCH}" \
 ARCHOPTS+="-Wl,-rpath-link,${MARCHDIR}/usr/lib/${RPIARCH}/pulseaudio" \
 ARCHOPTS+="-Wl,-rpath-link,${MARCHDIR}/opt/vc/lib" \
 TARGETOS=linux \
 NOASM=1 \
 OVERRIDE_CC="${MXTOOLS}/bin/${MXTARCH}-gcc" \
 OVERRIDE_LD="${MXTOOLS}/bin/${MXTARCH}-ld" \
 OVERRIDE_CXX="${MXTOOLS}/bin/${MXTARCH}-c++" \
 OSD="retro" \
 CONFIG="libretro" \
 TARGET="mame" -j14 
