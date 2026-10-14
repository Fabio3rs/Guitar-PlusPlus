# Suporte a charts MIDI no Guitar++

O Guitar++ importa `notes.mid` para as mesmas notas, BPMs e frases de **Plus** usadas pelo gameplay de `.chart`. O adaptador não reproduz MIDI, não sintetiza áudio e não modifica julgamento, renderização, input ou carregamento de stems.

## Arquitetura e seleção do formato

`CMidiChart` lê e normaliza MIDI sem depender de GPU, BASS ou instâncias do jogo. `CChart::NotesData::loadMidiChart` seleciona a dificuldade solicitada e entrega `gNotes`, `BPM` e `gPlus` existentes ao jogador. `CPlayer` apenas despacha a extensão adicional. `CCharter` continua consumindo essas estruturas; não há exportação ou edição nativa de MIDI nesta etapa.

`ChartSourceInfo`, pertencente a cada `NotesData`, conserva PPQ, eventos de tempo em ticks e segundos, posições das notas, fins de cada pista, textos e motivos de classificação HOPO. Esses dados acompanham cópia, movimento e limpeza do chart; não alteram os campos serializados de `.gpp`. O gameplay continua usando segundos em `double`.

A descoberta permanece no primeiro nível de `data/songs`. Dentro da pasta, a ordem histórica dos nomes é preservada e `notes.mid` é o último candidato:

1. `<nome da pasta>.gpp`
2. `<nome da pasta>.chart`
3. `Chart.gpp`
4. `Notes.gpp`
5. `Chart.chart`
6. `notes.chart`
7. `notes.mid`

A comparação de nomes ignora maiúsculas/minúsculas e empates usam a ordem lexical dos nomes reais. Um candidato precisa conter pelo menos uma dificuldade carregável. Um `.chart` inválido permite tentar MIDI; a ausência da dificuldade escolhida em um chart válido não mistura formatos nem escolhe outra dificuldade. `original.mid` é apenas uma entrada explícita para diagnóstico. Não são criadas duas entradas para a mesma pasta.

## Convenções implementadas

| Informação | Interpretação |
| --- | --- |
| Contêiner | SMF Type 0 e Type 1, divisão PPQ positiva |
| Instrumentos | `PART GUITAR` e alias GH antigo `T1 GEMS` → `Single`; `PART BASS` → `DoubleBass`; `PART RHYTHM` → `DoubleRhythm`; `PART GUITAR COOP` → `DoubleGuitar`; `PART KEYS` → `Keyboard` |
| Dificuldades | Easy, Medium, Hard, Expert: bases 60, 72, 84, 96 |
| Pistas | Base + 0…4: verde, vermelho, amarelo, azul, laranja |
| Acordes | Notas com o mesmo tick viram um ataque com máscara de pistas |
| Sustain | Duração do par Note On/Off; Note On com velocidade zero é Note Off |
| Force HOPO/strum | Base + 5 / base + 6; intervalos incluem início e excluem fim |
| Tap | Nota 104 ou frase SysEx Phase Shift de tipo 4 |
| Open | Base − 1 com texto `[ENHANCED_OPENS]`; ou SysEx Phase Shift de tipo 1 |
| Plus | Nota 116; fallback antigo 103 quando a trilha não contém 116 |
| Tempo | Todos os eventos de tempo, inclusive simultâneos; padrão MIDI de 120 BPM antes do primeiro |
| Textos | Metas 1…7, incluindo seções `section` e `prc_`; assinaturas de compasso conservadas para diagnóstico |
| Metadados | `song.ini`: name, artist, charter, delay em ms; nomes e chaves sem distinção de caixa |

As frases SysEx aceitas são `F0 50 53 00 00 dificuldade tipo estado F7`, com dificuldade 0…3 ou FF para todas, tipo 1/4, estado 1/0. Mensagens desconhecidas não viram notas.

GH/RB/Clone Hero não são tratados como convenções idênticas. A nota 103 pode significar Plus nos formatos antigos ou solo nos modernos. A presença de 116 determina a interpretação automática; `star_power_note` ou `multiplier_note` em `song.ini` permite escolher 103/116, com precedência de `star_power_note`. Um arquivo moderno contendo solo 103, mas nenhum 116, é ambíguo: configure `star_power_note=116`. Solos são preservados como eventos, sem novas regras de gameplay.

