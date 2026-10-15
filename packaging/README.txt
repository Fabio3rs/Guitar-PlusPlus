Guitar++ CI build (x86-64, RelWithDebInfo)
=======================================

Includes GuitarPP, Chart Studio, score-to-chart, chart dump and library scan
tools. Extract the entire ZIP before running. Launch from this directory so
Config.lua and data/ resolve correctly. Keep Windows PDB files with the EXEs
when investigating a crash; Linux executables retain their debug symbols.

Only repository-owned scripts and assets tracked in Git are included. Install
the full game asset pack separately, then apply this build's scripts/assets.
Put your own songs in data/songs/. Back up your Config.lua and saves before
updating an existing installation. No songs, user saves or caches are bundled.

Linux: Ubuntu 24.04 x86-64 is the compatibility baseline. Install the system
C/C++ runtimes, X11 and OpenGL drivers. BASS/BASS_FX, GLEW, SQLite3, libpng and
zlib are in lib/ and are located relative to the executable. From this folder:
    ./GuitarPP
    ./gpp_chart_studio
    ./gpp_score_to_chart --help
Use a ZIP extractor that preserves executable permissions (for example unzip).
If necessary, run chmod +x GuitarPP gpp_chart_studio gpp_score_to_chart
gpp_dump_chart gpp_scan_library before launching.

Windows: run GuitarPP.exe or gpp_chart_studio.exe. Keep bass.dll and bass_fx.dll
beside the EXEs. Other build dependencies and the MSVC runtime are linked
statically. OpenGL drivers are required. Linux webcam capture and unfinished
legacy networking are excluded. The local two-player Server MP mode remains.

The optional Python UMAP experiment environment is not bundled. It is not
required for the game or normal Chart Studio operation.

These are CI builds, not installers. Building without native CPU optimizations
does not guarantee support for every older OS or GPU. See licenses/ for the
included components' notices and BASS/BASS_FX usage terms.
