# Guitar++ Chart Studio

Workbench independente para importar **MIDI musical**, escolher uma pista, gerar
cinco lanes, corrigir frases e guardar decisões em SQLite. O objetivo musical é
um chart razoável e consistente, com estilo próprio; igualdade com lanes de um
chart profissional não é uma medida de qualidade.

## Build e execução

Requer as dependências habituais do jogo, ImGui existente e libpng. Para os testes
de interface em Linux, instale Xvfb e xdotool. `BUILD_CHART_STUDIO` fica OFF por padrão.

```sh
cmake -S . -B build -G Ninja -DBUILD_CHART_STUDIO=ON -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --parallel 4
./build/bin/gpp_chart_studio
./build/bin/gpp_chart_studio "build/tabs/Paramore - Still Into You.mid" --track 3
ctest --test-dir build -R '^chart_studio' --output-on-failure
```

A GUI usa GLFW/OpenGL 2.0 e GLSL 1.10, com configuração própria em
`chart-studio/layout.ini`; `--output-dir DIR` altera esse diretório. Não precisa
abrir o jogo, inicializar `GPPGame` ou carregar seus modelos/texturas. A CLI não
cria janela nem contexto GL. `gpp_score_tests` nem sequer linka bibliotecas gráficas.

## Fluxo de uso

1. Abra MIDI ou `.gppchartproj` pelo campo de caminho. Escolha explicitamente a
   pista e, para Type 0 ambíguo, canal/programa. Índices são **zero-based**.
2. Inspecione os detalhes da pista. Generate usa beam search determinística;
   a heurística gulosa está disponível para comparação. Ajuste os seis pesos.
3. Na timeline, roda do mouse faz zoom e arrasto com botão central navega.
   Clique no ataque para destacar as fontes musicais. Arrasto move a nota;
   Shift+arrasto ajusta sustain. Ctrl+clique adiciona à seleção.
4. Teclas 1–5 alteram lanes da nota selecionada. Use o painel para ticks/sustain,
   copiar/colar, inserir/excluir, confirmar, bloquear, desbloquear ou recalcular.
   “Apply to equivalent occurrences” torna a correção compartilhada explicitamente.
5. Ctrl+L bloqueia a frase; Ctrl+G regenera preservando intervenções manuais e
   bloqueios. Ctrl+Z/Y desfaz/refaz; Ctrl+S salva; Ctrl+E exporta; Ctrl+Q fecha.
6. Carregue referência `.chart`, `.gpp` ou MIDI de **gameplay** separadamente.
   A referência é somente leitura e usa a mesma escala temporal.
7. Áudio é opcional. Play/Pause/Stop usam BASS quando carregado e relógio monotônico
   sem áudio. Loop usa os limites em ticks do painel. O offset global é expresso
   em milissegundos; marcas manuais são apenas referências visuais.

A inspeção FFT reutiliza `gpp::audio::analyze` do Audio Studio. O botão analisa
uma janela de 200 ms na posição de playback, FFT de 8192 amostras, sem sintetizar
áudio. Candidatos harmônicos numa mixagem não identificam isoladamente guitarra,
e harmônicos podem parecer outras notas. Não modifica BPM, offset nem lanes.

```sh
./build/bin/gpp_score_to_chart fft "build/bin/data/songs/Paramore - Still Into You/song.ogg" \
  --at 60 --output build/chart-studio/paramore-fft.json
```

Diferenças de offset entre MIDI e gravação são toleradas nesta fase. Alinhamento
automático, DTW e inferência de instrumentos a partir da mixagem ficam para depois.

## CLI e memória persistente

```sh
./build/bin/gpp_score_to_chart inspect "build/tabs/Paramore - Still Into You.mid"
./build/bin/gpp_score_to_chart convert "build/tabs/Paramore - Still Into You.mid" \
  --track 3 --output build/chart-studio/Paramore.chart \
  --project build/chart-studio/Paramore.gppchartproj \
  --report build/chart-studio/Paramore.json

# Outro processo: abre o projeto, consulta decisões anteriores e valida exportação.
./build/bin/gpp_score_to_chart convert "build/tabs/Paramore - Still Into You.mid" \
  --track 3 --output build/chart-studio/Paramore.chart \
  --project build/chart-studio/Paramore.gppchartproj --replace

./build/bin/gpp_score_to_chart patterns build/chart-studio/Paramore.gppchartproj list
./build/bin/gpp_score_to_chart patterns build/chart-studio/Paramore.gppchartproj lock --event 1
./build/bin/gpp_score_to_chart patterns build/chart-studio/Paramore.gppchartproj promote \
  --event 1 --cache build/chart-studio/patterns.sqlite
```

