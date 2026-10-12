# Guitar++ GUI Studio

GUI Studio is an independent development executable with **Menu Studio** and
**Font Studio** tabs. Dear ImGui supplies the editor controls; the previews use
`CMenu` and `CFonts`, including the game's atlas analysis, text layout, controls
and modal input dispatch. It links `GuitarPP_lib` but does not instantiate
`GPPGame`, initialize audio, start gameplay, or load personal settings/global
game scripts.

The first runtime integration is intentionally limited to the rename and create
profile modals. Migrating the game's initial menu catalogue is a later step.

## Build and launch

```sh
cmake -S . -B build -G Ninja -DBUILD_GUI_STUDIO=ON -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --target gpp_gui_studio gpp_gui_tests GuitarPP --parallel
./build/bin/gpp_gui_studio
```

`BUILD_GUI_STUDIO` defaults to OFF and works with the other Studios disabled.
It reuses `gpp_studio_imgui` and the pinned GLFW compatibility shim. The game
itself does not link ImGui or PNG. The editor needs the usual game build
requirements, libpng, and an OpenGL 2.0 compatibility context / GLSL 1.10.
No framebuffer-object extension or newer GL feature is required by its preview.

By default assets come from the workspace's `build/bin`, when its font exists,
otherwise the workspace root. Use `--asset-root DIR` to select another runtime.
The directory contains `data/sprites`, `data/themes`, etc. TGA is the supported
image format, matching the runtime. Missing assets produce a visible diagnostic;
they are not synthesized or added to version control.

## Modals and menu editing

The initial catalogue contains `uiRenameMenu` and `uiCreateProfileMenu`, loaded
from `data/themes/gppdefaulttheme/Gui.lua` when present, otherwise from the shared
C++ defaults. Choose **New menu** or **Duplicate menu** to create other
standalone definitions. **Open** replaces the catalogue with a declarative file.

Select a control in the inspector or preview. Drag it in the preview to freeze
automatic flow at its current positions and move the selected control. The
inspector edits control order, text, initial input values, translation keys,
control types, positions, flow/anchors, clipping, textures, styles and actions.
The preview reads PT-BR translation strings from the selected asset root; missing
keys retain their fallback text. Unchecked **Override** properties inherit their parent style. **Move up/down**
changes order without changing the control's stable ID.

Enable **Test interaction**, then click the preview to focus it. Keyboard,
character, mouse and scroll events run through `CMenu`. Clicking another panel
or losing window focus cancels preview capture. For a focused text field,
Escape first ends text editing; another Escape closes the modal. **Restart
preview** reopens it. Preview edits to values do not become definition changes.
Game actions are logged, not executed. `menu:ID` navigates to another definition;
`modal:ID` opens one as a child interface. Other action strings are identifiers
for future host bindings. This version's runtime consumes the two built-in
modal definitions, not the Studio's navigation/action catalogue.

`CModalManager` owns modal definitions and binds their fields when opening each
instance. `CMenu` retains ownership of the generic interface stack, focus and
input blocking. The runtime calls `openRename(owner, text)` or
`openCreateProfile(owner)` instead of accessing templates/field indices.
Renaming keeps the existing live text binding. The profile modal updates the
path label; this change does not add profile persistence.

### Declarative Lua

`Gui.lua` returns an array of `MenuDefinition` tables. Its fields map to C++
structs in `CUIDefinition.h`. `options` contains `OptionDefinition` tables.
Missing fields use struct defaults; omitted style fields inherit. Exports include
persistent data only, never interaction state, GL handles, pointers or callbacks.
Strings are UTF-8, floating-point values retain round-trip precision, and files
are validated before replacing the active catalogue.

Menu fields include `id`, `title`, `modal`, `background`, `x`, `y`, `flow`
(0 manual, 1 vertical, 2 horizontal), `flowX/Y`, `flowGroup`, `anchorLeft`,
`leftMargin`, `viewportEnabled`, `viewport`, `panel`, `style`, and `options`.
Rectangles use `left`, `bottom`, `right`, `top`. Texture names are paths relative
to the asset root, or absolute paths for local development.

