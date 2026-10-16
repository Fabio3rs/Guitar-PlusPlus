# Layout e ownership de gameplay — 15/10/2026

## Resultado e escopo

A mudança separa caches/efeitos do estado autoritativo de `CPlayer`, concentra o estado quente de input/julgamento/score/Plus e torna a referência ao jogador no caminho de render somente leitura. Os caches continuam mutáveis. **Não constitui uma solução geral das races de gameplay, nem prova de ganho de FPS.**

O baseline é o workspace local de `a7d0ef0`, incluindo a correção não commitada que separa `BPMNowUpdate` de `BPMNowBuffer`. Não foi usado um checkout limpo de master. O SVG não rastreado em `assets/logos/g++.svg` foi preservado. Não houve reset/stash/commit/push.

Artefatos grandes, fontes/binários preservados, dumps e logs estão em `build/layout-study/` (ignorado pelo Git). Uma queda de energia apagou os primeiros dumps em `/tmp`; eles foram regenerados antes da reorganização. Os binários `baseline/normal`, `baseline/instrumented`, `after/normal` e `after/instrumented` são independentes do build principal. `binary-hashes.txt` identifica os executáveis medidos. As compilações de sanitizer usam diretórios próprios.

## Ownership e alterações

O [inventário por membro](ownership.md) cobre os 75 membros originais de CPlayer, 34 de CGamePlay e 60 de GPPGame, mais o novo estado visual. Estáticos, como `playersMutex`, não ocupam esses layouts.

- `PlayerRenderState` contém tails, índice visual de BPM, ângulo, cinco identificadores de buffers, partículas e lightning. Fica embutido depois dos dados frios; não introduz uma alocação por frame nem um ponteiro extra. `tailsData` mantém `reserve(32)` e capacidade entre frames/resets.
- `renderPlayer(const CPlayer&, PlayerRenderState&)` e os helpers que percorrem o jogador não escrevem score, notas, input ou Plus. O renderer, seus buffers e o estado visual continuam mutáveis. `const` não publica um snapshot nem elimina leituras concorrentes de campos não atômicos.
- `updatedMusicRunningTime` não tinha leitores e foi removido. O relógio usado por posições/julgamento não mudou.
- `rangle` usa o mesmo relógio GLFW e velocidade de 400 graus/s, agora amostrado uma vez por render, com quantização de um grau. `floor(fmod(...))` evita também o overflow do antigo cast para int após longa execução. O menu de pause normal não redesenha o gameplay; na volta, ambos os caminhos usam o relógio corrente.
- `bPlusStrike` expira no update depois de quatro segundos. O render limita a luz ao intervalo visual mesmo se o update estiver pausado/inativo; lightning mantém seu próprio limite de 0,85 s. O timestamp continua em tempo de engine, não em tempo musical compensado pelo pause, como antes. A produção da recompensa e a lógica de frases não mudaram.
- A escrita em `Notes.BPMMinPosition` foi retirada de `drawBPMLines`: não existia consumidor do valor visual. O membro foi mantido para editor/dump offline. A sobrecarga const de `getChartEnd` consulta sem preencher `Notes.chartEnd`; a sobrecarga original conserva o cache e seu comportamento, inclusive quando o valor já estava preenchido.
- A correção local de `BPMNowUpdate`, inclusive seus resets, foi preservada. Os resets de `BPMNowBuffer` foram migrados junto com o campo, inclusive no editor. Não há volta ao índice compartilhado entre julgamento e tarefa BPM.
- CGamePlay mantém seu layout: buffers visuais já estão juntos e contadores de update ficam ao final. Foi documentado o ownership dos buffers e dos contadores. GPPGame foi revisado, mas não reorganizado: não houve evidência suficiente de benefício. Sua prioridade foi reduzida conforme orientação durante a tarefa.