O uso de base − 1 exige enhanced opens para não converter animações antigas, particularmente nota 59, em gameplay. `nf_open` conserva o significado existente: nenhuma cor pressionada, com strum ou HOPO conforme os outros bits. Um acorde misturando open e cores sem conversão SysEx é rejeitado.

Sustains menores que `PPQ/3` ticks são removidos por padrão; `sustain_cutoff_threshold` altera esse corte. Os fins originais permanecem no diagnóstico. Como `Note` tem uma só duração, acordes com durações diferentes usam o máximo e emitem aviso. O corte não é uma margem de erro temporal: sustains aceitos são calculados atravessando todos os segmentos de BPM.

## Tempo e HOPO

O adaptador usa `MidiFile::read`, `doTimeAnalysis`, `linkNotePairs`, `MidiEvent::seconds` e `getDurationInSeconds`. Ticks não são reconstruídos a partir de segundos. `delay` é aplicado uma vez às posições, sem deslocar durações nem as distâncias musicais usadas para HOPO.

`chart::classifyHopo`, em `ChartHopo.h`, compartilha a classificação estrutural com o loader Feedback. Cada formato fornece sua política temporal:

- `.chart` conserva o teste histórico em segundos/BPM com divisor 2.08, inclusive seu comportamento anterior de acordes.
- MIDI usa distância inclusiva de `floor(PPQ/3)+1` ticks por padrão. `eighthnote_hopo=true` usa `floor(PPQ/2)+1`; `hopo_frequency` positivo tem precedência e define ticks diretamente. Mudanças de BPM não alteram essa distância musical.
- A primeira nota, acordes, repetição da mesma pista e uma nota contida no acorde anterior exigem strum naturalmente no MIDI.
- Force HOPO/strum substitui o resultado natural. Se ambos cobrirem a nota, strum vence e há aviso. Tap é aplicado por último com os bits esperados pelo gameplay.

Essa política MIDI segue a convenção comunitária moderna; não promete reproduzir todas as versões históricas de GH, RB, FoFiX ou Clone Hero. A opção antiga `hopofreq` não é interpretada e gera aviso. Os dados musicais preservados permitem ampliar essas políticas sem mudar o gameplay.

## Robustez e dependência

Antes do parser upstream, uma passagem estrutural verifica limites de chunks, VLQs, running status, dados de canal, EOT, tempo e acumulação de ticks em `int`. Limites: 64 MiB, 256 trilhas, 2 milhões de eventos. Arquivos inválidos retornam erro e limpam o resultado. Notas jogáveis sem fim, instrumentos duplicados e frases de Plus sobrepostas são rejeitados; marcadores desconhecidos ou incompletos não jogáveis geram aviso quando pertinente. Type 2, SMPTE e extensões de contêiner fora do SMF aceito não são suportados.

`vendor/midifile` permanece em `98917df5b1bf0d6e8d4c0e5fff86d6b05343e793`, sem alterações upstream. CMake liga o target estático `midifile`, configura `BUILD_MIDILIBRARY_ONLY=ON` e exclui os exemplos do build padrão, incluindo exemplos que ignoram essa opção upstream. O C++11 do subdiretório não altera C++17 nos targets do Guitar++.

A licença BSD completa permanece no submodule, é copiada para `build/bin/licenses/midifile-LICENSE.txt` e instalada em `share/GuitarPP/licenses`. Distribuições binárias manuais devem incluir esse arquivo junto aos materiais distribuídos; distribuições de fontes devem conservar os avisos originais.

A validação com sanitizadores também motivou correções pequenas em caminhos antigos: limite de leitura no fim do buffer de `CText`, acesso seguro a metadados opcionais no compilador Feedback e rejeição de prefixos binários `.gpp` impossíveis antes de alocações cereal. Essa última verificação não constitui validação completa de todos os tamanhos internos de arquivos cereal arbitrários.

## Diagnóstico e testes

Ative `BUILD_CHART_TOOLS=ON` e use:

```sh
cmake --build build --target gpp_dump_chart gpp_midi_tests tests
build/bin/gpp_dump_chart inspect /caminho/notes.mid /tmp/chart.json
python3 tools/chart_evidence.py formats build/bin/gpp_dump_chart build/comparacao /pasta/da/musica
ctest --test-dir build --output-on-failure -R 'midi_core|SongLoading|Chart'
```

