# Guitar++ GUI Studio

GUI Studio is an independent development executable with **Menu Studio** and
**Font Studio** tabs. Dear ImGui supplies the editor controls; the previews use
`CMenu` and `CFonts`, including the game's atlas analysis, text layout, controls
and modal input dispatch. It links `GuitarPP_lib` but does not instantiate
`GPPGame`, initialize audio, start gameplay, or load personal settings/global
game scripts.

The Studio edits complete definitions for the rename and create-profile modals.
The game also supports presentation patches for its C++ screens, described below;
editing those patches in the Studio is a later step.

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
text, 9 table row, 10 image, 11 panel. Control fields include `id`, `text`, `value`, `languageKey`, `texture`,
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
Font metrics are exported with up to five decimal places, rounded and without
trailing zeros. The Studio executes font scripts in a separate Lua state with the font functions
and limited Lua helpers; game, process and file-loading APIs are unavailable.
It reopens generated exports and the maintained font theme, not arbitrary game
scripts. Failed imports preserve the previous font and textures.

## Export, captures and validation

Exports default to `build/gui-studio/Gui.lua` and `Theme.lua`. An absolute export
path is also accepted. Hover the export field or button to see its full destination.
Each export opens a confirmation with the full saved path and a **Copy path**
button; failures show the destination and error instead. Press **OK**, Enter or
Escape to dismiss it. Nothing automatically installs exported files in the game.
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

## C++ screens and theme presentation

`GPPGame::menus` owns a `CMenuSystem`: the menu registry, navigation loop,
current/main menu, and theme presentation. Screen code lives under `src/ui`,
separated into initial/settings menus, selections, campaign, pause, development
controls and shared background rendering. `CMenu` remains the runtime widget
implementation. Pause returns `PauseResult::Resume` or `Exit`; it no longer adds
menu fields to `CGamePlay`. Campaign serialization and gameplay clocks are unchanged.

The optional `MenuOverrides.lua` beside `Theme.lua` returns a table keyed by
menu name and local option ID:

```lua
return {
    mainMenu = {
        style = { padding = 0.06 },
        options = {
            play = {
                y = 0.35, centerX = 0.0,
                style = {
                    textSize = 0.12,
                    textColor = { r = 1, g = 0.8, b = 0.2, a = 1 },
                },
            },
        },
    },
}
```

Named screens include `mainMenu`, `playOptions`, `mainCampaingOptions`,
`optionsmenu`, `audiomenu`, `extrasmenu`, `chartsmenu`, `chartCompileMenu`,
`playerSelectMenu`, `selectSongMenu`, `continueCampaingOptions`, `campaingMenu`,
`pauseMenu` and `devMenus`. The initial menu uses `play`, `settings`, `extras`,
`help`, `quit` and `sampleInput`; pause uses `resume` and `exit`; audio uses
`volume` and `back`. Selection screens use `back`, profiles use `createProfile`,
and song rows use `song:` followed by the directory name. Use bracketed Lua keys
for names containing spaces or punctuation. Other IDs are explicit in their
screen builders.

`menuOpt::optionName` is qualified once as `menu.option`. `addOpt` returns `-1`
without inserting/registering an event on a duplicate. Anonymous controls get
unique `auto_N` IDs with no stability guarantee across reconstruction. Complete
`Gui.lua` definitions transfer their IDs to this same runtime identity. The old
numeric event names are not aliased.

Supported menu properties: `x`, `y`, `flow` (0 manual, 1 vertical, 2 horizontal),
`flowX`, `flowY`, `flowGroup`, `anchorLeft`, `leftMargin`, `viewportEnabled`,
`viewport`, `panel`, and `style`. Rectangles replace the complete rectangle;
provide all four coordinates: `left`, `bottom`, `right`, `top`.
Option properties: `x`, `y`, `fieldWidth`, `listWidth`, `centerX`, `anchorRight`,
and `style`. Styles use the existing `CUIStyle` fields; colors use named RGBA
components. Text, images, actions, input bindings, control types, user values and
lists cannot be changed by this file. Unknown properties and invalid dimensions
are errors. Missing option IDs produce a diagnostic when the menu is prepared,
without creating controls.

Omitted properties inherit. Style precedence is global, C++ menu, Lua menu,
C++ option, then Lua option. Thus a button's explicit C++ text size still wins
over a Lua menu size; use `options[id].style` to override that button. Fixed axes
or explicit alignment retain the item's space in the automatic flow. Undeclared
axes remain automatic. `x`, `centerX` and `anchorRight=true` are mutually exclusive
in one patch and replace inherited horizontal anchors. `flowX` replaces inherited
left anchoring; combining it with `anchorLeft=true` is invalid.

The UI thread reads files only on theme application. Declarative documents are
not registered in the runtime Lua callback dispatcher. Missing/invalid patches
restore C++ defaults, including after a theme switch; invalid documents are
logged and never partially applied. Existing complete modal definitions change
on the next modal opening. Presentation patches can update existing instances
without rebuilding callbacks, resetting focus, or replacing edited values.