`loadThread` continua usando `playersMutex`. No código real deste workspace, `loadMarathonThread` prepara jogadores separados; o main mantém a exclusão de sessão e instala `prepared` após publicação de `processing`. Isso difere da descrição histórica do AGENTS.md. O handoff, mutex, protocolo pauseRequest/pauseAck e callbacks MP Windows não foram alterados. CPlayer já não era copiável por causa dos atomics; essa propriedade permanece. Inicialização é por valores default/corpo do construtor, sem listas dependentes da ordem antiga. Serialização continua gravando os mesmos campos de player/chart. Nenhuma interface Lua ou API OpenGL foi substituída.

## Layout e cache lines

AMD Ryzen 9 5900X, x86-64, cache line de 64 bytes confirmada em sysfs. Compilador Clang 21, C++17, libstdc++; alinhamento natural de oito bytes nas classes analisadas.

| Tipo | Antes | Depois | Alinhamento | Padding direto antes/depois |
|---|---:|---:|---:|---:|
| CPlayer | 1.680 | 1.672 | 8 | 32 / 21 |
| CGamePlay | 704 | 704 | 8 | 10 / 10 |
| GPPGame | 2.056 | 2.056 | 8 | 33 / 33 |
| PlayerRenderState | — | 272, incluídos em CPlayer | 8 | — / 11 |

O padding do struct visual está dentro de seu tamanho e não é contado novamente como padding direto de CPlayer. A redução total é de oito bytes por jogador, sem padding de cache line introduzido.

Os mapas completos [antes](layout-before.md) e [depois](layout-after.md) mostram offset, tamanho, intervalo de bytes e linhas relativas para **cada membro**. `tools/report_gameplay_layout.py` combina `-fdump-record-layouts`, `-Wpadded` e tamanhos/offsets DWARF via GDB. `pahole` também foi executado: nesta versão não resolve vários tipos aninhados e imprime tamanhos zero/falsos holes; esses números não foram usados como padding. GDB chegou a inferir alignof 16 para GPPGame, contradizendo sizeof 2056; o alinhamento foi confirmado pelo dump completo do compilador (8).

Antes, `tailsData` em 416–439 ficava junto de `BPMNowUpdate` em 408–415; `updatedMusicRunningTime`/`BPMNowBuffer` dividiam a região 448–511 com input e tempo. Agora os campos diretos quentes ocupam 0–311, Notes começa em 312, e o estado visual começa em 1400. Sua separação dos campos quentes não depende de a alocação começar num múltiplo de 64. Cabeçalhos STL têm a disposição descrita, mas as notas, partículas e vértices apontados possuem alocações próprias.

Não foi adicionado `alignas(64)` nem `hardware_destructive_interference_size`. Também não se experimentou padding indiscriminado: a evidência não o justificava. Em CGamePlay continuam candidatos locais: a linha relativa zero mistura o fade visual com relógio/velocidades lidos pelo update; BPMl e fretboardLData atravessam a mesma linha relativa durante a tarefa assíncrona. Não se atribui contenção medida a essas regiões sem correlação de endereço/instrução.

## Experimentos reproduzíveis

Os [valores e variabilidade](measurements.md) e [amostras por execução](measurements.csv) são as medições finais. Os diretórios `timing`, `normal-runs` e `paired-*` em build contêm pilotos, **não** usados como evidência final; o reset parcial do bot após aquecimento foi corrigido antes de `final-*`.

Builds comparados: `-O2 -g -DNDEBUG`, `GPP_NATIVE_CPU=OFF`. Mesmo harness, flags, CPU, afinidade `2,3`, assets de `build/bin`, Xvfb/Mesa software e `LP_NUM_THREADS=1`. O host não fornece DISPLAY ao Native Lab. Não houve mudança de governor, segurança global ou instalação de ferramentas. Frequência/térmica e processos externos não foram rigidamente controlados.

