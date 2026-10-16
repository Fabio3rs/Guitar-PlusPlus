# Guitar++ Chart Studio

Workbench independente para importar **MIDI musical**, associar pistas a partes
instrumentais do jogo, gerar cinco lanes, corrigir frases e guardar decisões em SQLite. O objetivo musical é
um chart razoável e consistente, com estilo próprio; igualdade com lanes de um
chart profissional não é uma medida de qualidade.

Há um [experimento opcional com UMAP](chart-studio-umap.md) para projetar contexto
musical em alvos de cinco lanes, preservando a busca e a precedência do cache.
O algoritmo padrão continua sendo `beam`.

## Build e execução

Requer as dependências habituais do jogo, ImGui existente e libpng. Para os testes
de interface em Linux, instale Xvfb e xdotool. ImageMagick é opcional para capturas
de diagnóstico (instalado no CI). `BUILD_CHART_STUDIO` fica OFF por padrão.

```sh
cmake -S . -B build -G Ninja -DBUILD_CHART_STUDIO=ON -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --parallel 4
./build/bin/gpp_chart_studio
./build/bin/gpp_chart_studio "build/tabs/Paramore - Still Into You.mid" --track 3
ctest --test-dir build -R '^chart_studio' --output-on-failure
```

A GUI usa GLFW/OpenGL 2.0 e GLSL 1.10, com configuração própria em
`chart-studio/layout-docking.ini`; `--output-dir DIR` altera esse diretório. Não precisa
abrir o jogo, inicializar `GPPGame` ou carregar seus modelos/texturas. A CLI não
cria janela nem contexto GL. `gpp_score_tests` nem sequer linka bibliotecas gráficas.

## Fluxo de uso

1. Em **Files**, clique em **Browse...**, navegue pelas pastas e selecione um
   `.mid`/`.midi`. **Use this path** preenche o campo; **Open MIDI / project**
   carrega o arquivo. O mesmo fluxo abre `.gppchartproj`. A mensagem central informa
   quantas pistas foram carregadas e qual é o próximo passo.
2. Em **Tracks**, selecione uma parte na tabela e escolha sua pista musical.
   **New instrument / New difficulty / Add part** acrescenta outra associação.
   **Source filters** permite escolher canal/programa (índices a partir de zero;
   `-1` inclui todos). A lista mostra nome, programas, canais e quantidade de notas;
   os detalhes mostram também acordes e alturas. Selecionar uma pista não gera notas.
3. **Generate all parts** gera o projeto inteiro em background, incluindo Plus nas
   partes em que está habilitado. **Generate chart** e Ctrl+G geram somente as lanes
   da parte ativa. Comece com Expert e Beam. Pesos/densidade valem para a próxima
   geração; decisões confirmadas/bloqueadas continuam sendo reutilizadas.
   O lote é aplicado inteiro como uma operação de undo; falhas/cancelamento
   preservam os resultados anteriores. Cancelamento é verificado entre partes.
   Alterar o projeto durante o processamento torna o resultado obsoleto.
4. Na timeline, roda do mouse faz zoom e arrasto com botão central navega.
   Clique no ataque para destacar suas fontes. Arrasto move a nota;
   Shift+arrasto ajusta sustain; Ctrl+clique adiciona à seleção.
5. Em **Edit and patterns**, as lanes têm nomes e cores. Início e sustain podem
   ser editados em segundos ou, opcionalmente, ticks. Os campos editam a primeira
   nota selecionada; Copy/Delete usam a seleção. Confirm/Lock ensinam a frase.
   **Apply to equivalent occurrences** compartilha a correção explicitamente.
6. Para regenerar um trecho, posicione o cursor e use **Set start/end at cursor**
   em Mapping. Os limites aparecem em segundos, com ticks disponíveis em detalhes.
7. Em **Files**, confira os destinos e use **Save project** para guardar a edição
   e **Export .chart** para criar o arquivo jogável com todas as seções listadas.
   São arquivos diferentes. Todas as partes precisam ter ataques para exportar;
   remova associações incompletas ou gere-as antes.
   Um MIDI novo sugere nomes derivados da música, no diretório de saída do studio.
   A substituição de export existente exige habilitar a opção correspondente.
