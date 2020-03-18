cd gccout
g++-9 -ggdb -o GuitarPP *.o -lglfw3 -ldl -lX11 -lpthread -lGLEW -lGL -l bass -l bass_fx
cp GuitarPP ../../Release

