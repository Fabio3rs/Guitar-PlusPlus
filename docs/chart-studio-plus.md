# Geração estratégica de Plus

O Chart Studio pode propor frases Plus depois de mapear o MIDI musical para lanes.
O gerador, a simulação e a edição usam C++ e funcionam sem áudio, janela ou
`GPPGame`. A geração é opt-in: a CLI exige `--plus auto`; a GUI separa a proposta
da aplicação. Nenhum perfil tenta reproduzir um jogo comercial.

## Uso pela interface

Abra um MIDI, selecione a pista e gere/revise as lanes normalmente. Em **Panels >
Plus**, marque **Enable Plus generation**, escolha o perfil e clique em **Analyze
and propose Plus**. Roxo indica proposta; azul indica frases aplicadas. A timeline
mostra também densidade em cinza, regiões estratégicas em laranja, energia em
ciano e ativações em verde. O gráfico de energia é a execução representativa 0,
não uma curva média que represente um jogador real.

Inspecione **Preview reasons**, os diagnósticos e a comparação da simulação antes
de clicar em **Apply Plus proposal**. A simulação exige acertar todos os ataques
da frase; suas limitações estão descritas abaixo. A aplicação é uma operação de
undo/redo.
**Save project** guarda as frases; **Export .chart** publica os eventos `S 2`
somente após validação pelo loader do jogo.

**Save Plus settings** persiste parâmetros sem aplicar uma distribuição. Desativar
a geração não apaga frases existentes. **Only selected tick region** utiliza os
limites de região definidos em Mapping. Frases que atravessam esses limites são
preservadas. Geração e simulação rodam sobre snapshots CPU canceláveis; resultados
de documentos alterados são descartados, e a aplicação verifica a identidade do
snapshot novamente.

A lista **Applied phrases** seleciona uma frase e navega até ela. Edite início e
fim exclusivo em ticks, depois **Save phrase edit**. Escolha proteção **Confirmed**
ou **Locked** para conservar decisões. Uma frase bloqueada precisa ser desbloqueada
antes de mover, redimensionar ou excluir. **New phrase from region** cria uma frase
manual. Toda intervenção manual fica preservada na regeneração, mesmo sem bloqueio.
A exclusão de notas pode tornar uma frase inválida: o projeto continua editável,
mas a exportação recusa frases vazias ou incompatíveis. O botão `?` reúne o manual
resumido dentro da interface.

## CLI e build

```sh
cmake -S . -B build -G Ninja -DBUILD_CHART_STUDIO=ON \
  -DBUILD_CHART_TOOLS=ON -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --target gpp_chart_studio gpp_score_to_chart gpp_dump_chart

./build/bin/gpp_score_to_chart convert 'build/tabs/Foghat - Slow Ride.mid' \
  --track 1 --plus auto --plus-profile strategic \
  --output build/chart-studio/SlowRide.plus.chart \
  --report build/chart-studio/SlowRide.plus.json

# Conservar exatamente o mapeamento de um projeto existente:
./build/bin/gpp_score_to_chart plus-generate \
  build/chart-studio/evaluation-v2/'Foghat - Slow Ride.gppchartproj' \
  --plus-profile sparse --plus-reserve .4 --plus-bars 2 --plus-spacing 12 \
  --output build/chart-studio/SlowRide.sparse.chart \
  --report build/chart-studio/SlowRide.sparse.json

./build/bin/gpp_score_to_chart plus-simulate build/chart-studio/SlowRide.plus.chart \
  --plus-trials 256 --plus-seed 42 --output build/chart-studio/simulation.json
```

`plus-analyze` produz o mesmo relatório completo de análise e simulação. Ambos
aceitam `.gppchartproj` ou charts de gameplay suportados pelo loader, incluindo
MIDI de gameplay; um MIDI musical deve passar por `convert` primeiro.
`--instrument '[ExpertSingle]'` seleciona a parte da referência.

