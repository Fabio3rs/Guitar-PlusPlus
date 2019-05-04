cd gccout
for f in $(ls ../Lua/*.c); 
do 
  echo $f;
  gcc -O3 $f -c -w; 
done;