8. Áudio e referência são opcionais. Carregue uma referência `.chart`, `.gpp` ou
   MIDI de **gameplay** em Files para comparação somente leitura. Play/Pause/Stop
   usam BASS com áudio carregado e relógio monotônico sem áudio. **Audio alignment**
   separa Analyze, Preview e Apply; o botão **?** explica candidatos e confiança.

Os botões **?** abrem um manual modal por assunto. **Help / workflow** abre o guia
inicial. Arraste as abas para encaixar, agrupar ou soltar painéis dentro da janela;
**Panels** reabre painéis fechados e **Reset layout** restaura a organização inicial.
O layout é salvo em arquivo próprio, sem dependência de ImGui no runtime.

Atalhos opcionais: Ctrl+L bloqueia frase; Ctrl+G gera a parte ativa; Ctrl+Z/Y desfaz/refaz;
Ctrl+S salva; Ctrl+E exporta; Ctrl+C/V copia/cola; Ctrl+Q fecha. Teclas 1–5 alternam
as lanes da nota selecionada. A interface permite o fluxo principal sem terminal;
o experimento UMAP ainda exige preparar externamente seu arquivo de projeção.

O navegador é não recursivo, filtra por extensão e nome, permite subir de pasta,
atualizar a lista e digitar caminhos (inclusive unidades no Windows). Ele apenas
seleciona caminhos: não sobrescreve nem carrega arquivos ao navegar.

A inspeção FFT reutiliza `gpp::audio::analyze` do Audio Studio. O botão analisa
uma janela de 200 ms na posição de playback, FFT de 8192 amostras, sem sintetizar
áudio. Candidatos harmônicos numa mixagem não identificam isoladamente guitarra,
e harmônicos podem parecer outras notas. Não modifica BPM, offset nem lanes.

```sh
./build/bin/gpp_score_to_chart fft "build/bin/data/songs/Paramore - Still Into You/song.ogg" \
  --at 60 --output build/chart-studio/paramore-fft.json
```

O [alinhamento MIDI/áudio](chart-studio-alignment.md) oferece offset e escala global.
DTW e inferência de instrumentos a partir da mixagem continuam pendentes.

## Várias partes no mesmo chart

Uma parte é o par **instrumento do jogo + dificuldade**, com uma pista musical
como origem. A mesma pista pode alimentar destinos diferentes. Cada parte guarda
suas notas, edições, padrões e Plus; mapa de tempo, gravação e alinhamento são comuns.
Troque a parte ativa pela tabela Tracks ou pelo seletor da timeline. Cursor/zoom
continuam iguais; seleção de notas e referência carregada são reiniciadas para não
editar/comparar acidentalmente outra parte. Remover uma parte permite undo.

| Destino | Sufixo da seção `.chart` |
|---|---|
| Guitarra principal | `Single` |
| Baixo | `DoubleBass` |
| Guitarra rítmica | `DoubleRhythm` |
| Guitarra co-op | `DoubleGuitar` |
| Keys de cinco lanes | `Keyboard` |

A dificuldade prefixa o sufixo: `[ExpertDoubleBass]`, por exemplo. Keys usa o mesmo
núcleo melódico de cinco lanes; não é Pro Keys. Não há conversão de bateria/vocal/Pro
nessa lista. O programa GM descreve o timbre de reprodução, não garante o papel
musical: nossas tablaturas usam sax/clarinete para algumas vozes.

```sh
./build/bin/gpp_score_to_chart convert "build/tabs/Paramore - Still Into You.mid" \
  --part ExpertSingle:3 --part ExpertDoubleRhythm:4 \
  --part ExpertDoubleBass:5 --part ExpertKeyboard:6 \
  --plus auto --plus-profile strategic --plus-trials 32 \
  --output build/chart-studio/StillIntoYou.multitrack.chart \
  --project build/chart-studio/StillIntoYou.multitrack.gppchartproj \
  --report build/chart-studio/StillIntoYou.multitrack.json

./build/bin/gpp_score_to_chart inspect build/chart-studio/StillIntoYou.multitrack.gppchartproj
./build/bin/gpp_score_to_chart events build/chart-studio/StillIntoYou.multitrack.gppchartproj \
  --instrument ExpertDoubleBass
./build/bin/gpp_score_to_chart plus-simulate build/chart-studio/StillIntoYou.multitrack.gppchartproj \
  --instrument ExpertDoubleBass --output build/chart-studio/bass-plus.json
```

