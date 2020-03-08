cd gccout
for f in $(ls ../Lua/*.c); 
do 
  echo $f;
  gcc -mtune=native -march=native -O3 $f -c -w; 
done;

