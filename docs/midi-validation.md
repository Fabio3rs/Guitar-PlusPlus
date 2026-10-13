# Validação MIDI no Guitar++

Validação realizada em 12 de outubro de 2026, com LLVM 21 já instalado, C++17 e `GPP_NATIVE_CPU=OFF`. Os processos do NativeLab foram reiniciados após a queda de energia. Os resultados abaixo correspondem às execuções concluídas após o reinício, incluindo as correções localizadas apontadas pelos sanitizadores.

## Builds e testes

| Verificação | Resultado |
| --- | --- |
| Build normal completo | Aprovado, incluindo GuitarPP, testes e gpp_dump_chart |
| CTest completo no NativeLab | 214/214, incluindo testes gráficos com Xvfb |
| Charts e carregamento com ASan + UBSan no NativeLab | 47/47; 14 casos MIDI isolados e 33 casos de integração/regressão |
| Snapshots runtime `.chart`/`.gpp` anteriores à implementação | 54 comparações, zero diferenças bit a bit |
| Varredura manual das músicas locais | 96 arquivos, zero falhas, cinco pares comparados |
| Mesma varredura manual com ASan + UBSan no NativeLab | 96 arquivos, zero falhas |
| Submodule midifile | Commit fixado e árvore upstream sem modificações |

ASan executou com `detect_leaks=1`; UBSan com `print_stacktrace=1:halt_on_error=1`. A suíte completa normal incluiu os testes gráficos existentes; o build com sanitizadores foi aplicado aos testes relevantes de charts e à varredura manual, não a toda a suíte gráfica.

Os testes sintéticos cobrem SMF Type 1 e Type 0, PPQ 96/192/480/960, mudanças e coincidência de BPM, sustain atravessando segmentos, duração zero, Note On de velocidade zero, reataques coincidentes, running status e canais, acordes, corte de sustain, HOPO inclusivo e limites, force HOPO/strum, tap, opens, Plus consecutivo e suas fronteiras, metadados, trilhas não suportadas, dificuldades ausentes, truncamento de todos os prefixos de uma fixture e entradas inválidas. Os testes de integração preservam os nomes alternativos e a seleção de formato.

Os arquivos originais foram apenas lidos. Os hashes antes/depois da varredura e os hashes do baseline conferem. Nenhuma música, áudio ou chart de terceiros foi adicionada ao Git. A coleção completa foi usada somente para validação manual, fora do CTest.

Evidências locais, ignoradas pelo Git:

- `build/midi-build.log`, `build/midi-full-tests.log`.
- `build/midi-sanitizer-build.log`, `build/midi-sanitizer-tests.log`.
- `build/midi-chart-baseline/manifest.json`, `before/` e `after/`.
- `build/midi-validation/report.txt`, `summary.json` e dumps normalizados por arquivo.
- `build/midi-validation-sanitized/` e `build/midi-sanitizer-survey.log`.

O build sanitizado permanece em `build-midi-sanitize/`, ignorado pelo Git. As mensagens antigas de link do BASS_FX e de `tmpnam` do Lua continuam aparecendo; não impediram a compilação. Não foi realizado playtest interativo com áudio, controles físicos ou avaliação de desempenho em hardware antigo.

## Mindflow Destructive Device

`notes.mid` foi carregado com PPQ 480 e `hopo_frequency=170` de `song.ini`. As oito partes reconhecidas são:

| Instrumento | Dificuldade | Ataques | Notas por pista | Acordes | Sustains | HOPOs | Plus |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Guitarra | Easy | 492 | 492 | 0 | 42 | 0 | 17 |
| Guitarra | Medium | 826 | 864 | 38 | 42 | 0 | 17 |
| Guitarra | Hard | 1304 | 1342 | 38 | 66 | 402 | 17 |
| Guitarra | Expert | 1659 | 1704 | 45 | 66 | 793 | 17 |
| Baixo | Easy | 527 | 527 | 0 | 26 | 0 | 17 |
| Baixo | Medium | 936 | 936 | 0 | 24 | 0 | 17 |
| Baixo | Hard | 1431 | 1432 | 1 | 140 | 354 | 17 |
| Baixo | Expert | 1769 | 1769 | 0 | 140 | 750 | 17 |

Um ataque é uma posição musical; um acorde reúne várias notas por pista. As frases de Star Power/Overdrive importadas são representadas como Plus.

`notes.mid` e `original.mid` não são equivalentes. O primeiro tem 61 eventos de tempo; o segundo, 11. As contagens de notas, acordes, sustains, HOPOs e Plus coincidem nas oito partes, mas há posições em ticks diferentes no final. Por exemplo, na guitarra Expert, tick 389999 em `notes.mid` corresponde a 390000 em `original.mid`.

A duração estimada da guitarra Expert é 398,953063492 s contra 398,954872500 s. A diferença máxima por posição ordenada é 1,809008 ms. Na comparação por tempo com tolerância de 1 ms, 1645 ataques coincidem e 14 de cada arquivo ficam sem par; isso não significa que faltam 14 notas. Nos pares alinhados não há diferenças de pistas, duração de sustain normalizado ou flags. As 17 frases de Plus e 20 seções têm correspondência dentro da tolerância.

