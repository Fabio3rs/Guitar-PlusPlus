# Síntese MIDI e stems opcionais

Chart Studio pode renderizar o MIDI musical original com **libfluidsynth 2**,
sem dispositivo de áudio, usando o alinhamento já aceito no projeto.
`convert`, `inspect`, `align` e a exportação de `.chart` não sintetizam áudio.
Nenhuma mudança de mixagem ou carregamento de stems foi feita no runtime.

## Build

```sh
cmake -S . -B build -G Ninja -DBUILD_CHART_STUDIO=ON -DBUILD_TESTING=ON -DGPP_ENABLE_MIDI_SYNTH=ON
cmake --build build --target gpp_chart_studio gpp_score_to_chart gpp_synth_tests
ctest --test-dir build -R 'chart_studio_(synth|synth_cli)' --output-on-failure
```

A opção é ON por padrão, mas a biblioteca é opcional. A detecção usa
`FluidSynth::libfluidsynth` via pacote CMake ou pkg-config `fluidsynth>=2.0`.
Ausência da biblioteca apenas desabilita renderização; `OFF` não a procura.
No Ubuntu: `libfluidsynth-dev`. FFmpeg é um executável opcional no PATH;
WAV não precisa dele. SoundFonts não são distribuídas pelo projeto: selecione
um `.sf2` local com licença adequada. Os testes de áudio usam
`GPP_TEST_SOUNDFONT` quando configurado; em Linux procuram TimGM6mb.sf2.

## CLI

```sh
build/bin/gpp_score_to_chart render-stems song.gppchartproj \
  --soundfont /path/instruments.sf2 --output-dir build/stems \
  --instruments chart --format ogg --save-config

build/bin/gpp_score_to_chart render-stems song.gppchartproj \
  --soundfont /path/instruments.sf2 --output-dir build/all --instruments all

build/bin/gpp_score_to_chart render-stems song.gppchartproj \
  --soundfont /path/instruments.sf2 --output-dir build/custom --instruments custom \
  --stem guitar:1 --stem guitar:2 --stem bass:3:4 --format wav
```

`--stem NAME:TRACK[:CHANNEL[:PROGRAM]]` é repetível: o mesmo nome agrupa
fontes. Índices são zero-based; canal/programa `-1` significa todos.
`--track N` é um atalho para uma stem `trackN`. Use `--instruments` antes das
seleções personalizadas. `chart` é o padrão inicial; configurações previamente
salvas são reutilizadas. `--instruments chart` reconstrói a seleção automática.
Fontes idênticas entre dificuldades são deduplicadas. A seleção `all` cria
uma stem por track/canal que possui notas, incluindo percussão.

Opções: `--sample-rate 44100` (8000–192000), `--quality 5` (Vorbis 0–10),
`--gain 0.5` (0–2), `--tail 2` (0–30 segundos). Não há normalização automática.
O ganho pode causar clipping: reduza-o quando necessário.
`--save-config` salva as escolhas no projeto; os demais usos não o modificam.
O campo opcional `synth` preserva os campos históricos do documento e
projetos anteriores continuam abrindo com os valores padrão.
SoundFont e diretório são relativos ao projeto quando possível.

## GUI

Abra **Panels → MIDI Synth**. Use os botões para carregar as fontes do chart
ou todas as fontes, desmarque instrumentos, acrescente stems e suas fontes,
escolha o SoundFont, diretório, formato e parâmetros. Os índices e nomes das
tracks estão no painel expansível. Os campos de caminhos aceitam caminhos
completos; integração desses campos com o navegador de arquivos é futura.
**Render selected stems** é a ação explícita que inicia o job. A barra mostra
progresso e **Cancel render** solicita cancelamento. A inicialização/carregamento
do SoundFont é uma chamada síncrona do worker e pode atrasar o cancelamento.
Salve o projeto para persistir a configuração. Sem FluidSynth o painel explica
a indisponibilidade e não oferece controles de renderização.

## Tempo e expressão

O leitor existente valida o SMF e midifile lê os eventos originais. O scheduler
usa `round(audioSeconds(state, tick) * sampleRate)` para Note On/Off, Program
Change, CC (incluindo sustain e seleção de banco), pitch bend e pressões de
canal/tecla. Empates respeitam tick, ordem de track e ordem original dos eventos.
A escala temporal altera intervalos entre eventos e durações das notas, não a
frequência das amostras. Envelopes, modulações e efeitos do SoundFont continuam
em segundos físicos; não são time-stretched. FFmpeg apenas codifica Vorbis.

