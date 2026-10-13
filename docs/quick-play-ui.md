# Quick Play song browser

The screen uses `SongLibrary` snapshots and existing `CMenu` composition. Switching
tabs, searching and sorting does not read charts, SQLite or the filesystem. The
index and BASS duration probing are unchanged, including incremental F5 and full
Ctrl+F5. Discovery remains recursive.

## Composition and ownership

- `src/ui/QuickPlayView.{h,cpp}` builds the header, tabs, controls, navigation,
  table, informational preview and footer. Group construction/filtering operate
  on an immutable snapshot; separate builders populate existing menu controls.
  State belongs exclusively to the UI thread. Song paths identify rows, not titles.
- `src/ui/SelectionMenus.cpp` owns the screen lifecycle, scan requests/cancellation,
  presentation callbacks and asynchronous cover requests through the existing
  texture manager. Cover handles are borrowed; no UI code uploads GL resources.
  Panels close on exit before gameplay, including exceptional exits.
- `src/CMenu.{h,cpp}` adds opt-in cached cell ellipsis and mouse selection without
  activation. Existing controls keep their defaults. No renderer backend or GL
  state changes are involved.
- `src/CUIDefinition.{h,cpp}` exposes those two row flags in declarative Lua.
- `CMakeLists.txt` registers the new view compilation unit; regression coverage is
  in `tests/quick_play_tests.cpp` and `tests/menu_renderer_tests.cpp`.

The root is `selectSongMenu`. Attached regions are `quickPlayControls`,
`quickPlayNav0` (folders), `quickPlayNav1` (artists), `quickPlayNav2` (initials),
`quickPlayNav3` (charters), `quickPlayResults` and `quickPlayPreview`. Only navigation
panels participate in explicit tab visibility. The result and preview panels stay
attached; focus/modal routing is still handled by CMenu. Windows and modals are
never hidden just because another region has focus.

`MenuOverrides.lua` can address these names and their normal presentation fields.
Useful IDs include `tab0` through `tab3`, `search`, `sort`, `filter`, `context`,
`column0` through `column4`, `group:<key>`, `song:<relative-path>`, `title`, `artist`,
`cover`, `charter`, `format`, `parts`, `stems`, `scanStatus`, `play` and `back`.
Wrapped preview lines append `_lineN` to the field ID. Presentation patches are
applied through the existing menu service; no alternative theme system is added.

## Interaction and data

Folders include ancestors of indexed song directories. Enter/click on a directory
selects all songs in that directory and its descendants, and expands/collapses its
children. Counts describe indexed songs, including unavailable ones. Empty folders
without indexed descendants are not shown. Artist and charter groups have explicit
fallbacks for absent fields. Title navigation offers All, A–Z and a `#` group.

Search is shared by the four tabs and matches titles, artists and charters. It
folds ASCII/Latin-1 case and common Portuguese accents; it is not complete Unicode
collation. Common chart color/bold/italic markup is removed for display/search,
without modifying persisted metadata. Ordering is ascending and stable by path
for equal keys; absent durations sort last. The sort button cycles four orders.
The filter toggles All/Available. Random highlights a playable filtered result.

Each tab retains its group, selected path and result scroll, while each navigation
panel retains its own scroll. Publications with the same result path order update
rows in place, preserving text caches. Progress-only publications do not rebuild
rows. Structural changes retain the selected path if possible, otherwise select
the first playable result. This is not table virtualization.

Single-click highlights a song for preview. Enter while focused on the table, or
the footer Selecionar button, starts it. Tab/Shift+Tab cycles regions (including
read-only preview rows); arrows navigate locally. The preview scrolls independently
and can be navigated by keyboard. Escape first leaves text editing, then goes back.
Modal input precedence and shortcut propagation remain CMenu responsibilities.

The table shows title, artist, charter, duration and difficulty initials (F/M/D/E).
Overflow uses UTF-8-safe ellipsis within each column. Preview fields wrap using the
active font's measured widths, so full names remain available there. Its compact
instrument/difficulty summary comes from indexed chart parts; the legend explains
the initials. Audio lists the main audio filename and indexed stems separately. Unknown
parts are shown by their indexed name, and absent fields are omitted or marked as
unknown. Chart-estimated durations retain `~`. No audio preview is started.