`inspect` exporta todas as dificuldades reconhecidas; arquivos MIDI sem partes compatíveis de cinco lanes são reconhecidos, mas não oferecem uma parte jogável. `formats` compara `.chart` com `notes.mid` e `notes.mid` com `original.mid`. A tolerância padrão é 1 ms, ajustável por `--tolerance`. O relatório compara instrumentos, ataques, pistas, acordes, HOPOs, taps, sustains agregados e por pista, Plus, seções, tempos e duração estimada até o último fim de nota. Inserções não deslocam toda a comparação subsequente.

O diretório de evidências deve ser novo. O script verifica hashes dos arquivos antes/depois. A varredura recursiva ocorre somente nessa ferramenta manual quando recebe uma pasta; não integra a descoberta runtime nem o CTest. Os modos históricos `capture` e `compare` mantêm snapshots exatos de `.chart`/`.gpp`.

Os testes MIDI geram SMFs sintéticos e não dependem de músicas, áudio ou contexto gráfico. `gpp_midi_tests` não liga o runtime do jogo. Os testes de integração de `SongLoading` verificam despacho, seleção determinística, dificuldades ausentes e preservação dos formatos antigos. Resultados das fixtures reais estão em [Validação MIDI](midi-validation.md).

## Limites e continuidade

Bateria, voz, teclado Pro (`PART REAL_KEYS_*`), GHL/seis pistas, pro guitar/bass, BRE, trills/tremolos e animações não são convertidos em gameplay. Trilhas continuam identificáveis e textos relevantes são conservados. Não há reprodução MIDI, exportação MIDI, migração do editor ou descoberta recursiva runtime. A interpretação de 104 e 103 é uma política de importação documentada, não detecção infalível da origem do arquivo.

Próximos passos: integrar instrumentos/dificuldades à futura interface; validar interativamente input, áudio e Plus com Mindflow; tratar sustains independentes por pista se o gameplay passar a representá-los; corrigir separadamente os casos históricos de `.chart` apontados pela comparação, com uma decisão explícita de compatibilidade.

## TODO: reconhecimento vocal sem gameplay

Não implementar gameplay vocal nesta etapa. Atualmente `MidiChart::trackNames`
preserva os nomes `PART VOCALS`, `HARM1`, `HARM2` e `HARM3`, e eventos de texto
continuam disponíveis para diagnóstico. Isso identifica a presença das trilhas,
mas não equivale a interpretar semanticamente um chart vocal.

Em uma etapa futura, expor capacidades vocais separadas da lista de instrumentos
jogáveis e, se necessário, importar alturas, letras, limites de frases e marcadores
de voz falada para uma representação própria. Não transformar pitches vocais em
cinco lanes, nem inserir essas partes em `gNotes`/`gPlus` do jogador de guitarra.
Testar vocais sem instrumentos, harmonias independentes e marcadores específicos
antes de habilitar a interpretação. A UI deve indicar "presente, não jogável".

Inventário local de 2026-10-14: entre 157 arquivos MIDI de `build/bin/data/songs`,
71 contêm `PART VOCALS`, 62 contêm `HARM1`, 62 contêm `HARM2` e 43 contêm `HARM3`.
São contagens de arquivos com essas trilhas, não garantia de conteúdo completo ou
de músicas únicas. Nenhum arquivo musical foi modificado por esta revisão.

## Referências de convenções

- [Mapeamento MIDI de guitarra de cinco pistas](https://github.com/TheNathannator/GuitarGame_ChartFormats/blob/main/docs/Chart-File-Formats/mid-format/Tracks/5-Fret-Guitar.md)
- [Tags de song.ini](https://github.com/TheNathannator/GuitarGame_ChartFormats/blob/main/docs/Chart-File-Formats/song-ini/Standard-Tags.md)
- [Cálculo comunitário de HOPO em scan-chart](https://github.com/Geomitron/scan-chart/blob/master/src/chart/natural-hopo.ts)
- [Loader de músicas do FoFiX](https://github.com/fofix/fofix/blob/master/fofix/game/song/song.py)
- [midifile](https://github.com/craigsapp/midifile/tree/98917df5b1bf0d6e8d4c0e5fff86d6b05343e793)

Rhythm, Co-op e Keys de cinco lanes usam o mesmo adaptador CPU das partes de
guitarra/baixo, incluindo as quatro dificuldades, sustains, modificadores e Plus.
Cada parte permanece independente; uma parte ausente não é substituída por outra.
Esta ampliação não altera julgamento, clocks, energia Plus ou seleção de stems.