`--part SECAO:TRACK[:CANAL[:PROGRAMA]]` é repetível. Track/canal/programa começam em
zero; filtros omitidos ou `-1` incluem todos. Seções duplicadas, fontes sem ataques
ou instrumentos não suportados causam erro. `--part` substitui `--track`,
`--difficulty`, `--channel` e `--program`; não combine essas opções. `--track`
sozinho continua gerando guitarra principal como antes.

Pesos, política de sustains e parâmetros Plus da CLI são aplicados às partes do
lote. O projeto mantém parâmetros individuais para ajustes posteriores na GUI.
Ao reutilizar um projeto existente, associações correspondentes preservam edições;
novas associações são adicionadas e partes omitidas permanecem no projeto. Mudar a
origem de uma parte já editada exige **Reset this part to new source** na GUI
(com undo), ou um novo projeto. Nenhuma edição é descartada implicitamente.
Regeneração por região continua disponível para a parte ativa na GUI; o lote
pela CLI não aceita limites globais `--begin-tick`/`--end-tick`.

O alinhamento `--audio ... --align auto` é calculado uma única vez. Todas as partes
recebem o mesmo offset/escala; apenas um `[Song]` e um `[SyncTrack]` são exportados.
O round-trip carrega cada seção pelo parser real e verifica notas, sustains e Plus
antes de publicar o arquivo. Isso não acrescenta modos de bateria/voz ao runtime.

UMAP permanece opcional: use `--algorithm umap` e um
`--part-projection SECAO=ARQUIVO.json` por parte. Para preparar features de outro
instrumento, use `features MIDI --track N --instrument ExpertDoubleBass` antes do
fluxo de projeção descrito no manual UMAP. Artefatos são externos e devem ser
recarregados depois de abrir o projeto. O cache verifica instrumento/dificuldade;
as assinaturas antigas de guitarra principal continuam válidas.

Projetos novos usam SQLite versão 5 (receitas de mescla). Projetos versão 4
preservam suas múltiplas partes; as versões 1–3 são lidas como uma parte de
guitarra principal; ler não modifica o arquivo, salvar publica a nova versão.
O payload mantém o bloco legado e acrescenta a identidade ativa e as partes
adicionais. `Part` contém os dados editáveis; `State` mantém a parte ativa e a
partitura/alinhamento compartilhados. Os motores existentes operam na parte ativa,
sem duplicar suas regras. O histórico guarda snapshots completos, até 64 operações.
A biblioteca global de padrões continua na versão 1; não há migração de `.gpp`.
Relatórios de múltiplas partes usam `schema: 2` e uma lista `parts` com a seção,
a origem e o relatório existente de cada parte. Relatórios de parte única mantêm
seu formato anterior. `plus-simulate` sem seletor relata todas as partes;
`events` e `patterns` usam a parte ativa quando `--instrument` é omitido.

### Fontes musicais locais verificadas

Índices abaixo são candidatos identificados por nomes/programas, não uma seleção
automática nem uma afirmação de equivalência ao chart profissional:

| MIDI em `build/tabs` | Pistas candidatas |
|---|---|
| Slow Ride | Guitarra 1, segunda guitarra 2, baixo 3 |
| Magia, ambas as versões | Guitarras 1/2/3, baixo 4 |
| Still Into You | Principal 3, rítmica 4, baixo 5, sintetizador 6 |
| Through the Fire and Flames | Elétricas 6/7/9, baixo 10; teclados em várias pistas |
| Destructive Device, ambas as versões | Guitarra 1, baixo 2 |
| Knights of Cydonia | Elétrica 4, limpa 3, baixo 5, sintetizador 7 |
| Before I Forget | Principal 2, rítmica 3, baixo 6, teclado 7 |
| My Name Is Jonas | Elétrica 6, rítmica 8, baixo 10, acústica 5 |
| Barracuda | Guitarras 2/3/4, baixo 5; nomes vazios exigem inspeção |
| Honesty | Guitarra 1; guitarras 2/3 têm apenas 19 notas cada |
| The Logic Behind Heads Or Tails | Guitarra 1; pista 2 usa piano e tem apenas 28 notas |

Foram encontrados 13 MIDI. Cliffs of Dover tem `.gp5`, mas não MIDI local nesta
avaliação. Não inventar baixo/teclado ausente nem preencher lacunas da transcrição.
Os arquivos locais não são fixtures redistribuíveis e não entram no repositório.

### Avaliação desta implementação