`--cache` é a biblioteca reutilizável; o projeto já tem seu próprio cache.
A biblioteca recebe **somente promoção explícita de padrões confirmados/bloqueados**,
nunca arquivos MIDI completos. Alterar pesos recalcula experimentos, preservando
confirmados e intervenções. `clear-experimental` não remove confirmados/bloqueados.
`--no-cache` desativa reutilização experimental/global, não a proteção manual.
Use `--algorithm heuristic|beam`, `--difficulty easy|medium|hard|expert`,
`--offset-ms`, `--channel`, `--program`, `--max-chord`, `--density`,
`--minimum-sustain`, `--weight0`…`--weight5`, `--begin-tick`/`--end-tick`.

Pesos, na ordem: contorno melódico, distância, ergonomia, complexidade dos acordes,
consistência de notas repetidas e repetição de lane. Preservação de frase conhecida
é uma restrição de precedência, não uma pequena penalidade de custo.

Assinaturas canônicas `signature-v1` incluem dificuldade, famílias de programas,
conjuntos de pitches e durações de seus componentes, ritmos/durações como razões inteiras de PPQ.
Correspondência estrutural aceita transposição estrita, não similaridade vaga.
XXH128 identifica padrões, mas a representação canônica inteira decide equivalência.
Segmentação não depende do conteúdo do cache: adicionar memória não muda as
fronteiras das frases já analisadas. Repetições recebem um padrão compartilhado
na primeira geração; decisões novas passam por três ajustes limitados de fronteiras.

Prioridade: bloqueios, confirmações do projeto, decisões estabelecidas no projeto,
biblioteca exata, biblioteca estrutural, otimizador. Empates usam ordem canônica e
máscaras lexicográficas. Conflitos são relatados, não resolvidos apagando bloqueios.
Uma edição isolada é marcada como variante manual; propagação exige ação explícita.

Projetos e biblioteca têm identidade/schema SQLite versionados. O projeto guarda
score, eventos originais/editados, parâmetros, decisões e referências externas;
undo/redo da sessão fica em memória, com limite de 64 estados. Escrita de projeto
usa temporário e publicação atômica; promoção usa transação. Arquivo alterado por
outro processo exige Save As. Arquivos ausentes permitem abrir o estado salvo.
Nenhuma alteração no formato histórico `.gpp`.

## Arquitetura

- `tools/chart_studio/core.*`: score musical, importador midifile, tempo,
  assinaturas canônicas, validação e relatórios.
- `mapping.cpp`: ataques, frases, perfis, heurística/beam, precedência e conflitos.
- `persistence.cpp`: projetos e biblioteca SQLite; `session.cpp`: edição e histórico.
- `export.cpp`: `.chart`; `runtime.*`: adaptador CPU de leitura/round-trip por
  `CChart`/`CMidiChart`, comparação; linka efetivamente `GuitarPP_lib`.
- `cli.cpp` e `ui.cpp`: frontends do mesmo núcleo. Apenas GUI linka ImGui.
- `src/charter/Document.*`: editor genérico com IDs estáveis usado pelo CCharter
  e pela sessão; sem ImGui, SQLite ou dependência do studio.
- `src/MidiInput.*`: validação SMF compartilhada pelos dois importadores.

O CCharter recebe `GPPGame&` explicitamente. Seu documento pode alimentar um futuro
adaptador 3D; não foi extraído o renderer de CGamePlay. A prévia do workbench é 2D,
pois o highway atual depende de recursos e estados globais de gameplay.

No CCharter, ponteiros/iteradores persistentes foram substituídos por IDs;
navegação integra mudanças de BPM; undo/redo opera sobre o documento. A análise
BPM agora é explícita (B inicia, A aplica), em stream decode próprio, cancelável
entre blocos de 10 segundos e joined no destrutor. Prévia cria player e streams
próprios em vez de copiar ownership de áudio. Flags persistentes e frases Plus são
preservadas. Não foram redesenhadas as threads de gameplay nem corrigidas todas
as corridas históricas entre update/render.