Control types retain CMenu's numeric values: 0 OK, 1 text input, 2 cancel,
3 text button, 4 multi button, 5 list slider, 6 UI button, 7 drag bar, 8 static
text. Control fields include `id`, `text`, `value`, `languageKey`, `texture`,
`group`, `shortcut`, `x/y`, `fieldWidth`, `listWidth`, `disabled`, `manual`,
`anchorRight`, `centerX`, `back`, `enableEnter`, `maxBytes`, `items`, `actions`,
and `style`. `maxBytes` is the existing UTF-8 byte limit, not a glyph count.

The built-in modals must remain modal, retain a text-input control named `name`
and an enabled close button. The profile modal also requires static text named
`path`. Their positions, order and presentation can change. Invalid definitions
are rejected without replacing the last valid definitions.

After the existing `applyTheme` callback, the runtime reads the theme's sibling
`Gui.lua` through the shared loader. Absence uses the C++ defaults; an invalid
file is logged and leaves the previous definitions intact. Already open
instances keep their bindings and are replaced on the next opening.

## Font editing

The existing `default` font loads automatically, including its theme adjustments.
Select an atlas and codepoint in the list or by clicking a cell. After clicking
the atlas, typing selects the produced Unicode character in the current font
(and switches atlas if needed). Shift and AltGr follow the OS keyboard layout;
dead-key accents work when the OS delivers the composed character. Missing
glyphs leave the selection unchanged and add a diagnostic to the event log.
Escape, clicking outside the atlas, switching tabs or losing window focus ends
keyboard selection. Text/numeric fields and popups keep their normal input;
Ctrl-only, Alt-only and Super shortcuts do not select glyphs. Ctrl+Alt is treated
as AltGr, so any printable character emitted by that combination selects a glyph.
This is character selection, not physical-key binding or Unicode normalization. The atlas shows
its loading grid, raw detected ink bounds and selected cell. **Fit atlas** fits
the image; wheel zooms and middle drag pans. Green boxes are the original ink
bounds; vertical adjustments change the text preview, not the source pixels.

Edit vertical offset (positive moves upward), height, advance and bearing.
Offsets/heights use the same units as `setFontCharMetrics`; face metrics use
`setFontMetrics`. Guides show baseline, ascent and descent. Only the selected
codepoint changes. **Reset glyph metrics** restores that glyph's detected defaults.
The text preview starts at 48 logical pixels. **Preview size** adjusts it from
16 to 128 pixels independently of panel height; **Fit text** fits the sample to
the available width within that range. Drag the horizontal divider above these
controls to resize the atlas/preview areas. These settings affect only the editor,
not exported glyph metrics.
Text previews and menus immediately use the selected font and revised metrics.
The runtime's menus continue to use the font named `default`.

**New font** adds a named font from an existing TGA, explicit columns/rows and
UTF-8 mapping with newlines between atlas rows. **Add atlas** extends the selected
font. **Atlas configuration** edits a draft; **Apply atlas configuration** reloads
and analyzes it. This can fail if existing adjustments refer to removed glyphs;
the previous working font remains active. Font rasterization from TTF/OTF is not
part of this version.

**Export** writes the existing `Theme.lua` contract: `addTextureToFont` first,
then `setFontMetrics`, `setFontCharMetrics` and `setFontCharSpacing`, inside an
`applyTheme` callback. Multiple atlases and codepoints beyond ASCII are retained.
The Studio executes font scripts in a separate Lua state with the font functions
and limited Lua helpers; game, process and file-loading APIs are unavailable.
It reopens generated exports and the maintained font theme, not arbitrary game
scripts. Failed imports preserve the previous font and textures.

## Export, captures and validation

