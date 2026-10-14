# Validação local do Chart Studio — 2026-10-13

Ambiente: Linux, Ninja, Clang, NativeLab MCP. Testes gráficos com Xvfb e Mesa
software rendering, OpenGL 2.0 / GLSL 1.10. Arquivos comerciais permaneceram em
`build/`; nenhum MIDI/áudio/chart profissional foi acrescentado ao repositório.

## Resultados

- Build completo de jogo e studios: passou.
- Suite normal: **303/303 testes CTest passaram**.
- ASan/UBSan: **53/53 testes CTest passaram**, incluindo MIDI/runtime, CPU, CLI
  e interface. O fluxo gráfico também passou em duas repetições consecutivas.
- Núcleo novo: 27 casos GoogleTest, incluindo parser, edição, cache e transações;
  teste de runtime separado valida exportação com o loader real.
- CLI testa processos distintos, biblioteca confirmada, bloqueios, limpeza,
  colisões de arquivos, FFT de seno sintético e funcionamento sem DISPLAY.
- Interface testa mouse, edição de lane, bloqueio, regeneração, salvar/exportar,
  fechamento e reabertura. As teclas do teste são mantidas por frames separados
  para evitar pulsos artificiais menores que um frame do renderer em software.
- Configuração com todos os studios OFF: grafo Ninja sem ImGui e sem score core.
  `ldd gpp_score_tests`: SQLite e bibliotecas padrão, sem GL/GLFW/BASS.
- TTFAF abriu e fechou no workbench com ASan/UBSan, Xvfb e rendering em software;
  captura local em `build/chart-studio/ttfaf-ui.png`.

ASan/UBSan usam as supressões já existentes: somente shift-base do GLFW X11 e
alocações de duração do processo em libGLX_mesa. Não foram suprimidos diagnósticos
do editor, núcleo ou jogo. A validação não equivale a um playtest completo nem
prova ausência das corridas históricas de gameplay. Não houve build Windows nem
teste interativo completo do cancelamento de BPM do CCharter.

## Arquivos locais

| MIDI | Pista escolhida | Ataques gerados | Reuso após reabrir | .chart idêntico |
|---|---:|---:|---:|---|
| DragonForce - Through The Fire And Flames | 6 | 3183 | 100% | sim |
| Foghat - Slow Ride | 1 | 545 | 100% | sim |
| Heart - Barracuda | 3 | 846 | 100% | sim |
| Kalafina - Magia (r571207) | 1 | 1066 | 100% | sim |
| Kalafina - Magia | 1 | 1066 | 100% | sim |
| Mindflow - 01 - Destructive Device | 1 | 1554 | 100% | sim |
| Mindflow - Destructive Device | 1 | 1619 | 100% | sim |
| Mindflow - Honesty | 1 | 1546 | 100% | sim |
| Muse - Knights Of Cydonia | 4 | 2005 | 100% | sim |
| Paramore - Still Into You | 3 | 827 | 100% | sim |
| Slipknot - Before I Forget | 2 | 1124 | 100% | sim |
| Weezer - My Name Is Jonas | 6 | 662 | 100% | sim |

Cada exportação passou pelo round-trip CPU do Guitar++. Foi corrigido um problema
observado em Honesty e Still Into You: consultar o cache durante segmentação mudava
fronteiras de motivos na execução seguinte. Agora a segmentação depende somente
da música/configuração; um teste sintético cobre a estabilidade das fronteiras.

Referências locais, ExpertSingle:

| Música | Gerado | Referência |
|---|---:|---:|
| Slow Ride | 545 | 551 |
| Still Into You | 827 | 742 |
| TTFAF solo | 3183 | 3722 |
| TTFAF co-op | 3183 | 3587 |

São contagens, **não uma avaliação de qualidade**. A pista lead 6 de TTFAF não
representa necessariamente a mistura de instrumentos escolhida pelo chart GH.
Diferenças de offset são toleráveis; não foi feita correção automática nem
alegada equivalência de alinhamento. Foram analisadas janelas FFT aos 60 s dos
arquivos `song.ogg` de Paramore e `song.mp3` de TTFAF: leitura e espectro válidos.
Esses candidatos espectrais não foram usados como verdade musical.

Artefatos locais: `build/chart-studio/evaluation-v2/` contém projetos, charts,
relatórios, comparação solo/co-op e FFT. `summary.json` resume as duas execuções.
Logs de build/testes ficam em `build/chart-studio/`.

## Reprodução com sanitizadores

```sh
cmake -S . -B build/chart-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DBUILD_CHART_STUDIO=ON -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build/chart-asan --parallel 4
ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1:suppressions=$PWD/tools/gui_studio/ubsan.supp" \
LSAN_OPTIONS="suppressions=$PWD/tests/lsan.supp" \
ctest --test-dir build/chart-asan -R '^(chart_studio|midi_core\.|ChartRuntime\.|SongLoading\.)' --output-on-failure
```

No ambiente do agente, esses comandos de sanitizadores/GL foram executados via
NativeLab MCP; a configuração local reaproveitada é `build/ui-asan`.