Parâmetros compartilhados: `--plus-profile`, `--plus-reserve` (0–1),
`--plus-spacing` (compassos), `--plus-bars` (1 ou 2), `--plus-structure`,
`--plus-strategy`, `--plus-risk`, `--plus-trials`, `--plus-seed` e
`--begin-tick`/`--end-tick`. Os relatórios calculam os cinco modos de ativação e
quatro taxas de acerto. A GUI permite escolher a combinação exibida.

`plus-generate` cria um novo `.chart.gppchartproj` por padrão; `--project` escolhe
o destino. Não sobrescreve o projeto de entrada. Saídas existentes exigem
`--replace`; entradas MIDI, áudio e referências permanecem protegidas. Sem
`--plus auto`, a conversão mantém o comportamento anterior e conserva frases
já presentes no projeto.

## Algoritmo e perfis

O gerador usa ataques agrupados e a política de sustains que será exportada.
Janelas de 4, 8, 12 e 20 segundos estimam valor potencial, combinando ataques e
sustains. Máximos separados em pelo menos 20 segundos identificam várias regiões;
planaltos constantes recebem alvos distribuídos. Marcadores MIDI explícitos
`solo`, `solo_start` e `[solo_on]` podem acrescentar alvos. Nomes vagos de seções
não são interpretados como solos.

Os candidatos usam um ou dois compassos, respeitando numerador, denominador e
mudanças de compasso. Motivos já reconhecidos no mapeamento também fornecem
candidatos entre linhas de compasso. Mudanças métricas no meio de um compasso
reiniciam sua contagem; as fronteiras racionais são arredondadas a ticks sem
acumular erro por compasso. São exigidos pelo menos dois ataques para candidatos
automáticos, sem longas pausas internas. A edição manual admite um ataque.

Uma busca regressiva prepara a reserva de cada alvo, reavaliando o saldo carregado
com a política de guardar para regiões densas. Depois preenche lacunas úteis e
faz refinamento discreto limitado a quatro passagens de até 64 avaliações por
passagem, por remoção/troca. A função global considera reserva, benefício,
saturação, risco e quantidade desejável de oportunidades. Não é um ótimo global;
o teste exaustivo pequeno verifica a viabilidade da preparação com duas frases,
não uma prova de otimalidade do gerador inteiro.

| Perfil | Reserva | Medidas preferidas | Espaçamento desejado |
|---|---:|---:|---:|
| Balanced | 40% | 1 | 8 compassos |
| Strategic | 40% | 1 | 8 compassos |
| Generous | 40% | 1 | 6 compassos |
| Sparse | 40% | 2 | 12 compassos |

Todos consideram candidatos de uma e duas medidas. Strategic dobra o peso de
preparação em relação a Balanced; Generous reduz a penalidade de risco. Pesos
iniciais estrutura/estratégia/risco: 2/4/2 em Strategic, 2/2/2 em Balanced,
2/4/1 em Generous. Sparse mantém 2/4/2. O espaçamento é uma preferência: preparar
uma região pode exigir oportunidades mais próximas. O refinamento penaliza risco
e saturação, mas não garante uma frequência determinada.

Risco é um índice de projeto: número de ataques, velocidade média, proporção de
acordes e deslocamento entre centros de lanes. Não é probabilidade humana. O
horizonte inicial de preparação é de 32 compassos; metas impossíveis são relatadas,
sem inventar notas. Empates seguem ordem canônica e utilidade quantizada. O teste
com offset confirma que transladar a gravação não altera a distribuição musical.
Escala temporal pode mudar as métricas; os ticks das frases já aplicadas não mudam.

## Simulação e contrato real

As constantes em `src/PlusRules.h` são compartilhadas com o runtime: máximo 1,
recompensa `maxPlusPower / 5`, consumo 0,05/s. O jogo continua permitindo ativação
com qualquer energia positiva. Não houve alteração de julgamento, combo,
pontuação, relógios ou condição de recompensa.

O runtime **invalida frases por erros**, além de verificar o último ataque para
conceder a recompensa. São caminhos distintos:

- `CPlayer::processError()` chama `breakPlus()`. Dentro do intervalo temporal
  da frase corrente, `breakPlus()` avança `Notes.plusPos` e zera o indicador de
  progresso, descartando essa oportunidade de recompensa.
- `CPlayer::processErrorNonPickedB()` verifica notas anteriores não tocadas e
  pode avançar o mesmo cursor quando encontra uma falha dentro da frase. Esse
  caminho tem suas próprias condições temporais; não é uma varredura irrestrita
  de todas as notas desde o início da frase.
- `CGamePlay::updatePlayer()` concede energia ao acertar `lastNote` da frase
  ainda corrente, quando a barra não está cheia. O último ataque é o gatilho
  da recompensa, não a única condição de sucesso. O cursor também avança por
  expiração temporal da frase.

A descrição anterior de que o jogo ignorava erros anteriores estava incorreta:
ela considerava apenas o caminho de recompensa e omitia a invalidação em
`CPlayer`. Não se deve deduzir desse trecho isolado que basta acertar a última
nota para obter Plus.

O simulador usa somente o modelo **sequence**, que exige todos os ataques da
frase. Ele não emula frames, janelas de input, palhetadas extras ou todas as
condições de fronteira do jogo. O antigo modo chamado `runtime`, que considerava
somente o último ataque, foi removido por não representar o gameplay. Relatórios
antigos que incluam essa variante não devem ser usados como previsão do runtime;
gere novamente o relatório para obter apenas as 20 combinações válidas de cinco
políticas de ativação e quatro taxas de acerto.

A simulação começa com energia zero por padrão; a API CPU também admite energia
inicial. Recompensa é creditada no último ataque, antes de terminar seu sustain,
e não beneficia retroativamente o ataque que a concedeu. Ativações continuam até
esgotamento, drenam durante pausas e podem receber novas recompensas. Saturação
é contabilizada como energia desperdiçada.

Políticas: guardar para regiões densas, ativar imediatamente, esperar reserva,
busca aproximada de valor futuro e nunca ativar. A busca futura compara ataques
em janelas de oito segundos até vinte segundos à frente; é uma referência com
conhecimento do futuro, não limite superior matemático.

As taxas 100%, 90%, 75% e 50% são **por ataque**. O modelo independente exige todos
os ataques de uma frase; sua chance é `p^n`. São 256 execuções por padrão e semente
42; com 0%/100% basta uma execução. Sorteios usam identidade das notas e índice da
execução/ataque, sem incluir a distribuição de Plus, permitindo comparação pareada.

Benefício usa pontos base normalizados a multiplicador 1: 50 por ataque e 50/s
sustentado, sem multiplicar por gems. O modelo aproxima retenção de sustains,
considera a tolerância atual de 130 ms e não duplica duração sustentada simultânea.
Não reproduz combo, release de cada fret ou execução humana validada.

## Persistência e fronteiras de exportação

O projeto SQLite v3 acrescenta blocos nomeados `plus` e `musical_markers`, sem
reordenar o payload antigo. Projetos v1/v2 são lidos com Plus vazio/desabilitado;
a migração só é escrita ao salvar. Biblioteca de padrões continua no schema v1.
Não há mudança na serialização histórica `.gpp` nem na identidade dos motivos.

Frases usam `[início, fim)`. O loader histórico impõe duas precauções verificadas
por testes: `N` deve aparecer antes de `S 2` no mesmo tick, e deve existir um ataque
fora das frases entre frases sucessivas, pois seu cursor consome esse ataque ao
avançar. O studio impõe essas restrições e uma margem de 75 ms entre último ataque
e fim, para não expirar a frase junto do ataque. Não modifica o loader para
acomodar saídas inválidas. Após exportar, confere `gPlus`, `firstNote`, `lastNote`,
notas, lanes, sustains e tempos, com tolerância temporal de 1 µs.