## Current limits and future polish

- The default theme still uses the game's bitmap font and current 3D background;
  this implements the mock's organization, not its artwork or typography. Glyphs
  missing from that atlas retain the existing fallback.
- Panel widths are calculated from the window aspect when opening the screen.
  After changing the window aspect while it is open, re-enter Quick Play to
  recompute the three-column layout. Live responsive reflow is future work.
- Cover loading is asynchronous and keeps image aspect. Failed/missing covers show
  a neutral placeholder. Images use the existing game texture cache; replacing an
  image at the same path may require restarting to invalidate that resource.
- Favorites, graphical instrument icons, configurable/reorderable columns,
  descending sorting, scrollbars and richer folder keyboard shortcuts remain
  future polish. There is no automatic audio playback or new duration-probe option.
- Small screens abbreviate table fields with ellipsis; full values are available
  in the scrollable preview. Custom themes changing preview font sizes/column
  geometry should keep their layout coherent; wrapped lines are built on selection
  or library publication, not reshaped continuously by the renderer.

## Validation (2026-10-13)

The normal build and all 291 CTest entries passed. NativeLab ASan/UBSan with leak
checking passed 44 selected library/menu/composition tests, including the 14 new
Quick Play cases and cell-ellipsis regression. Coverage includes recursive folder
contexts, each grouping, accent/markup search, sorting/filtering, unavailable and
empty results, publication updates, stable row caches, modal precedence, mouse vs
Enter activation, first asynchronous population, UTF-8 wrapping, cover ownership/
aspect and declarative round-trips. A synthetic 10,000-song case exercises tab scroll
preservation and progress-only publications without rebuilding the table.

Real runtime captures were inspected at **800×600**, **1920×1080** and
**3440×1440**, using Xvfb/Mesa and the available 160-song library. They exercise tabs,
search, sorting, long titles, column ellipsis and independent preview scrolling.
Full HD and ultrawide runs also completed F5/Ctrl+F5, gameplay entry, pause/resume
and exit. The initial ultrawide automation timed out waiting for a gameplay pause;
the repeated run held Escape for 400 ms and passed. Gameplay input/timing code was
not changed, and these runs do not establish physical input latency.

Runtime tests used isolated saves/cache and an ALSA null output. Audible playback,
physical controllers, Windows and older hardware performance were not verified.
The bitmap font's existing missing-glyph fallback remains visible for some source
characters. There are no redistributed song or mock assets in this change.

Clang-format **21.0.0git** passed its dry-run check. Clang-tidy **21.0.0git** covered
all six changed/new translation units and their project headers. An unused test
assignment was removed. Remaining diagnostics concern layout constants/style,
existing code and the intentional exact comparison of a cell-width cache key;
there are no new analyzer/bugprone/performance findings in the added view. Local
logs/captures are under `build/quick-play-*` and `build/library-ui-*` (not tracked).

### Logo do cabeçalho

O host carrega `data/sprites/guitarpp-logo.tga` no gerenciador de texturas e
entrega o identificador à view. A imagem mantém a proporção original e ocupa
somente a faixa acima das abas; sem textura válida, permanece o nome em texto.
O TGA local em `build/bin` foi exportado do SVG mantido em `assets/logos/g++.svg`:

```sh
inkscape assets/logos/g++.svg --export-area-drawing --export-width=1024 --export-background-opacity=0 --export-filename=/tmp/guitarpp-logo.png
convert /tmp/guitarpp-logo.png -flip -alpha on -type TrueColorAlpha build/bin/data/sprites/guitarpp-logo.tga
```

O `-flip` compensa a orientação vertical usada atualmente pelo desenho de
sprites da UI. O arquivo tem 1024×372 pixels e canal alfa; `build/bin` não é
versionado e a distribuição precisa incluir esse asset gerado.