`tools/compare_gameplay_benchmarks.py` alterna A/B e B/A por repetição, com cinco repetições de três segundos para quatro cenários. Cada execução aquece 100 frames e reinicia estado de julgamento/score/Plus, mantendo o chart alocado. São 20.000 notas/jogador, espaçadas por 25 ms, com acordes, HOPOs, abertas, sustains e frases Plus; bot, modelos reais carregados e HUD ativado. Não é uma reprodução de música licenciada nem uma simulação determinística de input físico. Pontuações dependem da cadência e não são usadas como teste de equivalência funcional.

Código dos casos: `jogadores-threaded-BPM-delay_ms-HUD`. Os casos finais são `1-0-1-0-1`, `1-1-0-0-1`, `2-1-1-0-1` e `2-1-1-50-1`. O caminho sequencial exercita update+render; o concorrente mantém worker independente e sleep solicitado de 200 µs. O benchmark antigo `GPPGame::benchmark()` depende de TTFAF, janela/manual ESC e execução sequencial; não foi suficiente para este estudo.

`final-perf` usa binários **sem instrumentação interna**. Um protocolo FIFO liga/desliga perf somente nas fronteiras, fora dos loops: carregamento/aquecimento/teardown não entram nos contadores. Foram obtidos cycles, instructions, IPC, L1 loads/misses, cache-misses genérico, context switches, migrations e task-clock. LLC-loads/misses genéricos não são suportados nesta PMU/configuração. Não se converte cache-misses em LLC-misses. Os contadores agregam render, update, driver e tarefas auxiliares: dividir ciclos por updates normaliza trabalho, mas **não mede ciclos exclusivos de CGamePlay::update**.

`final-timing` usa instrumentação opcional `GPP_PROFILE_GAMEPLAY=ON`, com buffers thread_local limitados a 65.536 amostras/métrica (~3 MiB/thread), sem alocação/log durante os loops. Mede update, updatePlayer, render, renderPlayer, intervalo entre updates e aquisição de playersMutex; médias, medianas, p95, p99, máximos e descartes são exportados ao parar. Ela muda código e tráfego de memória: suas durações não foram misturadas com os contadores dos binários normais. O build principal fica com a opção OFF. LLVM fprofile não foi usado para inferir locality; instrumentação de contadores PGO também perturbaria o experimento, e não substituiria a distribuição de latências.

A captura fornecida pelo usuário (`gpp-record-20261015-124608-1116856.data`) tem 6.005 amostras de `cache-misses:u`, sem lost samples, dois bots, BPM ligado e aproximadamente 1.200–2.000 FPS reportados. Na agregação por DSO: 62,98% Gallium, 8,26% libc, 5,80% GuitarPP e 13,54% sem resolução. Participação própria ponderada no evento cache-misses de updatePlayer 0,39%, renderPlayer 0,40% (não percentuais de tempo de CPU). `perf annotate` encontrou apenas cinco amostras diretas em updatePlayer. Isso não permite inferir false sharing de membros nem um ganho no jogo real a partir do benchmark software.

## Interpretação dos resultados finais

As [tabelas completas](measurements.md) e [amostras tabuladas](measurements.csv) distinguem as duas séries. Foram concluídas 80 execuções finais (40 normais com perf, 40 instrumentadas), em ordem A/B alternada. Pilotos anteriores com reset parcial do bot ou espera incorreta pelo PID do wrapper Native Lab foram excluídos. Os executáveis finais foram preservados antes das alterações cosméticas de formatação.

No cenário concorrente com dois bots, BPM e HUD, os binários normais passaram de 3.693 para 3.717 updates/s (+0,64%), 37,52 para 37,90 frames/s (+1,01%), IPC 2,395 para 2,417 e 691.500 para 666.000 cache-misses genéricos/frame (-3,68%). L1 misses/frame aumentaram 0,78%. Esses números incluem o driver de renderização por software. Não demonstram melhoria no jogo com GPU real: intervalos exploratórios pareados de 95% (Student, cinco pares) para FPS e cache-misses incluem zero; para cache-misses, aproximadamente -7,46% a +0,24% da diferença relativa por par.

