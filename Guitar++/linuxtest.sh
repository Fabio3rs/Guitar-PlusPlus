cd gccout
for f in $(ls ../*.cpp); 
do 
  g++ -I ~/includes -g $f -c -w; 
done;