## Pares de CHART e MIDI

Resultados da guitarra Expert. Valores com seta indicam `.chart` → `.mid`:

| Música | Ataques | Acordes | Sustains | HOPOs | Taps | Plus | Duração MIDI |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rusty Cooley War of the Angels | 2794 → 2795 | 178 | 162 | 2198 → 2177 | 0 | 19 | 417,546577 s |
| Rock You Like a Hurricane | 843 | 394 | 107 → 121 | 266 → 234 | 0 → 21 | 0 | 253,061 s |
| Slow Ride | 551 | 193 | 148 | 63 → 55 | 0 → 17 | 0 | 248,475 s |
| Welcome to the Jungle | 937 | 302 | 134 | 219 → 183 | 0 → 13 | 0 | 270,893 s |

**Rusty Cooley:** 2794 ataques alinhados, nenhum com pista ou sustain diferente acima de 1 ms; diferença temporal máxima de 0,253318 ms. O MIDI possui um ataque adicional no tick 200640, pista vermelha, a 224,3318325 s. Essa nota também consta no texto `.chart`, mas o loader histórico mistura a nota com o registro `S 2 1920` anterior no mesmo tick e a perde. O MIDI preserva a nota. Há 22 diferenças de flags e nove frases com índices de limite distintos. A política HOPO MIDI e os limites de Plus calculados em ticks explicam parte da divergência; o comportamento legado de `.chart` foi mantido deliberadamente e comprovado pelo baseline. As 48 seções coincidem.

**Rock You Like a Hurricane:** todos os ataques e pistas coincidem temporalmente, mas 14 sustains normalizados diferem, com delta máximo de 237 ms. O MIDI possui durações que o resultado `.chart` não conserva, inclusive em notas individuais; não se deve atribuir todas essas diferenças à regra de duração máxima dos acordes. Há 46 diferenças combinadas de flags, incluindo os 21 taps da nota MIDI 104 e a política HOPO. Não há frases de Plus nos arquivos desse par.

**Slow Ride:** `Chart.chart` conserva sua prioridade sobre `notes.mid`. Ataques, pistas e durações normalizadas coincidem; 22 ataques têm flags diferentes, incluindo 17 taps do MIDI e diferenças da política HOPO. Não há Plus nesse par.

**Welcome to the Jungle:** ataques, pistas e durações normalizadas coincidem; 39 ataques têm flags diferentes, incluindo 13 taps do MIDI. Não há Plus nesse par.

Nos três últimos pares, as diferenças de tempo dos ataques são inferiores a 0,000001 ms. Durações brutas de notas MIDI curtas frequentemente diferem de notas `.chart` com duração zero; o corte de sustain faz muitas delas convergirem no gameplay. O relatório distingue essas diferenças brutas das durações normalizadas.

## Coleção Guitar Hero III

A coleção `Guitar Hero III - Legends of Rock/` contém 75 arquivos MIDI em subpastas de músicas. Todos foram importados na varredura manual. A biblioteca runtime continua sem varredura recursiva.

Nomes iguais em pastas diferentes não garantem charts iguais. Por exemplo, o MIDI de Slow Ride dentro da coleção tem nove frases de Plus, 300 eventos de tempo e 551 ataques com sustain; o par da pasta superior tem zero Plus, um evento de tempo e 148 sustains normalizados. Os relatórios preservam o caminho e hash de cada entrada, sem consolidar versões pelo título.

## Arquivos da implementação

- `src/CMidiChart.h` e `.cpp`: adaptador MIDI, metadados e normalização.
- `src/ChartSource.h`: evidências musicais preservadas, sem alteração de serialização.
- `src/ChartHopo.h`: classificador estrutural compartilhado.
- `src/CChart.h` e `.cpp`: entrada MIDI, seleção de candidato válido e diagnóstico do formato antigo; correções pontuais encontradas por ASan.
- `src/CPlayer.cpp`: despacho de `.mid`.
- `src/ui/SelectionMenus.cpp`: usa a validade já verificada pela descoberta, sem consulta redundante ao cabeçalho.
- `src/CText.cpp`: impede leitura além do buffer em texto legado terminado por caracteres não imprimíveis.
- `CMakeLists.txt`: biblioteca estática, isolamento C++17 e distribuição da licença.
- `tests/CMakeLists.txt`, `tests/MidiFixture.h`, `tests/midi_chart_tests.cpp`, `tests/song_loading_tests.cpp`: fixtures sintéticas e regressões.
- `tools/dump_chart.cpp`, `tools/chart_evidence.py`: inspeção normalizada e comparação manual.
- `.gitignore`: diretório local do build sanitizado.
- `docs/midi-support.md` e este relatório: convenções, limites, uso e validação.

As entradas `.gitmodules` e `vendor/midifile` já estavam preparadas no índice antes desta implementação e permaneceram no commit selecionado. Os próximos passos e as convenções ainda não implementadas estão em [Suporte MIDI](midi-support.md).
