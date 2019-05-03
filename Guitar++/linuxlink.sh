cd gccout
g++ -o GuitarPP *.o -lglfw3 -ldl -lX11 -lpthread -lGLEW -lGL -lbass -lbass_fx
cp GuitarPP ../../Release