O script `tools/chart_studio/experiments/multipart_compare.py` executa as associações
explícitas acima e grava comandos, hashes dos MIDI, relatórios por parte e resultados
de reabertura. Ele exige um diretório de saída novo; não sobrescreve `songs`.

```sh
python3 tools/chart_studio/experiments/multipart_compare.py \
  build/bin/gpp_score_to_chart build/tabs build/bin/data/songs \
  build/chart-studio/minha-avaliacao-multipart
```

Na avaliação local de 2026-10-14, os 13 MIDI geraram **44 partes**, todas validadas
pelo loader, e os 13 projetos reabriram. Foram usados Beam, Expert, Plus Strategic,
preferência de dois compassos por frase e 32 ensaios por combinação de estratégia/
habilidade. Honesty, Magia e The Logic Behind Heads Or Tails reutilizaram a evidência
de alinhamento dos projetos instalados. Os resultados estão em
`build/chart-studio/multipart-evaluation-20261014/summary.json`; os `.chart`, projetos
e relatórios individuais ficam ao lado. Essa avaliação verifica o pipeline e não
substitui ouvir/jogar cada arranjo.

A suíte sintética verifica exportação das cinco seções, dificuldades diferentes,
fontes/filtros, isolamento de decisões, undo/redo, migração v1–v3, projeções opcionais
e proteção dos arquivos. O teste de GUI adiciona baixo, gera o lote, preserva o
bloqueio da guitarra, desfaz/refaz e exporta ambas as partes usando ImGui real.

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

