# Alinhamento de MIDI musical e áudio

O Chart Studio compartilha um alinhador determinístico entre GUI e CLI. Ele
compara descritores harmônicos das pistas melódicas do MIDI com o áudio indicado;
não sintetiza MIDI, não aprende lanes e não usa charts profissionais como alvo.
O runtime não recebe dependências de ImGui nem mudanças em seus relógios.

## Uso

Compile os mesmos alvos do studio, sem novas dependências:

```sh
cmake -S . -B build -G Ninja -DBUILD_CHART_STUDIO=ON -DBUILD_TESTING=ON
cmake --build build --target gpp_score_to_chart gpp_chart_studio

build/bin/gpp_score_to_chart align "build/tabs/Kalafina - Magia.mid" \
  --audio "build/bin/data/songs/Magia/song.ogg" \
  --output build/chart-studio/alignment/magia.json

build/bin/gpp_score_to_chart convert "build/tabs/Kalafina - Magia.mid" \
  --track 1 --audio "build/bin/data/songs/Magia/song.ogg" --align auto \
  --output build/chart-studio/alignment/Magia.generated.chart \
  --report build/chart-studio/alignment/Magia.report.json

# Reutilizar uma análise sem recalcular FFT:
build/bin/gpp_score_to_chart convert "build/tabs/Kalafina - Magia.mid" \
  --track 1 --alignment build/chart-studio/alignment/magia.json \
  --output build/chart-studio/alignment/Magia.replayed.chart
```

`--align none` restaura escala 1 e offset 0 (ou `--offset-ms` explícito).
Sem opções de alinhamento, a CLI preserva o tempo MIDI ou a transformação já
salva no projeto. `--audio` sozinho associa o arquivo sem alterar o tempo.
`--align offset` procura apenas deslocamento. `linear` também procura escala;
`auto` prefere offset se a melhora do modelo linear for inferior a 0,02 na
pontuação. `dtw` retorna erro explícito: não foi implementado nesta versão.
`--align-min-offset` e `--align-max-offset` mudam a busca padrão de -15 a +30 s
(limites absolutos de -120 a +120 s). Escala padrão: 0,97 a 1,03.

A análise `align` sempre retorna os candidatos sem aplicá-los. `convert` retorna
código **4** quando não há confiança suficiente, sem escrever chart/projeto;
se `--report` for dado, escreve nesse arquivo o diagnóstico de alinhamento.
Após inspecionar a análise, `--alignment FILE --alignment-candidate 0` aceita
explicitamente um candidato, inclusive ambíguo. Os índices vão de 0 a 2.
Offsets manuais e análise automática não podem ser solicitados simultaneamente.
Os hashes do MIDI e áudio são conferidos ao reutilizar o artefato.

No painel **Auto Align**, escolha o modo e a faixa, clique **Analyze alignment**,
selecione um candidato, ative **Preview alignment** (contornos brancos na
timeline) e então **Apply selected alignment**. A análise roda em um worker
cancelável sobre uma cópia da partitura; apenas a thread da GUI modifica a
sessão. O dispositivo BASS permanece vivo até o worker terminar. Apply e Reset
participam de undo/redo. O painel mostra candidatos, cobertura, diferença de
pontuação e resíduos locais. Após reabrir o projeto, a evidência aplicada volta
a ficar disponível, mesmo sem refazer a análise.

## Representação e responsabilidades

- `alignment.cpp/h`: descritores MIDI, busca e diagnóstico, sem BASS/OpenGL.
- `alignment_audio.cpp`: adaptador para `gpp_audio::scanSong`, sem segmentação
  de regiões de fail sound. FFT de 8192 amostras, passo de 50 ms; timestamps
  corrigidos para o centro da janela. Não cria um dispositivo BASS por conta
  própria: CLI/GUI são responsáveis pela inicialização e encerramento.
- `TimingAlignment`, `audioSeconds`, `audioTick`: transformação explícita
  `t_audio = scale * t_MIDI + settings.offset`. A partitura, seus ticks e
  mudanças de tempo permanecem intactos. Ambas as pontas dos sustains, piano
  roll, compassos, BPM exibido, edição pelo cursor e preview usam essa relação.
- Exportação divide BPM pela escala e mantém os ticks e assinaturas originais.
  O `[Song] Offset` representa a translação. O round-trip usa o parser real do
  jogo; a quantização do `.chart` continua em milésimos de BPM.
- Projetos SQLite versão 2 guardam escala e evidência fora do documento musical
  original. A leitura migra projetos versão 1 para escala 1, sem alterar o banco
  de entrada até salvar. A biblioteca de padrões permanece versão 1. Projetos
  novos são rejeitados por studios antigos em vez de ignorarem o alinhamento.
  A serialização histórica `.gpp` não muda.

O chroma MIDI usa todas as pistas exceto canal MIDI 10/percussão. Peso de guitarra
GM: 1; baixo: 0,65; demais instrumentos: 0,8, multiplicados pela raiz da velocity
normalizada. Cada nota contribui durante sua duração, inclusive em mudanças de
BPM. Não depende da pista escolhida para gerar gameplay.

