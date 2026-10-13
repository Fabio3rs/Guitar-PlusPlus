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
selects all songs in that directory and its descendants. The separate disclosure
arrow expands/collapses children without changing the result context or selected
song. Right expands the focused directory and Left collapses it; Enter selects.
Disclosure buttons are skipped by keyboard traversal. Expansion survives tab
switches, including folders containing both songs and subfolders. Counts describe indexed songs, including unavailable ones. Empty folders
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
the footer Selecionar button, starts it. Tab/Shift+Tab cycles regions; arrows navigate locally. The preview has a single
fixed focus target, `scrollPreview`: Up/Down and PgUp/PgDn scroll its passive
content. The mouse wheel scrolls the region under the cursor without moving
focus. Informational fields are no longer individual keyboard stops. Escape first leaves text editing, then goes back.
Modal input precedence and shortcut propagation remain CMenu responsibilities.

The table shows title, artist, charter, duration and difficulty initials (F/M/D/E).
Overflow uses UTF-8-safe ellipsis within each column. Preview fields wrap using the
active font's measured widths, so full title and artist names remain available there. Its compact
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
  descending sorting, scrollbars remain future polish. There is no automatic audio playback or new duration-probe option.
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

## Refinamento visual (2026-10-13)

A composição continua usando três painéis persistentes e snapshots da biblioteca.
O logotipo original, a fonte bitmap e o cenário 3D foram preservados. Nenhum backend
de biblioteca, parsing, áudio, gameplay ou estado OpenGL foi alterado.

- Abas e músicas ativas mantêm preenchimento laranja. O foco usa contorno azul-cinza
  e preenchimento sutil, inclusive ao apontar uma aba inativa. Os campos opcionais
  `style.focus` e `style.focusBackground` seguem o mesmo `MenuOverrides.lua`; ausentes,
  mantêm a aparência anterior dos outros menus. Seleção/foco continuam no `CMenu`.
- Fundos mais opacos, listras alternadas mais perceptíveis e cabeçalhos secundários
  reduzem interferência do cenário. Título e artista recebem mais espaço na tabela;
  duração e níveis têm largura mínima. As células conservam clipping/ellipsis.
- Busca, ordenação e disponibilidade compartilham uma faixa de controles. O rodapé
  distribui as cinco ações pela largura disponível e separa o status em outra linha.
  Em 800×600, a navegação fica ligeiramente mais estreita para acomodar a tabela;
  os textos principais usam tamanho equivalente a 12 pixels de altura lógica.
- Na prévia, título/artista precedem a capa. Uma moldura de tamanho fixo mantém o
  placeholder estável; imagens válidas ficam centralizadas e preservam a proporção.
  Duração/BPM ficam ao lado. Charter, álbum e ano/gênero usam cor secundária.
  Divisores separam instrumentos, áudio e informações de arquivo.
- Dificuldades indexadas usam F/M/D/E coloridos; lacunas usam `-`, sem avaliação
  numérica inventada. Áudio principal e stems têm rótulos separados. O caminho vai
  ao final, limitado a duas linhas com reticências. Não foi criado tooltip/modal
  para o caminho completo. Título/artista longos continuam quebrando linhas e podem
  ser consultados pela rolagem independente.
- A extensão mínima de interação em `CMenu::menuOpt` é exclusivamente C++:
  `keyboardFocus=false` exclui a seta do percurso de teclado; `navigationKey`
  trata as setas da árvore e a rolagem da prévia depois da edição de texto e do
  roteamento modal normal. Esses campos não são configuráveis por temas.

Os cinco ícones originais desenhados por Codex, seus SVGs/TGAs, manifesto de uso,
comandos de exportação e orientação vertical estão documentados em
[`data/sprites/quick-play/README.md`](../data/sprites/quick-play/README.md).
Os arquivos também foram copiados para `build/bin/data/sprites/quick-play/` para
validação local. São texturas pequenas reutilizadas pelo cache; não há atlas/UV,
fontes novas ou capas fictícias.

### Limites preservados