Na série instrumentada correspondente, update médio foi de 4,097 para 4,046 µs e a média dos p99 por execução foi de 8,078 para 8,350 µs. O intervalo exploratório de 95% da diferença pareada do p99 é [-0,488; +1,032] µs. Render médio foi de 10,87 para 11,14 ms. Com atraso visual de 50 ms, o update médio foi de 4,326 para 4,187 µs e p99 de 8,306 para 8,466 µs. No caso de um jogador concorrente, p99 passou de 4,350 para 4,690 µs; a diferença também é inconclusiva. Esses intervalos pequenos não são garantia de equivalência em outras CPUs/GPUs ou de ausência de regressão rara.

Os máximos observados de update entre as cinco execuções foram 31,89 → 38,33 µs (dois bots rápidos) e 491,98 → 182,51 µs (render lento); máximos isolados são sensíveis ao scheduler. Não foi assumida frequência fixa de update. O caso sequencial normal variou -2,38% em FPS, com desvio padrão posterior de 5,61 FPS sobre média 67,02, também inconclusivo.

A refatoração é mantida pelo ownership mais claro, pela remoção de escritas autoritativas do render e pelo agrupamento físico sem crescimento do objeto. **Não se reivindica ganho estatisticamente convincente de performance.** Não foi introduzido alignas: não há evidência de que o padding adicional compensaria. Não foram alterados algoritmos de notas, a criação de tarefas async ou o escalonamento do worker para tentar melhorar artificialmente estas medições.

## Diagnósticos e limites funcionais

Native Lab foi descoberto e usado para executar processos, sanitizers, perf e ptrace. Antes da queda, perf_event_paranoid era 4; após reinício foi observado 1. O wrapper `/usr/bin/perf` não encontrava o pacote do kernel. Foi usado o binário indicado pelo usuário: `/usr/lib/linux-tools/6.14.0-37-generic/perf`. Não foi alterada a política do host.

- CTest normal: 296 testes passaram, incluindo três regressões novas (isolamento/capacidade de caches, leitura de chartEnd sem escrita, expiração de Plus sem render e independência do índice BPM).
- ASan+UBSan em builds separados antes/depois, com símbolos e frame pointers: 56 testes selecionados no baseline e 59 depois passaram com a supressão existente `tests/lsan.supp`. Incluem charts/carregamento, shaders GL2, partículas e Plus. O primeiro build tentou descoberta sob o sandbox e LSan recusou ptrace; a etapa foi repetida pelo Native Lab.
- LSan sem supressão no baseline reportou 111.160 bytes/515 alocações no cenário gráfico. A supressão estreita de Mesa elimina os leaks de GLX; numa execução baseline restaram 3.036 bytes/34 alocações com stacks PipeWire/ALSA/BASS. Os logs originais foram conservados. Não há alegação de correção desses leaks externos pela mudança de layout.
- TSan em build próprio: 19 avisos no cenário baseline e 15 depois. Races em flags de notas, frets, sustains, aprovação e combo permanecem. O acesso concorrente ao antigo rangle foi reproduzido antes e deixou de existir no código novo. A contagem de warnings varia por execução; não significa que exatamente quatro races tenham sido resolvidas. Nenhuma supressão global de TSan foi adicionada.
- Não se executou MSan: GLFW/GL/BASS/driver não são inteiramente instrumentados. Valgrind, bpftrace, pahole, readelf, GDB, strace e ferramentas LLVM estão disponíveis; hyperfine e numactl não foram encontrados. PMU foi preferida a simular o driver inteiro em Cachegrind.


`perf c2c` e `perf mem` foram executados separadamente sobre os binários normais antes/depois. c2c registrou 6.446/5.288 registros e 873/828 loads; reportou zero HITM local/remoto e zero linhas compartilhadas identificadas em ambos. A captura posterior teve 3.932 registros sem data source interpretável. Isso limita a análise nesta combinação Zen 3/kernel/perf: **zero HITM reportado não prova ausência de false sharing**. Não foi possível correlacionar contenção mensurável aos offsets de membros. perf mem mostrou acessos em updatePlayer/CPlayer e bibliotecas, mas não estabeleceu uma redução de contenção por campo.

