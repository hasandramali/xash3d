#!/bin/bash

# Build custom SDL2

cd $TRAVIS_BUILD_DIR/SDL2-2.0.20
export CC="ccache gcc -msse2 -march=i686 -m32 -ggdb -O2"
./configure \
	--disable-dependency-tracking \
	--enable-audio \
	--enable-video \
	--enable-events \
	--disable-render \
	--enable-joystick \
	--disable-haptic \
	--disable-power \
	--enable-threads \
	--enable-timers \
	--enable-loadso \
	--enable-video-opengl \
	--enable-x11-shared \
	--enable-video-x11 \
	--enable-video-x11-xrandr \
	--enable-video-x11-scrnsaver \
	--enable-video-x11-xinput \
	--disable-video-x11-xshape \
	--disable-video-x11-xdbe \
	--disable-libudev \
	--disable-dbus \
	--disable-ibus \
	--enable-sdl-dlopen \
	--disable-video-opengles \
	--disable-cpuinfo \
	--disable-assembly \
	--disable-atomic \
	--enable-alsa
make -j2
mkdir -p $TRAVIS_BUILD_DIR/sdl2-linux
make install DESTDIR=$TRAVIS_BUILD_DIR/sdl2-linux

# Build engine

mkdir -p build && cd build/
export CFLAGS="-m32"
export CXXFLAGS="-m32"
cmake \
	-DXASH_DEDICATED=ON \
	-DXASH_VGUI=ON \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo ../
make