Offset positivo gera silêncio inicial. Offset negativo renderiza também o
pré-roll e descarta samples anteriores ao zero, preservando estado de notas,
pedal e efeitos. Todas as stems terminam no mesmo instante: fim global do MIDI
transformado (limitado a zero), mais a cauda configurada. No fim musical,
liberam-se pedal e notas restantes para renderizar a cauda. A duração não é
forçada à duração da gravação real. Nenhuma nova análise de alinhamento ocorre.

O scheduler tem precisão de sample. O ataque perceptível depende do SoundFont
e do processamento interno em blocos do FluidSynth; não confundir início da
energia sonora com posição do evento. A biblioteca pode introduzir uma
quantização interna de bloco (tipicamente 64 samples). Os testes de tempo
verificam o agendamento separadamente da forma de onda.

Cada stem usa um sintetizador novo; cada par track/canal usa um canal virtual
separado. Controles de tracks com notas permanecem isolados. Tracks sem notas
naquele canal funcionam como trilhas de controle compartilhadas. Isso atende
exports usuais de tablaturas, mas MIDI com roteamento compartilhado incomum
pode exigir uma política adicional de roteamento. Mesclas de chart incluem
as fontes MIDI completas de ambas as pistas: **recortes de gameplay não são
recortes de áudio**. Ajuste a seleção custom quando necessário.

## Proteção e limites

Saída intermediária WAV PCM16 estéreo em diretório temporário exclusivo;
Ogg usa `ffmpeg` com argumentos separados (`posix_spawnp`/`CreateProcessW`),
sem shell. Só publica após todas as renderizações e codificações concluírem.
Falha/cancelamento remove temporários. Colisões de destino são recusadas:
use novo nome/diretório. `song.wav`/`song.ogg` são reservados; áudio original
nunca é substituído. Publicação usa hard links no mesmo filesystem (NTFS em
Windows); filesystems sem hard links não são suportados nesta versão.

Não implementados: SysEx/GS/XG e mensagens de sistema expressivas, exportação
conjunta com `convert`, prévia/mixagem de stems, recortes de áudio equivalentes
à mesclagem do chart, RF64, MIDI 2.0 ou MPE. Mesmo pitch simultâneo no mesmo
track/canal segue a semântica monofônica por tecla do FluidSynth (um novo
Note On libera a voz anterior); tracks diferentes não roubam essas vozes.
No máximo 256 pares track/canal por stem e 500 milhões de frames por arquivo.

## Windows e CI

CI Linux instala FluidSynth, FFmpeg e TimGM6mb para testes offline e repete
build/testes com síntese OFF. CI Windows explicita OFF, mantendo o fluxo
existente. O processo externo tem implementação Windows Unicode e quoting
por argumento. Integração FluidSynth ON, SoundFonts com caminhos Unicode,
execução FFmpeg e distribuição das DLLs/transitivas ainda precisam de
validação real no Windows. Distribuidores devem incluir as dependências da
biblioteca habilitada ou compilar com OFF; SoundFonts/FFmpeg não são baixados
pelo CMake nem incluídos automaticamente.

Referências de API: [renderização sem driver](https://www.fluidsynth.org/api/UsingSynth.html)
e [mensagens MIDI](https://www.fluidsynth.org/api/group__midi__messages.html).

### Validação local (2026-10-16)

Linux/FluidSynth 2.3.4: testes de CPU, CLI e round-trip existentes passaram,
assim como testes de WAV/Ogg, seleção, controle MIDI, tempo, pré-roll,
cancelamento e encoder ausente/com falha. Build OFF e CLI tradicional também
passaram. ASan/UBSan/LSan no NativeLab exigiu
`LSAN_OPTIONS=suppressions=tools/chart_studio/lsan_synth.supp` para registros de
tipos da libinstpatch do sistema; não há supressões de código do studio.
O log sem filtro fica em `build/chart-studio/synth-asan-unfiltered.log`.

Exemplo local: `build/chart-studio/synth-example-silent/{guitar,bass}.ogg`,
renderizado do projeto de Silent Screams com offset +23.62 s e escala 1,
44.1 kHz estéreo, duração comum de 305.175283 s (incluindo cauda de 2 s).
Nenhum arquivo em `songs` foi modificado. Não houve avaliação auditiva.

A ação de renderizar na GUI também passou sob Xvfb/Mesa no NativeLab, inclusive
com ASan/UBSan/LSan. Nesse teste gráfico foram usadas adicionalmente as
supressões existentes de Mesa/GLFW (`tests/lsan.supp` e a supressão local de
`shift-base` em `glfw/src/x11_window.c`). O teste antigo de UI teve uma falha
intermitente na automação de redo; passou na repetição sem alteração no teste.
