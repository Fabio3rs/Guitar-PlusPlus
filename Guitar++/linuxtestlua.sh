cd gccout
for f in $(ls ../Lua/*.c); 
do 
  echo $f;
  gcc -g $f -c -w; 
done;