Exports default to `build/gui-studio/Gui.lua` and `Theme.lua`. An absolute export
path is also accepted. Nothing automatically installs exported files in the game.
Use the theme directory under your chosen runtime to apply them deliberately.
For maintained theme changes, update both the repository theme and its copy in
`build/bin/data/themes`. Unexported documents prompt before opening another file
or closing the editor. The atlas image itself is never modified.

```sh
./build/bin/gpp_gui_studio --asset-root build/bin --panel fonts \
  --frames 5 --ui-capture fonts.png
./build/bin/gpp_gui_studio --menus build/gui-studio/Gui.lua \
  --font build/gui-studio/Theme.lua --width 1280 --height 800 \
  --frames 5 --ui-capture menus.png
ctest --test-dir build -R '^(Font|Menu|Shader|gui)' --output-on-failure
```

`--output-dir` changes the output directory; `--hidden` hides the window for
bounded capture runs but still requires a display/context. A capture without
`--frames` uses five frames. Invalid explicit file paths and CLI arguments return
nonzero. Use `xvfb-run -a` on headless Linux. No RPC interface is included.

The opt-in tests cover Lua round trips, UTF-8 and styles, stable modal field
bindings after reordering, required fields, failed replacements, codepoint
adjustments, modal input and GL state. Xvfb/xdotool tests exercise actual mouse
reordering/dragging, numeric glyph editing, export/reopening and PNG captures
using generated fixtures. The Scene/GUI Studio CI job enables these tests and
uploads captures. These checks do not measure hardware performance or validate
a full gameplay session. The gameplay/update split and its existing shared-state
risks are unchanged.

Editor state, resources and input are owned by one UI thread. CPU atlas analysis
runs on load/configuration changes; per-glyph metric edits do not upload textures
or rescan pixels. Previews draw into a clipped viewport, restoring GL state and
engine binding caches across the ImGui boundary. Resources are released while
the context remains current. There is no new rendering cost in gameplay beyond
the existing font/menu paths and the small host-context selection in CMenu.

### Sanitizer run

A build with `BUILD_SCENE_STUDIO=OFF`, `BUILD_AUDIO_STUDIO=OFF`, and
`BUILD_GUI_STUDIO=ON` also supports the same tests with ASan/UBSan. Use NativeLab
for these runs when available. The existing `tests/lsan.supp` covers Mesa GLX
process-lifetime allocations. `tools/gui_studio/ubsan.supp` narrowly covers the
pinned GLFW X11 timestamp-wrap expression `(1 << 31)` at `x11_window.c:1274`,
which UBSan reports during real keyboard input. Neither suppression covers
application frames; the third-party source is unchanged.

```sh
env ASAN_OPTIONS=halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:suppressions="$PWD/tools/gui_studio/ubsan.supp" \
  LSAN_OPTIONS=suppressions="$PWD/tests/lsan.supp" \
  ctest --test-dir build/ui-asan -R '^gui_studio' --output-on-failure
```

Validation on 2026-10-12: the runtime and Studio built successfully; the selected
49 CTest entries (font/menu/shader regressions and both Studio entries) passed.
The Studio core contains nine cases; its final core and mouse/keyboard suites
also passed under ASan/UBSan/LSan with the documented vendor suppressions. The
sanitizer configuration built with both other Studios disabled. A font capture
also succeeded with Mesa forced to OpenGL 2.0 / GLSL 1.10. Local captures are in
`build/gui-studio`, and synthetic interaction artifacts in its `tests` directory.
A full in-game session, Windows, physical input devices and hardware performance
were not validated in this run.

### Follow-up review (2026-10-12)

The review added regressions for non-string Lua errors and process locales that
use decimal commas/grouping. Exporters always use the classic numeric locale.
The Studio rejects zero/negative/oversized atlas grids before loading textures;
this keeps its explicit preview grid consistent with the imported document.
The core now contains eleven cases. New C++ files were formatted in full with
clang-format 21 using the repository configuration; existing runtime files were
formatted only in changed ranges, preserving unrelated edits.