A busca usa cosseno do chroma centrado (90%) e novidade harmônica entre janelas
(10%). Essa novidade é uma **aproximação de ataques**, não um detector preciso
de transientes. Silêncio e chroma uniforme não fornecem evidência positiva.
Frames MIDI ativos fora do áudio penalizam a pontuação; cobertura é reportada.
A busca grossa avalia offsets a cada 100 ms e escalas a cada 0,002, seguida por
refinamento de 10 ms e 0,0005. Os finalistas são avaliados em todos os frames;
a etapa grossa usa amostragem limitada. Desempates explícitos favorecem escala
mais próxima de 1 e depois menor offset. Determinismo é testado no mesmo
ambiente; diferenças de decodificação/FFT entre plataformas podem mudar um
candidato próximo do limiar.

A confiança é uma regra diagnóstica, **não uma probabilidade calibrada**: duração
mínima de 10 s, score > 0,35, cobertura > 75%, margem > 0,025, ao menos quatro
das seis janelas com score > 0,25 e resíduo <= 300 ms, sem encostar nos limites
da busca. As seis janelas buscam deslocamentos locais de +/-1 s e reportam o
resíduo; não aplicam correções locais. No modo auto, o primeiro candidato é a
solução preferida por simplicidade e pode ter score ligeiramente inferior ao
segundo, resultando numa margem negativa e recusa de aplicação automática.

## Silêncio inicial

Se a primeira nota musical cair antes de zero, o relatório informa
`suggestedSilence`: o silêncio mínimo a acrescentar ao áudio para trazê-la a
zero. Nenhum áudio é reescrito. Ao acrescentar S segundos de silêncio, aumente
o offset do chart em S segundos, ou analise novamente o novo arquivo. Isso não
inclui uma contagem de entrada: adicione manualmente a folga desejada. Um offset
negativo sozinho não exige silêncio se todas as notas ainda começarem após zero.

## Avaliação local e limites

Arquivos locais não são incluídos no repositório; resultados ficam em
`build/chart-studio/alignment/`. Primeira avaliação:

| MIDI / áudio | Escala sugerida | Offset | Diagnóstico |
|---|---:|---:|---|
| Magia / `Magia/song.ogg` | 1 | +1,07 s | consistente; resíduos de 0 a -50 ms |
| Honesty / `Honesty/song.ogg` | 1,0015 | -0,19 s | global consistente; janela em ~237 s fraca, resíduo -850 ms |
| Slow Ride / `Slow Ride/song.ogg` | 1 | +0,47 s | ambíguo; não aplicar automaticamente |
| Slow Ride / `Slow Ride/guitar.ogg` | 1,0155 | -2,71 s | harmonia melhor, mas candidatos próximos e deriva inicial |

Esses valores são estimativas, não uma validação auditiva. `song.ogg` de um jogo
pode ser somente um stem, sem a guitarra; não confundir com uma mixagem completa.
Não foram modificados arquivos em `songs/`. Também não se tentou ajustar o
conversor às lanes dos charts profissionais.

DTW, cortes de arranjo, repetição de seções, mudança de afinação/transposição do
áudio e separação de instrumentos continuam pendentes. Uma transformação linear
não resolve essas diferenças. Próximos experimentos: mixagem explícita dos stems,
melhoria dos ataques, residual em janelas menores e âncoras manuais antes de DTW
monotônico limitado. Chroma e onset/DTW são referências de sincronização no
[Sync Toolbox](https://github.com/groupmm/synctoolbox); esta implementação não
adiciona essa dependência nem promete os resultados daquele sistema.

## Testes

```sh
ctest --test-dir build -R '^chart_studio_(cpu|runtime|cli|alignment|ui)$' --output-on-failure
```

Testes sintéticos verificam offsets positivos/negativos, escala, mudanças de
BPM, percussão excluída, silêncio/harmonia constante, entrada inválida, repetição
determinística, persistência/migração, undo/redo e preservação de lanes bloqueadas.
Um WAV sintético permite verificar FFT BASS -> alinhamento -> `.chart` -> parser
do jogo sem DISPLAY, incluindo a correção do timestamp da janela, recusa de
ambiguidade, aceitação explícita e proteção dos arquivos originais. O teste de
runtime verifica sustain atravessando mudança de BPM com escala diferente de 1.

Validação desta implementação: seis suites CTest do studio passaram (incluindo
UMAP); cinco suites passaram com ASan/UBSan no NativeLab, incluindo GUI via
Xvfb/Mesa OpenGL 2.0. Após corrigir o contador da grade, os testes CPU, runtime
e alinhamento foram repetidos. Usadas as supressões já existentes para código
terceiro/Mesa. `clang-format` passou; a revisão `clang-tidy` corrigiu o contador
float e deixou apenas uma recomendação de padding da estrutura Workbench.
Não foram feitos playtest auditivo, build Windows ou validação completa do jogo.
