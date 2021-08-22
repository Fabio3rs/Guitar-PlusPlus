# Guitar-PlusPlus
A brazilian indie game

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://www.youtube.com/watch?v=p2zfNnjcJ2M

# Requirements
Bass, Bass_FX http://www.un4seen.com/

GLEW, OpenGL
GLFW (included as submodule)

# Compiling on linux
(instructions for Debian based, need search equivalent packages in other distros):
```
sudo apt update
sudo apt install xorg-dev
sudo apt install extra-cmake-modules cmake build-essential gcc g++ libgl-dev libglew-dev

git clone  --recurse-submodules https://github.com/Fabio3rs/Guitar-PlusPlus/
cd Guitar-PlusPlus/
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE
make
```

Binary output directory: Guitar-PlusPlus/build/bin
Place the binary with the assets (not updated yet...) https://brmodstudio.forumeiros.com/t1590-guitar-topico-oficial

Put your song assets in the data/songs directory

