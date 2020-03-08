cd gccout
for f in $(ls ../*.cpp); 
do 
  echo Processing $f;
  g++ -mtune=native -march=native -O3 -I ~/includes $f -c -w; 
done;