Uma captura adicional de dois segundos por TID, sem instrumentação interna, encontrou 3,821 bilhões de ciclos no main, 4,830 bilhões no llvmpipe e 130,55 milhões no worker identificado pelo ciclo de vida do harness. Cache-misses foram respectivamente 21,86 milhões, 26,23 milhões e 1,269 milhão; IPC 2,56, 2,23 e 3,06. É diagnóstico posterior isolado, sem afinidade fixada, não comparação A/B nem cycles/update exclusivo. O attach com --no-inherit não inclui todas as tarefas transitórias futuras. Arquivos per-thread.csv/per-thread-list.txt conservam os TIDs.

GDB/ptrace capturou stacks do render e de workers Gallium em espera; uma segunda captura após aquecimento encontrou o worker de gameplay em clock_nanosleep, chamado pelo sleep_for de 200 µs do harness. Ambos os processos terminaram normalmente. strace restrito a futex/clock_nanosleep/clone/clone3 registrou 5.670 chamadas, incluindo 2.143 futex e 272 clone3. Os 95,02% em futex são fração do tempo das syscalls rastreadas, não utilização de CPU do jogo nem evidência de contenção de playersMutex. A criação repetida de tarefas BPM continua sendo oportunidade separada. Uma tentativa inicial de strace completo ficou esperando descendentes e foi interrompida; a captura restrita é a utilizada. perf sched foi bloqueado por permissão de leitura de sched:sched_switch em tracefs; perf lock não encontrou lock:contention_begin habilitado. Não se alteraram permissões globais. Nenhum desses tempos de diagnóstico entrou nas comparações de desempenho.

Const não corrige a publicação dos dados. Persistem as races update/render e as consultas GLFW pelo worker, inclusive a tecla de ativação de Plus. O benchmark com bot não valida entrega de input humano, foco/dispositivos, áudio sincronizado ou unplug/replug. CTest cobre aspectos de sustains, score, pause e handoff; não substitui playtest de música/marathon/multiplayer Windows, taps ou visuais em GPU real. A arquitetura independente, a ordem de julgamento, os clocks, os flags bitwise e os formatos de saves/charts foram preservados. Nenhum mutex global envolvendo render foi introduzido.

## Como repetir manualmente

```sh
cmake -S . -B build -DBUILD_GAMEPLAY_BENCHMARK=ON -DGPP_PROFILE_GAMEPLAY=OFF -DGPP_NATIVE_CPU=OFF
cmake --build build --parallel
# Executar com os assets presentes; args: segundos jogadores threaded BPM atraso_ms HUD
(cd build/bin && ./gpp_benchmark_gameplay 10 2 1 1 0 1)

export PERF_BIN=/usr/lib/linux-tools/6.14.0-37-generic/perf
# Abrir GuitarPP normalmente, reproduzir o mesmo chart/bots/configuração e então:
(cd build/bin && ../../tools/profile_gameplay.sh stat 30)
(cd build/bin && PERF_EVENT=cache-misses:u ../../tools/profile_gameplay.sh record 30)
(cd build/bin && ../../tools/profile_gameplay.sh c2c 30)
# TID=... seleciona uma thread em vez de todo o processo.
```

`tools/profile_gameplay.sh` também suporta mem, sched e stacks, registra IDs de build/threads/eventos e respeita PERF_BIN/PID/TID/OUT_DIR. Não modifica o helper local `build/bin/gpp-perf.sh`. Não comparar uma versão em GPU real com outra sob software, nem uma instrumentada com outra normal. Para mapas atualizados: `python3 tools/report_gameplay_layout.py build build/layout-study/layout-new`.