A recomposição por aspect ratio continua ocorrendo ao entrar no Quick Play; após
resize, reentrar recalcula o layout. Temas que alteram medidas da prévia precisam
manter seus campos coerentes: a quebra de linhas ocorre na mudança de seleção ou
publicação. Em 800×600, títulos/charters da tabela ainda podem ser abreviados;
a prévia e sua rolagem mantêm os detalhes acessíveis.

O decoder atual de `CEngine::readImage` aceita TGA. Capas PNG/JPG descobertas pela
biblioteca não são decodificadas por ele e usam o placeholder. A cadeia assíncrona
foi preservada; ampliar formatos de imagem exige outra tarefa no backend.

### Verificação desta rodada

Build RelWithDebInfo com Clang/C++17 e `GPP_NATIVE_CPU=OFF`. Pelo Native Lab MCP,
134 testes selecionados de Quick Play, menus, composição, overrides, Lua, fontes,
shaders e GUI Studio passaram. ASan/UBSan com detecção de leaks passou 68 testes
CPU de Quick Play, composição, apresentação e renderer, sem supressões. Foram
adicionados sete testes, além de adaptar os casos existentes de árvore e texto
longo: seleção versus expansão por mouse/teclado, persistência entre abas,
foco/seleção no desenho, round-trip de estilos, metadados ausentes, limite de
caminho, rolagem passiva, logo e proporção/moldura de capa.

As capturas reais usam a biblioteca local de 160 músicas, Xvfb/Mesa com OpenGL
2.0 / GLSL 1.10, assets de `build/bin` e cópias isoladas de saves/cache. Cenários:
Pastas expandida sem mudar o contexto, seleção da pasta, grupo Artista, grupo
Charter, Música geral, metadados completos, ausência de capa, caminho longo,
aba ativa com foco na busca, PgDn na prévia e F5/Ctrl+F5. O cursor também foi
conferido nas capturas, sem alterações no contrato do renderer.

- 800×600: `build/quick-play-polish-zhzgjjpq/`.
- 1920×1080: `build/quick-play-polish-5huobsry/`.
- 3440×1440: `build/quick-play-polish-9ldhp1_r/`.

Esses diretórios são evidências locais, não assets distribuídos. A primeira
tentativa em sandbox não abriu X11; uma tentativa fora dele caiu no startup.
As execuções subsequentes no Native Lab chegaram aos menus e saíram normalmente.
Não foram validados Windows, GPUs antigas, áudio audível ou latência de gameplay.
Nenhuma alteração foi feita em `CUIRendererGL2`, SQLite, SongLibrary, parsing,
descoberta de stems ou atualização incremental/completa.


O teste de capa em `build/quick-play-polish-ovfnhma5/complete-long-path.png`
(1920×1080) usa **a capa real de Generation Rock**, convertida apenas na cópia
isolada: bytes TGA no caminho indexado `album.png`. Isso exercita publicação
assíncrona, proporção, orientação e remoção do placeholder sem alterar o decoder,
o indexador nem o PNG original em `build/bin`. Não representa suporte nativo a
PNG/JPG. O teste ultrawide anterior tentou `album.tga`, que o indexador não inclui,
e portanto validou o placeholder. A mesma execução Full HD conferiu o alinhamento
mais compacto da grade de dificuldades, evitando lacunas largas em painéis grandes.

Arquivos desta rodada:

- `src/ui/QuickPlayView.{h,cpp}` e `SelectionMenus.cpp`: apresentação, árvore,
  prévia passiva e entrega das texturas de ícones pelo host.
- `src/CMenu.{h,cpp}`: cores de foco e dois pontos opt-in de interação C++.
- `src/CUIStyle.{h,cpp}`, `CUIOverrides.cpp`, `CUIDefinition.cpp`: herança e
  round-trip das duas cores opcionais no formato de tema existente.
- `tests/quick_play_tests.cpp`, `menu_renderer_tests.cpp`,
  `menu_presentation_tests.cpp`: regressões de interação/apresentação.
- `data/sprites/quick-play/`: cinco SVGs autorais, cinco TGAs e documentação.
  `CMakeLists.txt` copia os TGAs para o runtime na configuração.
- `docs/quick-play-ui.md` e `docs/gui-studio.md`: comportamento e personalização.