Essa validação utiliza os ataques compatíveis com a exportação, sem executar o
planejador estratégico. O limite de 100000 compassos pertence à análise de
geração, não à exportação de charts com ou sem frases já existentes.

Referências usam os índices reais do loader na simulação e na contagem de ataques
das frases no relatório, mesmo quando diferem de
um intervalo ideal. Sem evidência de ticks/compassos, como em alguns `.gpp`, o
adaptador usa reconstrução temporal e avisa que fronteiras musicais não são
autoritativas. Métricas comparativas de compasso vêm do analisador existente.

## Avaliação e testes

```sh
ctest --test-dir build -R 'chart_studio|chart_plus_statistics' --output-on-failure
python3 tools/chart_studio/experiments/plus_compare.py \
  build/chart-studio/evaluation-v2/*.gppchartproj \
  --cli build/bin/gpp_score_to_chart --dumper build/bin/gpp_dump_chart \
  --output-dir build/chart-studio/plus-evaluation/example
```

O driver congela lanes, compara quatro perfis, repete Strategic em outro processo,
confere SHA-256 dos projetos/referências e chama `plus_stats.py` para as métricas
estatísticas, sem duplicar o analisador. `--reference` pode ser repetido. O HTML
compara simulações e aponta para o relatório estatístico offline existente.
A tabela de reserva média não deve ser confundida com probabilidade de atingir
a reserva. Nenhuma métrica de semelhança com GH3/RB4 é usada para otimizar perfis.

Na avaliação local inicial foram usados 12 projetos anteriores, mais Magia e
Honesty alinhadas, totalizando 56 charts, com referências Slow Ride/GH3 e
Destructive Device/RBN. Cliffs of Dover tinha `.gp5`, mas não MIDI musical local.
Arquivos derivados e relatórios ficam somente em `build/`, fora do Git.

Na execução de 14/10/2026, o perfil Strategic e a política de guardar para regiões
densas, com acerto de 100%, produziram:

| Entrada | Frases | Regiões com reserva de pelo menos 40% |
|---|---:|---:|
| Slow Ride | 18 | 7/7 |
| Through the Fire and Flames | 42 | 12/12 |
| Magia | 17 | 6/7 |
| Honesty | 23 | 6/8 |
| Destructive Device (duas transcrições) | 30 / 26 | 5/8 / 4/6 |

Esses resultados medem preparação sob uma política específica, não diversão ou
superioridade sobre referências. O modelo de sequência completa perde mais
frases quando a taxa de acerto cai; os relatórios incluem essa sensibilidade.
Todos os 56 charts passaram pelo loader real, conservaram notas e BPM entre
perfis e repetiram o resultado Strategic em processos separados. O analisador
existente carregou 58 arquivos, incluindo duas referências, sem erros.

Relatório local: `build/chart-studio/plus-evaluation/batch/comparison.html`, com
JSON, charts e projetos correspondentes no mesmo diretório. Os hashes de entradas
e referências foram conferidos para assegurar que permaneceram intactas.

Validação final: 309/309 testes CTest no build normal. No NativeLab, a suíte com
ASan/UBSan/LSan passou 296/296 testes; depois dos ajustes finais, os sete testes
agrupados do Chart Studio foram repetidos e passaram, incluindo a interface com
Xvfb/Mesa software. Foram usadas as supressões já existentes para dependências
externas. O fluxo visual cobre prévia sem aplicação, aplicar, undo/redo,
salvar/reabrir e exportar. `clang-format` foi aplicado ao código tocado;
`clang-tidy` focado em análise, bugs e desempenho terminou sem diagnósticos no
código revisado, excluindo padding e parâmetros potencialmente intercambiáveis.
Logs ficam em `build/chart-studio/plus-evaluation/quality/`.

Limitações: heurística global limitada, não otimização exata; análise de intensidade
não reconhece semanticamente refrões; o risco e a execução são modelos experimentais.
Ajustes finos de perfis, comparação perceptual e diversão precisam de escuta e
playtest. Windows utiliza as rotinas existentes, mas esta validação foi Linux.
