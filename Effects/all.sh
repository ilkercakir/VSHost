#!/bin/bash
gcc -fPIC -c BiQuad.c $(pkg-config --cflags gtk+-3.0)
gcc -fPIC -c DelayS.c $(pkg-config --cflags gtk+-3.0)
gcc -fPIC -c VFO.c $(pkg-config --cflags gtk+-3.0)

gcc -fPIC -c Gain.c $(pkg-config --cflags gtk+-3.0)
gcc -fPIC -c Tremolo.c $(pkg-config --cflags gtk+-3.0)
gcc -fPIC -c Delay.c $(pkg-config --cflags gtk+-3.0)
gcc -fPIC -c Haas.c $(pkg-config --cflags gtk+-3.0)
gcc -fPIC -c Modulation.c $(pkg-config --cflags gtk+-3.0)
gcc -fPIC -c Equalizer.c $(pkg-config --cflags gtk+-3.0)

gcc -shared -fPIC -Wl,-soname,Gain.so -o Gain.so Gain.o -lc
gcc -shared -fPIC -Wl,-soname,Tremolo.so -o Tremolo.so Tremolo.o -lc
gcc -shared -fPIC -Wl,-soname,Delay.so -o Delay.so Delay.o DelayS.o -lc
gcc -shared -fPIC -Wl,-soname,Haas.so -o Haas.so Haas.o DelayS.o -lc
gcc -shared -fPIC -Wl,-soname,Modulation.so -o Modulation.so Modulation.o VFO.o -lc
gcc -shared -fPIC -Wl,-soname,Equalizer.so -o Equalizer.so Equalizer.o BiQuad.o -lc