Builders call `clearOptions()` before reconstructing a screen, then add its
controls. This restores original presentation before capturing a new base.
`preparePresentation()` applies the cached theme before first layout; newly
appended options also receive their matching patch. Do not bypass this lifecycle
by clearing the public option container directly. Clones own independent copies
of their presentation base. Manual developer edits are transient and do not
replace that base.

`ui::readOverrides`, `writeOverrides`, `capturePresentation`, and
`CMenu::originalPresentation()` provide the foundation for future Studio editing.
`CMenuSystem::replacePresentation()` applies already parsed data; a future file
watcher should deliver an explicit request to the UI thread. No file watching,
per-frame patch polling, or gameplay update/render hooks are installed now.

Maintain both the source theme and its runtime copy under `build/bin/data/themes`.
The default patch file is empty with a commented example, preserving the current
appearance. Profile selection/persistence remains unimplemented; the existing
creation modal is preserved without the former disabled background scan.

### Menu refactor validation (2026-10-12)

The Debug build passed. The full NativeLab run passed 195 of 196 tests; the
remaining `gui_studio_ui` test failed GLFW initialization and passed on an
isolated rerun. A separate ASan/UBSan run passed all 34 selected menu, Lua and
Studio core tests, using the repository sanitizer suppression files. Fifteen
regression cases were added for presentation/identity and Lua queue ownership.

All 28 changed/new C++ files were formatted with clang-format. Clang-tidy's
analyzer and bugprone checks covered the changed translation units. The review
fixed a use-after-move in Lua script queue insertion and added a regression;
existing warnings outside this refactor remain.

A NativeLab graphical run used Xvfb/Mesa with OpenGL 2.0 and GLSL 1.10 and assets
from `build/bin`, with isolated saves and test-only theme patches. It exercised
profile/main/song/campaign/settings navigation, volume adjustment, gameplay
entry, pause/resume and exit. Captures confirmed the main-button layout/color
and pause-button color overrides. BASS used a test-only no-sound-device shim;
real audio timing, physical controllers, Windows and hardware GPU performance
were not validated. Runtime artifacts are under `build/menu-review` and are
not source assets.

## CMenu composition and rectangular controls

The runtime can compose persistent panels using the same `CMenu` renderer and
input routing as ordinary menus. The GUI Studio inspector does not yet expose
these new controls; the C++ and declarative readers/writers support them.
The images used to motivate this API are a future Quick Play layout, not an
implemented song browser.

```cpp
CMenu root("library");
CMenu results("results");
results.viewportEnabled = true;
results.viewport = {-.6, -.7, .7, .6};
results.setFlow(CMenu::Flow::Vertical, -.6, .5);
results.tableColumns = {.7, .6};
CMenu::menuOpt song;
song.optionName = "demo";
song.type = CMenu::table_row;
song.group = 1;
song.cells = {"Canção de exemplo", "Artista"};
song.active = true;
results.addOpt(song);
const int id = root.pushPanel(results);
auto &panel = *CMenu::getUiAt(static_cast<size_t>(id)).m;
root.focusInterface(panel);
```

`pushPanel` clones its model, returns the existing UI slot identifier, and does
not transfer input focus. Bounds are in the child's local UI coordinates,
translated by its `x/y`, like its viewport. They define the outer clipping and
mouse-routing region independently of background textures. Child coordinates do
not implicitly inherit the parent's translation. Attach panels before applying
runtime presentation patches; keep ownership/composition in the host.

`pushPanel(model)` follows the child's current viewport for its outer bounds.
`pushPanel(model, bounds)` keeps an independent rectangle, useful for headers
outside the scrolling area. `getPanelBounds()` returns the effective local bounds;
`getFixedPanelBounds()` is empty in follow mode. `setPanelBounds(rect)` resizes
independent bounds, and `setPanelBounds(std::nullopt)` resumes following the
viewport. Explicit bounds must be finite and have positive dimensions. These
changes affect drawing and mouse routing without rebuilding options.

Overrides accept `panelBounds` for independent bounds and `panelFollowsViewport`
to choose following (`true`) or independent bounds (`false`, requiring
`panelBounds`).
Do not combine explicit bounds with `panelFollowsViewport=true`. Replacing a theme
restores the original bounds/mode before applying the new patch. The older `panel`
field still describes the background quad; it is not the outer clipping region.


`focusedInterface()` queries the focused region in the caller's root tree.
`focusInterface(menu)` rejects foreign trees, regions with no enabled controls,
and destinations blocked by a modal. `focusNextPanel(backwards)` implements the
same cycle as Tab/Shift+Tab and is available for future controller bindings.
The root participates before its panels; descendants follow registration order.
Empty/passive/disabled regions are skipped. Arrows keep their existing local
behavior. Without panels, Tab still visits individual options. Tab leaves text
editing; an active modal restricts navigation to its own tree.