O adaptador `tools/chart_studio/runtime.*` não executa o gameplay: usa os loaders
do jogo para leitura e validação CPU. A simulação offline de Plus também não é
uma emulação de `CGamePlay`/`CPlayer`: exige acertar a sequência completa, mas
não modela palhetadas extras nem todas as condições de julgamento. O jogo possui
invalidação por erros em `processError()`, `breakPlus()` e
`processErrorNonPickedB()`.
Consulte as [condições reais e limitações da simulação](chart-studio-plus.md#simulação-e-contrato-real).

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

- Referências `.chart` e MIDI de gameplay podem selecionar guitarra principal,
  baixo, rítmica, co-op e Keys de cinco lanes. Bateria, vocais, instrumentos Pro
  e GHL continuam sem adaptação ao gameplay; veja os TODOs em
  [Suporte MIDI](midi-support.md#todo-reconhecimento-vocal-sem-gameplay).
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
- Geração de uma parte e inspeção FFT ainda são síncronas. Geração de todas as
  partes roda em background, com cancelamento entre partes. Undo copia estados
  completos (até 64), com custo de memória que cresce com a quantidade de partes.

### Mesclar duas pistas em uma parte

Implementado no painel **Merge tracks** e na CLI. A mescla é uma alternância
explícita **de charts já mapeados**, não uma união de pitches nem mixagem de áudio.
A fonte primária é a associação configurada em Tracks / `--part`; a secundária
é escolhida no painel ou por `--merge-source SECTION:TRACK[:CHANNEL[:PROGRAM]]`.
As duas fontes pertencem ao mesmo MIDI, usam o instrumento/dificuldade de destino
e compartilham alinhamento. Não é preciso exportar as fontes como outras partes.

```sh
build/bin/gpp_score_to_chart convert "build/tabs/Mindflow - Break Me Out.mid" \
  --part ExpertSingle:2 --merge-source ExpertSingle:1 \
  --merge-min-silence 1/2 --merge-max-silence off --merge-clearance 0 \
  --plus auto --plus-profile strategic \
  --output build/break-merged.chart \
  --project build/break-merged.gppchartproj \
  --merge-report build/break-merge-decisions.json
```

A prioridade automática preserva os ataques e sustains **jogáveis** da primária
(após a política normal de sustain). A secundária só preenche pausas dentro dos
limites configurados. O mínimo padrão `1/2` significa duas semínimas; máximo
`off` não limita a pausa. Frações são de semibreve: `1/4` = uma semínima,
`1/8` = meia semínima, `1/12` = um terço. Comparações usam frações inteiras,
não segundos arredondados. Compasso não altera essa unidade. A interface mostra
a duração da semínima no BPM/alinhamento local.

`--merge-clearance` deixa uma margem musical em ambas as extremidades da pausa;
padrão zero. Os limites de silêncio são verificados antes de subtrair essa margem.
Um tick de separação evita um ataque da secundária exatamente na ponta da cauda
primária. A secundária é cortada antes do retorno da primária. A mescla não
remapeia lanes, não cria ataques para retomar caudas interrompidas e não junta as
duas fontes em acordes. Acordes ainda têm duração compartilhada no runtime.

**GUI:** Configure > Analyze merge > confira PRIMARY / SECONDARY / RESULT >
Apply merge. A prévia compartilha zoom, posição e áudio com Timeline. As áreas
coloridas identificam a fonte; marcas vermelhas indicam exclusões e cortes de
transição. O diagnóstico separa duração MIDI, duração jogável e duração após o
recorte, evitando contar a política normal de sustain como corte de mescla.
O botão `?` explica as unidades e o fluxo.

Recortes manuais `[begin,end)` podem escolher **Primary**, **Secondary** ou
**Silence**, vencendo a seleção automática. Marque início/fim no cursor, edite
ticks ou arraste as duas linhas brancas; use **Add manual cut** para incluir o
intervalo na proposta. **Edit range** retira o recorte para edição e posterior
reinserção; **Automatic** remove a exceção. Recortes sobrepostos são rejeitados.
A grade inicial é `1/16`, com divisões binárias e tercinas. A grade só altera as
bordas manipuladas; nunca quantiza ataques existentes. Arredonda para o tick
mais próximo, com empates para o anterior.

**Save recipe** escreve JSON versionado sem sobrescrever um arquivo existente.
**Load recipe into draft** importa a configuração da parte ativa sem aplicá-la.
A CLI aceita o mesmo arquivo por `--merge-recipe FILE`. Seu formato é:

```json
{
  "version": 1,
  "recipes": [
    {
      "key": "[ExpertSingle]",
      "value": {
        "secondaryTrack": 1,
        "secondaryChannel": -1,
        "secondaryProgram": -1,
        "minimum": "1/2",
        "maximum": "off",
        "clearance": "0",
        "grid": "1/16",
        "regions": [
          {"begin": 240000, "end": 276480, "source": 1}
        ]
      }
    }
  ]
}
```

`source`: 0 primária, 1 secundária, 2 silêncio. Os ticks do exemplo são
ilustrativos. Os parâmetros CLI globais substituem os valores correspondentes
da receita. Repita `--merge-source` para destinos diferentes; duas secundárias
para o mesmo destino são rejeitadas. `--merge-preview` calcula e imprime a
proposta, opcionalmente grava `--merge-report`, mas não escreve chart/projeto.
`--output` continua necessário para identificar o destino pretendido.

Projetos versão **5** preservam receita, snapshots de ambas as fontes, cache de
cada fonte, IDs de saída e proveniência de recortes. Projetos 1–4 continuam
legíveis; a biblioteca global permanece versão 1. A composição não promove
fragmentos recortados sob a assinatura da frase original. Edições de lanes na
saída são intervenções locais, preservadas nas reaplicações compatíveis.
Conflitos com edições/bloqueios interrompem a operação sem aplicar uma parte dela.
Use Restore/unlock para resolver explicitamente, ou **Detach / restore primary**
para voltar ao snapshot primário. Aplicação e detach têm undo/redo.

Analyze reaproveita snapshots já mapeados; Generate refaz as fontes com seus
caches e recompõe a parte. Regeneração parcial pelo antigo painel Mapping não
se aplica à composição: use seus recortes. UMAP não recebe fallback silencioso:
mapear/regenerar uma fonte UMAP sem seu artefato continua retornando erro; a
configuração de dois artefatos UMAP independentes na mescla permanece pendente.

Plus é calculado sobre o resultado composto, sem copiar frases Plus da secundária.
Frases manuais válidas são preservadas; conflitos são diagnosticados. O exportador
separa por um tick a borda exclusiva de Plus de um ataque seguinte quando isso
não remove nenhum ataque da frase, evitando a inclusão indevida por arredondamento
no loader. Ticks originais do projeto permanecem intactos; o round-trip verifica
os índices reais de Plus.

**Limites e plano futuro:** mais de duas fontes, mesclas encadeadas, fontes de
MIDIs diferentes, união de vozes simultâneas, remapeamento ergonômico das transições
e edição direta dos snapshots internos. A escolha primária/secundária é explícita;
não há seleção automática de instrumentos. Nenhuma alteração no runtime ou nos
arquivos musicais originais é necessária.

Também permanecem pendentes conversores próprios para bateria, vocais e
instrumentos Pro; importação direta de Guitar Pro/MusicXML; e DTW/alinhamento
local. A geração de várias seções de cinco lanes não implementa esses recursos.

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

O [alinhamento MIDI–áudio](chart-studio-alignment.md) oferece busca de offset e
escala global em CLI/GUI, diagnóstico de ambiguidade, preview, aplicação com
undo/redo e exportação do SyncTrack corrigido. Não altera o áudio nem o cache de lanes.

## Recursive Plus statistics

The offline analyzer discovers gameplay charts recursively and reads their Plus
phrases through the game's loaders. It does **not** generate phrases, change
energy rules, run the game, or synthesize audio. Musical MIDI without supported
gameplay tracks is reported as an unsupported input, not as a chart with zero Plus.

Build the existing CPU inspection tool (the GUI is not required):

```sh
cmake -S . -B build -G Ninja -DBUILD_CHART_TOOLS=ON -DBUILD_TESTING=ON
cmake --build build --target gpp_dump_chart
python3 tools/chart_studio/plus_stats.py build/bin/data/songs \
  --dumper build/bin/gpp_dump_chart \
  --output-dir build/chart-studio/plus-stats --samples 1000 --seed 42
```

Python 3.10+ is sufficient; no pip packages or network access are needed. Multiple
root directories or individual `.chart`, `.gpp`, `.mid`, and `.midi` files can be
passed as positional arguments. Directory symlinks are not followed. A failed
file does not stop the scan; `--timeout` limits each loader subprocess (30 seconds
by default). Loader logs are isolated in a temporary working directory.

Open `report.html` directly in a browser. It contains its own data, styles and
scripts; no server is required. Its initial view is Expert Guitar. Filter by
origin, instrument/difficulty, or song, and enable **Mostrar variantes** to see
alternative files. Click a song to inspect individual phrases, density, warnings
and observed versus randomized metrics. The **Como interpretar** expander explains
the method. Sorting, filters and graphs work offline.

Outputs:

- `report.json`: schema/version, settings, input hashes, inventory, errors, results
  per chart/part, random comparisons and aggregate statistics.
- `report.html`: interactive report in Portuguese, including density/Plus timelines
  and observed values against the randomized percentile range.
- `charts.csv` and `phrases.csv`: per-part and per-phrase tables, including variants
  and a `selected` column. Spreadsheet formula-like strings are quoted safely.
- `catalog.json`: editable selection and identity overrides.

Existing reports require `--replace`. Each output is written to a temporary file
and atomically installed; this is per-file atomicity, not a transaction covering
all five outputs. Musical inputs and song metadata are never modified. Keep local
reports under `build/`; do not commit commercial charts or derived fixture data.
Exit codes: **0** complete, **1** partial results with input errors, **2** invalid
configuration or output failure.

### Representatives and origin

All variants are inventoried. Aggregates use one representative per normalized
song identity, origin, and instrument/difficulty. Song identity uses artist/title
metadata; missing artists fall back to the directory identity to avoid merging
unrelated songs. Covers, alternate spellings and unknown metadata may require
manual identity overrides. Different origins are never silently pooled.

Selection priority is an explicit catalog preference, then the same filename
priority as `SongFiles::charts`, then the absolute path as a stable tie-break.
The Plus count is **not** a selection criterion. Equivalent playback fingerprints
within a song group include notes, Plus, tempos and meter; matching file bytes
alone do not establish equivalence when `song.ini` changes interpretation.
Metadata `icon` supplies origin (e.g. `gh3`, `gh3dlc`, `rb4`, `rbn`); absent/zero
values become `unknown`. This does not certify official authorship.

To override selection, copy/edit the emitted catalog and pass `--catalog` on the
next run. Keys may be absolute paths or paths relative to an input root. Absolute
keys take precedence; use them when roots contain the same relative filenames.
For example, explicitly associate the old Slow Ride copies with the GH3 song and
prefer the package version:

```json
{
  "schema": 1,
  "files": {
    "Slow Ride/Chart.chart": {
      "song_id": "foghat / slow ride", "game": "gh3"
    },
    "Slow Ride/notes.mid": {
      "song_id": "foghat / slow ride", "game": "gh3"
    },
    "Guitar Hero III - Legends of Rock/Quickplay/Foghat - Slow Ride/notes.mid": {
      "song_id": "foghat / slow ride", "game": "gh3", "preferred": true
    }
  }
}
```

`exclude: true` removes a file from representative selection, but retains its
inventory and observed statistics. Multiple preferred candidates are resolved by
filename priority/path and produce a warning. The local old Slow Ride copies
load zero Plus phrases, whereas the package version loads nine Expert Guitar
phrases; the report deliberately keeps that difference visible.

### Measurements and random baseline

Additional `.chart` evidence is explicitly opt-in through
`loadFeedbackChartCPU(..., includeExtendedEvidence=true)`, used only by the offline
dumper. Existing callers keep the default `false`: normal gameplay does not scan
SyncTrack for this extra evidence or append these diagnostic Plus events. Tests
compare note times, durations and flags with the option on/off, retain compiled
Plus compatibility, and verify that malformed diagnostic meter data cannot reject
a playable chart.

The source of playable special phrases is `gPlus`, not all MIDI marker notes.
The existing loader chooses marker 103/116 using the track and INI rules; a solo
marker is not counted as an additional Plus phrase. Original indices are retained
and checked against attacks in `[start, end)`. Disagreements are diagnostics, not
runtime repairs. Empty or overlapping phrases are also reported.

The report measures counts/rates, temporal and attack coverage, phrase durations,
start intervals, gaps, chords, sustains, HOPOs/taps/opens, lane movement, one-second
peak attack density, boundaries of beats/bars/sections and surrounding density.
Solo overlap uses explicit solo intervals when available; an absent marker yields
unavailable data, rather than evidence that a song contains no solos. Section
names are preserved; labels such as `0%-5%` are not inferred verses or choruses.

- Musical units are **quarter-note beats (semínimas)**, integrating the original
  tempo map. Compound meters retain their actual numerator and denominator.
- Time normalization uses the first attack through the last sustain end across
  supported parts in each file, not decoded audio duration. Negative audio offsets
  therefore do not shift the musical position distribution.
- Sustains are runtime caudas longer than 10 ms. A chord can have one merged
  duration in the runtime; independent original tails are not inferred here.
- Phrase context uses up to 16 quarter-note beats before/after, clipped to the
  playable part. A context window with no available time is unavailable, not zero.
  Whole-part density uses that part's playable span; zero-span parts have no density.
- Bar/section alignment tolerance is `max(1 / PPQ, 1 / 48)` quarter-note beats.
  Time-signature changes start a new bar; truncated bars are warned about. Assumed
  4/4 is excluded from bar statistics. Compiled `.gpp` lacks original meter/tick
  evidence, so only reconstructible durations are measured.
- Lane movement is the distance between the average positions of adjacent
  attacks, omitting transitions involving open notes. It is not a validated human
  difficulty rating.

The default baseline generates 1,000 schedules per representative. It preserves
phrase count and musical durations, shuffles their placement order, and samples
attack positions without overlap, wholly inside the playable part. Placement has
bounded retries (64 candidates per phrase, 32 schedules per requested sample);
failed samples are counted. This **attack-conditioned sequential model is not
uniform over every possible schedule** and does not preserve original gaps.
Charts with no valid phrases or overlapping phrases have no random comparison.
Use `--samples 0` for inventory/observed statistics only.

Seeds combine the requested seed, algorithm version, normalized playback
fingerprint and part. Traversal order and other songs do not affect a part's
samples. The report records Python and extractor versions/hashes. Results are
repeatable in the same software environment; floating-point results are not
promised bit-identical across different toolchains.

Observed values are compared with randomized medians, 2.5/97.5 percentiles,
percentile ranks (ties receive half weight), and differences from the random
median. Aggregates give equal weight to songs within the same origin/part;
correlations are exploratory Spearman coefficients with tied ranks and explicit
sample sizes. There is no causal claim, author-intention inference, significance
label, multiple-testing correction, energy simulation, or optimizer in this tool.

Validation:

```sh
ctest --test-dir build -R 'chart_plus_statistics|ChartRuntime|midi_core' \
  --output-on-failure
```

Tests use synthetic charts/MIDI only. The CI enables `BUILD_CHART_TOOLS` and runs
the same test suite. Report viewing is separate from ImGui and does not make the
runtime depend on a browser or Python.

## Geração estratégica de Plus

O studio agora oferece geração opt-in de Plus, prévia/aplicação, edição protegida,
undo/redo, exportação validada de `S 2` e simulação CPU compartilhada entre GUI/CLI.
Veja [o manual de Plus](chart-studio-plus.md) para perfis, comandos, modelos de
conclusão, migração de projetos e limitações. O analisador estatístico descrito
acima continua independente e é reutilizado pela avaliação dos perfis.

### Sustains e reutilização da mesma lane

A prévia jogável e a exportação encerram uma cauda um tick antes do próximo
ataque que reutilize qualquer lane ocupada por ela. Ataques em outras lanes não
interrompem o sustain. O limite mínimo configurado é aplicado depois desse corte.
Como o adaptador atual usa duração compartilhada para acordes, reutilizar uma de
suas lanes encurta todas as caudas daquele acorde. Durações originais e fontes
musicais permanecem no projeto; `export_sustains_shortened` no relatório conta
os eventos encurtados por estas políticas (incluindo o limite mínimo).

Na investigação de Magia, o MIDI musical original continha um Note On de pitch
38, canal 2, track 2, no tick 239040 e seu Note Off no tick 412000: 86,48 segundos
em 125 BPM/PPQ 960. Outras alturas tocavam simultaneamente, e o mapeamento havia
atribuído 53 novos ataques à mesma lane verde. O pareamento MIDI estava correto;
a política de saída não resolvia a colisão de lanes. Não se modifica o MIDI nem
o loader de gameplay para corrigir esse caso.


### Avaliação local da mescla

Para repetir a avaliação sem instalar nem sobrescrever músicas:

```sh
python3 tools/chart_studio/experiments/merge_compare.py \
  --binary build/bin/gpp_score_to_chart --tabs build/tabs \
  --output build/chart-studio/my-merge-evaluation
```

O diretório de saída deve ser novo. O script pula arquivos ausentes, gera os charts,
reabre cada projeto em outro processo, compara os bytes exportados e verifica
lanes e colisões de sustain. Aceita `--break-alignment FILE` para reutilizar o
alinhamento de Break Me Out. Não avalia semelhança com charters profissionais.

Na avaliação local de implementação, com mínimo `1/2` e máximo desligado:

| MIDI / primária + secundária | Ataques resultantes | Ataques copiados da secundária |
|---|---:|---:|
| Break Me Out / 2 + 1 | 1.015 | 23 |
| Break Me Out / 2 + 1, recorte manual 240000–276480 | 952 | 104 |
| TTFAF / 6 + 4 | 3.337 | 154 |
| Magia / 1 + 2 | 1.066 | 0 |
| Honesty / 1 + 2 | 1.565 | 19 |
| Slow Ride / 1 + 2 | 545 | 0 |
| Still Into You / 3 + 4 | 845 | 18 |

Zero inserções pode ser o resultado correto da prioridade conservadora.
Honesty continua sem material onde nenhuma das duas fontes tem notas.
O recorte manual de Break Me Out demonstra substituição, não uma recomendação
musical validada por audição. O alinhamento salvo foi aplicado nesse exemplo;
os demais verificam composição no tempo musical original.
Resultados: `build/chart-studio/merge-evaluation-final/summary.json`.
Não houve playtest auditivo nem substituição dos charts instalados.

Validação da implementação: suites CPU, runtime/round-trip, CLI, alinhamento,
UMAP opcional e testes gráficos existentes passaram. O novo teste gráfico
`chart_studio_merge_ui` verifica que Preview não modifica o projeto e exercita
Apply, undo/redo e reabertura. ASan/UBSan/LSan no NativeLab passaram nos testes CPU;
o teste gráfico usa a supressão Mesa já existente e uma supressão local
`shift-base:*/glfw/src/x11_window.c` para UB observado no GLFW vendorizado ao
focar a janela. Nenhuma supressão cobre o código do studio. Não foram validados
Windows nem execução auditiva em gameplay.

### Síntese opcional de stems

O painel **Panels → MIDI Synth** e o comando `render-stems` renderizam fontes
MIDI originais com FluidSynth, usando o alinhamento aceito no projeto. WAV
funciona sem FFmpeg; Ogg o utiliza apenas para codificação. A geração de
charts permanece independente. Consulte [síntese MIDI e stems](chart-studio-synth.md)
para seleção, build opcional, limitações e situação do suporte Windows.
