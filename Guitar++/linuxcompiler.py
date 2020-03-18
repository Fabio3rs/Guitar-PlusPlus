from multiprocessing import Process
import os, subprocess

def f(name):
    print(name.find('.cpp'), name)
    outp = name.replace('.cpp', '.o')
    print(outp)
    #-mtune=native -march=native -O3 -I ~/includes $f -c -w
    command = ['g++-9', '-g', '-mtune=native', '-march=native', '-O1', '-I' + os.environ['HOME'] + '/includes', name, '-c', '-w', '-o' + os.getcwd() + '/gccout/' + outp]
    print(command)

    subprocess.call(command)

if __name__ == '__main__':
    processes = []
    for fichier in os.listdir("."):
        if fichier.find('.cpp') != -1:
            processes.insert(0, [fichier, Process(target=f, args=(fichier,))])
            processes[0][1].start()
    
    for p in processes:
        print('Waiting for', p[0], 'to finish')
        p[1].join()
    