# Revisão do Chart Studio e Slow Ride

## Achados

1. **Corrigido: todos os ataques viravam sustains.** O exportador anterior
   preservava qualquer duração positiva do MIDI. Isso é fiel ao Note Off,
   mas não é uma política de charting adequada para ataques curtos. O padrão
   agora exige duração maior que meia batida, com limite ajustável. O score,
   as decisões de lanes e os bloqueios permanecem intactos.
2. **Corrigido: preview precisava refletir a política de exportação.** Timeline,
   snapshot para comparação e validação round-trip agora compartilham a função
   `exportDuration`. Piano roll continua mostrando o MIDI original.
3. **Cuidado na comparação: MIDI profissional contém caudas técnicas mínimas.**
   A referência GH3 possui 407 caudas abaixo de 10 ms, aproximadamente 1 ms.
   Seu `song.ini` define `sustain_cutoff_threshold = 0`. Não são comparáveis às
   caudas de 100–500 ms da geração anterior. O campo `sustains` da comparação
   genérica ainda conta toda duração positiva; nesta análise separamos esse caso.
4. **Pendente: excesso de acordes de três lanes.** O otimizador fixa a cardinalidade
   como `min(pitches distintos, maxChord)`. Assim, o peso “Chord complexity”
   penaliza principalmente a abertura da forma, sem realmente escolher entre
   duas e três lanes para um acorde rico. É uma limitação musical concreta:
   precisamos separar vozes relevantes, estrutura harmônica e decisão ergonômica.
   Não foi mascarada por reduzir globalmente todo acorde a duas lanes.
5. **Pendente: qualidade musical não está demonstrada por determinismo.** O cache
   reproduz fielmente uma decisão, inclusive uma decisão ruim. Promover apenas
   padrões revisados continua essencial. Sem alinhamento local, a contagem total
   próxima também não comprova correspondência entre os ataques.

## Comparação executada

Referência: `build/bin/data/songs/Guitar Hero III - Legends of Rock/Quickplay/Foghat - Slow Ride/notes.mid`,
ExpertSingle, Neversoft, versão cover WaveGroup. Fonte musical: `build/tabs/Foghat - Slow Ride.mid`,
pista 1, Rod Price. Arquivos profissionais não foram modificados.

| Métrica | GH3 | Gerada antes | Gerada, corte > 0,5 batida |
|---|---:|---:|---:|
| Ataques | 551 | 545 | 545 |
| Notas simples | 358 | 226 | 226 |
| Acordes de duas lanes | 138 | 7 | 7 |
| Acordes de três lanes | 55 | 312 | 312 |
| Caudas acima de 10 ms | 144 | 545 | 103 |
| Caudas entre zero e 10 ms | 407 | 0 | 0 |

O ajuste removeu **442 caudas**, sem mudar nenhum instante de ataque ou máscara.
A segunda conversão do projeto revisado reutilizou 545/545 decisões. O chart
revisado passou pelo loader real do jogo.

A expressão “acima de 10 ms” serve somente para separar caudas técnicas nesta
análise; não é o limite musical adotado pelo gerador. O GH3 mantém algumas caudas
curtas intencionais e não segue um único corte universal.

Sensibilidade do corte estrito, medida nos ticks originais (PPQ 960):

| Limite em batidas | Sustains restantes |
|---:|---:|
| 0,125 | 545 |
| 0,25 | 501 |
| 0,5 | 103 |
| 0,75 | 91 |
| 1 | 52 |
| 1,5 | 43 |
| 2 | 30 |

Uma contagem preliminar em segundos deu 387 para meia batida, incorretamente:
valores exatamente na fronteira foram classificados como maiores por arredondamento.
A conferência em ticks corrigiu a análise e motivou testes específicos de fronteira.

Primeiro ataque: GH3 4,141 s; gerada 4,210526 s. Último ataque: GH3 247,470 s;
gerada 247,368421 s. O MIDI musical tem BPM constante de 114; a referência possui
300 eventos de tempo. Não foi inferido um offset único nem corrigido o andamento.
Essas diferenças são toleradas e o alinhamento fica para trabalho posterior.

Artefatos locais: `build/chart-studio/review/SlowRide.review.chart`, projeto de
mesmo nome, `slowride-comparison.json` e dumps produzidos por `gpp_dump_chart`.

## Revisão de código e ferramentas

- clang-format-20 aplicado aos arquivos novos e ao CCharter refatorado;
  verificação com `--dry-run --Werror`.
- clang-tidy-20 executado com a configuração do projeto no studio, documento
  compartilhado, validador MIDI, CCharter, CMidiChart e GPPGame. Logs em
  `build/chart-studio/review/tidy-*.log`. A análise não usou `-fix` indiscriminado.
- Corrigidos no código novo: conversões implícitas de coordenadas/custos,
  alocações evitáveis no mapa de tempo do CCharter, passagem desnecessária por
  valor e identificador reservado no include guard. A thread continua recebendo
  uma cópia própria do caminho via `std::thread`.
- Permanecem recomendações de estilo (números literais, casts, DTOs públicos,
  funções extensas) e avisos legados de GPPGame/CMidiChart. Não foi feita uma
  reformatação nem uma reescrita geral do runtime para satisfazer esses avisos.
- Validação: 53/53 testes CTest selecionados passaram no build normal e
  53/53 com ASan/UBSan via NativeLab, incluindo Xvfb/software rendering. Foram
  usadas as supressões existentes específicas de GLFW/Mesa, sem novas supressões.
- Novos testes verificam a fronteira inclusiva, PPQ ímpar, mudança de BPM,
  sustentação longa sobreposta, política em segundos, limite configurável,
  preservação do score e leitura pelo parser real.

## Vetores musicais e open notes: direção para experimento

A proposta seguinte é representar ataques/frases por vetores de intervalos,
registro relativo, ritmo, duração, estrutura dos acordes e continuidade das
vozes. Um buscador sequencial deve transformar esses vetores em ações válidas,
com precedência dos padrões confirmados e bloqueados.

UMAP pode ser avaliado para visualização/exploração das vizinhanças, mas suas
dimensões não equivalem a cinco botões. Ele não substitui a etapa de escolha de
máscaras, sustains e restrições de gameplay. A documentação descreve redução de
dimensionalidade e cuidados de reprodutibilidade:
[UMAP](https://umap-learn.readthedocs.io/en/latest/basic_usage.html),
[reprodutibilidade](https://umap-learn.readthedocs.io/en/latest/reproducibility.html).
Nenhuma dependência UMAP foi adicionada nesta revisão.

Conforme a orientação de design do usuário, open note seria uma ação rara para
notas muito graves fora do registro habitual da frase, não uma sexta lane comum
nem uma regra global “pitch baixo = open”. Precisará de limite de frequência,
restrições de transição e representação explícita no documento/cache. O runtime
já possui open notes, mas o gerador do studio continua limitado a cinco lanes.
