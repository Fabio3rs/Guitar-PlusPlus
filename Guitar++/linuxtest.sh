cd gccout
for f in $(ls ../*.cpp); 
do 
  echo Processing $f;
  g++ -O3 -I ~/includes $f -c -w; 
done;

