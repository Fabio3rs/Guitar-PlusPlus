cd gccout
for f in $(ls ../*.cpp); 
do 
  echo Processing $f;
  g++ -I ~/includes -g $f -c -w; 
done;