## Exportação e avaliação

Exportação conserva PPQ, offset, mudanças de BPM e compasso. Agrupa lanes no mesmo
ataque; escolhe o menor sustain dos componentes do acorde, conservando durações
originais no projeto e registrando perdas no relatório. Limite mínimo de sustain
é política explícita de exportação. BPM é arredondado para milésimos exigidos pelo
`.chart`; round-trip compara tempos/duração contra esse mapa, tolerância 1 µs.

```sh
./build/bin/gpp_score_to_chart compare build/chart-studio/Paramore.chart \
  "build/bin/data/songs/Paramore - Still Into You/notes.mid" \
  --output build/chart-studio/Paramore-comparison.json
```

Comparação usa janelas de 10 s, ataques, formas/quantidade de acordes e sustains;
aceita offsets independentes e instrumento de referência. A timeline permite
inspecionar distribuição temporal. Não fornece uma nota de qualidade baseada
em igualdade de lanes. No TTFAF, compare também a versão co-op: a referência GH
pode trocar/misturar instrumentos, diferentemente da pista selecionada no MIDI.

## Limitações e próximos experimentos

- Expert é o foco. Outros perfis usam retenção periódica e restrições de lanes/
  acordes; relevância musical da simplificação ainda precisa evoluir.
- Motivos repetidos são procurados entre 4 e 32 ataques; passagens únicas usam
  até oito ataques/quatro semínimas. Não há segmentação semântica de riffs,
  tolerância rítmica, fusão automática de instrumentos nem otimização global exata.
- FFT é inspeção de janela, não separação de fontes, transcrição ou alinhador.
- Prévia 3D, edição de tempo pela GUI, avaliação perceptual automatizada e métricas
  avançadas de repetição/continuidade entre charts ainda estão pendentes.
- Windows tem caminhos UTF-8 e publicação via MoveFileExW, mas esta entrega foi
  compilada/testada em Linux. A GUI usa campos de caminho em vez de seletor nativo.
- Geração e análise FFT são síncronas na GUI; canções grandes podem pausar a janela.
  Undo copia estados completos (até 64), adequado ao protótipo, com custo de memória.

Próximos experimentos: ouvir/jogar pequenos trechos corrigidos, promover apenas
riffs aprovados, comparar lead/rhythm separadamente no TTFAF/co-op, ajustar pesos
com bibliotecas separadas e avaliar densidade após alinhamento manual. Arquivos
comerciais locais não fazem parte dos testes nem devem ser adicionados ao Git.

## Revisão dos sustains e comparação com Slow Ride

A política padrão passou a conservar sustains **somente acima de meia batida**
(semínima). O limite é ajustável em batidas na GUI, em “Musical sustain cutoff”,
e na CLI:

```sh
./build/bin/gpp_score_to_chart convert "build/tabs/Foghat - Slow Ride.mid" \
  --track 1 --sustain-beats 0.5 --output build/chart-studio/SlowRide.review.chart
```

`--sustain-beats 1` exige mais de uma batida; `--minimum-sustain auto` seleciona
meia batida; `--minimum-sustain 0` conserva todas as durações positivas. O limite
alternativo em segundos continua disponível. Não combine os dois argumentos.
Projetos antigos mantêm sua política salva: ative o controle em batidas para
aplicar o corte ao reexportar. Não é preciso regenerar lanes nem limpar o cache.

A comparação é feita diretamente entre duração em ticks e limite em ticks,
com corte inclusivo na fronteira: duração igual ao limite não é sustain. Isso
impede que erro de arredondamento em segundos retenha notas exatamente no limite.
Mudar BPM/offset não muda essa decisão. Um sustain longo não é removido só porque
outra nota começa perto dele. A timeline de cinco lanes e a comparação do resultado
gerado mostram a mesma duração que será exportada; o piano roll e o projeto
conservam as durações musicais completas. O relatório informa `short_sustains_removed`.

Para compatibilidade com projetos existentes, valores negativos do campo interno
`minimumSustain` representam o limite em batidas pelo seu módulo. Valores não
negativos continuam em segundos. Não houve alteração da estrutura serializada.

A comparação detalhada, os problemas encontrados e os resultados das ferramentas
estão em [chart-studio-review.md](chart-studio-review.md).
