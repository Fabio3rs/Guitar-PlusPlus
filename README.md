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

# Garage lighting and OBJ assets

The garage uses a shared OpenGL 2.0 / GLSL 1.10 renderer in menus and gameplay.
These optional settings belong before `setConfigs()` in `Config.lua`:

```lua
-- 0: original; 1: lighting; 2: 512px shadows; 3: 1024px shadows
setGameVar("backgroundQuality", 2)
setGameVar("backgroundAO", true)
setGameVar("backgroundShadowBias", 0.001)
```

`main()` loads `./Config.lua` relative to the working directory at startup.
Plain Lua globals do not update these registered game variables: use
`setGameVar` as above. The same calls from a running script take effect on the
next rendered frame, without recreating the window. Editing the file itself
is applied at the next launch; it is not watched for changes.

Disabling shaders also selects the original path. Shadows require
`EXT_framebuffer_object` and a complete depth framebuffer; otherwise the renderer
keeps per-pixel lighting. The garage light is fixed near its ceiling fixture.
Static shadow maps are reused across camera changes and invalidated on model
reload or shadow-resolution changes. The garage material overrides are confined
to its renderer; notes and other OBJ users retain their existing drawing path.

The CPU-only OBJ parser preserves object/group/material names, UVs (including
repeated coordinates), supplied normals, and MTL Ka/Kd/Ks/Ke/Ns/d/Tr/Ni/illum.
It handles relative indices and triangulates simple planar concave polygons.
Missing normals are generated with OBJ smoothing groups; missing UVs disable
texturing for the affected triangle instead of reusing the previous texture.
Degenerate/untriangulatable faces are logged and skipped; invalid indices reject
the new model without replacing a previously loaded one. Line/point primitives
are not rendered. MTL `map_Kd` supports paths with spaces, scale/offset and clamp;
without it, an existing `material-name.tga` retains the legacy convention.

Geometry remains in typed vectors. VBO owners cannot be copied; uploads use
separate typed arrays with checked byte sizes. Loading and replacement on worker
threads do not create or delete GL objects. Old buffers are retired on the next
render, and GL state is restored before drawing the highway or UI.

## Optional AO bake and scene preview

```sh
cmake -S . -B build -DBUILD_SCENE_TOOLS=ON
cmake --build build --parallel
build/bin/gpp_bake_ao build/bin/test/garage_gpp.obj
```

The offline baker writes `garage_gpp.obj.ao` (version 1, geometry/source signature,
atlas dimensions and independent UVs) and `garage_gpp.obj.ao.pgm` (8-bit P5 atlas).
It does not modify OBJ, MTL or diffuse textures. The loader rejects stale or
malformed sidecars and continues without AO. Regenerate both files after editing
the model. Missing sidecars are supported, including in existing installations.
Generated assets in `build/` are local and are not added to the repository.

The initial baker uses 32 cosine-weighted rays per texel, a 0.65 world-unit
radius, a BVH, triangle islands with padding, and bounded atlas dimensions. It
bakes geometric occlusion, not lighting or texture-alpha occlusion; inspect thin
or alpha-cutout surfaces before distributing the result. Garage oil decals and
the emissive fixture are excluded as occluders. AO affects ambient light only;
dynamic shadows use diffuse alpha cutouts. No bake work runs during gameplay.

`gpp_preview_background MODEL.obj OUTPUT.ppm QUALITY [FRAMES]` renders a hidden
960x540 window and reports synchronized frame times and shadow-map updates.
Use absolute asset/output paths and a scratch working directory: the existing
game texture manager initializes save/configuration services there. On headless
Linux run through `xvfb-run -a`; Mesa can exercise the baseline using
`LIBGL_ALWAYS_SOFTWARE=1 MESA_GL_VERSION_OVERRIDE=2.0 MESA_GLSL_VERSION_OVERRIDE=110`.
These timings describe the standalone garage, not full gameplay or older GPUs.

Tests are included in `ctest --test-dir build --output-on-failure`: CPU OBJ/AO
fixtures plus OpenGL shader, upload/ownership, state-restoration and shadow-cache
checks. Visual validation of gameplay and multiplayer is still required when
changing scene settings or assets.