Unconsumed press commands (shortcuts and Escape) bubble from permanent panels to
their owner, stopping at a modal or legacy window. Local commands win. Ancestor
commands do not change keyboard focus or the ancestor's focused option. Editing
consumes keys locally; Escape first exits editing. Navigation and character events
do not bubble, and legacy windows retain their existing behavior.

The wheel uses the current cursor position and scrolls the region under it,
without moving keyboard focus. `MenuEvent::Scroll.x/y` remain wheel deltas.
Mouse release goes to the captured region even after a focus change. Opening a
modal or losing window focus cancels capture. Modals render over panels, including
panels attached later; closing a modal restores the previous live focus target.
Clearing/rebuilding one panel leaves its siblings intact. `clearOptions()` still
closes that menu's own children and resets its options/presentation/scroll.

New option types append to the existing enum: **9 table row, 10 image, 11 panel**.
Their configuration is illustrated in
[`examples/menu-panels.lua`](examples/menu-panels.lua). Instantiate definitions
with `ui::instantiate`, then attach the resulting menu models from C++.

- `tableColumns` is a menu-level array of positive widths. Every row uses its sum
  for the hitbox. `cells` contains text, with missing trailing cells left empty;
  definitions reject rows with more cells than columns. C++ rendering safely
  ignores excess cells. `height = 0` uses the current `rowStep`.
- Rows are one selectable option, including their empty space. Cell text is left
  aligned, vertically centered and clipped, without wrapping or ellipsis.
  Header labels can remain ordinary `static_text` outside the scrolling group.
  There is no sorting, icon-cell support or library data model in this control.
- `image` and `panel` require positive `width/height` in definitions and are
  passive, including for shortcuts. An image draws its full texture into the
  supplied rectangle; its aspect ratio is the host's responsibility. Texture
  zero draws nothing. Texture handles are borrowed and never uploaded/deleted
  by `CMenu`; `TextureResolver` runs on instantiation, not during rendering.
- `panel` uses `style.background`, `style.border`, and `style.borderWidth`,
  inheriting the menu's `panel`, `panelBorder`, and `panelBorderWidth` defaults.
  Border width defaults to zero and is expressed in UI units. Background can
  also be overridden on a row or button.
- `textbtn` accepts optional `width/height`; zero retains automatic sizing on
  that axis. Existing automatic controls retain their legacy anchor/flow rules.
- `active` is host-owned persistent selection, copied as an initial value when
  instantiated/cloned. Navigation and transient resets do not clear it. The
  selection background remains visible after moving focus; only the region
  receiving keyboard input displays the focus marker/outline. Set the flag
  explicitly when changing tabs or the selected song.

`MenuOverrides` accepts `tableColumns`, option `width/height`, and the new style
properties. It does not accept `cells`, `active`, texture handles or callbacks.
Omitted properties restore/follow the captured base. Zero box dimensions in a
patch collapse an image/panel; zero button axes restore automatic sizing.
An empty column override hides row geometry. Patches preserve content and
interaction state; geometry changes can reveal the focused option and clamp
scroll to the current content. Invalid documents are rejected before application.
Theme reload uses the existing recursive `visitInterfaces` path.

Rows cache glyph layouts by text/font/revision. Geometry does not shape cell
text, and offscreen rows do not populate their cell caches. Layout still visits
the option list: this is not a virtualized table. All UI state and GL access
remain on the UI/context thread; gameplay ownership and synchronization are
unchanged.

### Composition validation (2026-10-13)

The runtime, tests, GUI Studio and its core tests built with Clang and
`GPP_NATIVE_CPU=OFF`. All 94 selected CTest entries passed: menu/composition,
Lua, fonts, shaders and both Studio suites, including real mouse/keyboard
interaction under Xvfb. The graphical run forced OpenGL 2.0 / GLSL 1.10.
There are 31 new regression cases across composition, presentation and rendering.

NativeLab MCP ran the instrumented build with ASan/UBSan and leak detection:
64 CPU entries passed without suppressions, and 29 graphical/Studio-core entries
passed with only the existing vendor suppressions documented above. The follow-up includes eight cases for command propagation, editable text,
modal boundaries, resizable bounds, theme restoration and pixel clipping.
No new application suppression was added.

Clang-format was applied to changed ranges and the new test file. Clang-tidy
reviewed all eight changed/new translation units with the repository checks,
filtering reported diagnostics to changed ranges. Final reports have no
analyzer, bugprone or performance findings on those ranges; style suggestions
remain, including numeric UI/test constants and the existing public-field API
convention. Local review logs are under `build/cmenu-validation/`.

This validates synthetic rendering and menu interaction, not in-game song
selection, Windows, physical controllers or performance on older GPUs.
