# Experimento opcional: contexto musical → UMAP → cinco lanes

O algoritmo padrão continua sendo `beam`. O experimento usa **umap-learn real**
para projetar vetores de contexto musical em um eixo ordenado. A busca discreta
C++ recebe essa projeção como alvo e continua escolhendo máscaras válidas de
cinco lanes. Não há treinamento com charts profissionais, nem seleção de sementes
para aproximar suas lanes. Open notes não fazem parte deste experimento.

## Arquitetura e cache

Tudo fica em `tools/chart_studio/`. `experiment.cpp` lê um artefato JSON versionado;
`experiments/umap_projection.py` faz a projeção fora do executável. O runtime não
recebe dependência de Python, UMAP ou ImGui. A GUI e a CLI compartilham o mesmo
otimizador nativo, exportador e validação pelo parser do jogo.

A ordem de busca do cache não mudou. Bloqueios e confirmações continuam tendo
precedência. Somente decisões novas recebem o alvo UMAP; a cardinalidade dos
acordes, os custos de transição e a política de sustains continuam iguais aos do
baseline. Isso permite avaliar a projeção sem misturar mudanças de simplificação.

O hash do conteúdo do artefato integra o nome `umap:<hash>` no fingerprint já
existente. Alterar semente/projeção invalida resultados **experimentais**, mantendo
decisões confirmadas e bloqueadas. Não existe um segundo banco de dados. O artefato
contém assinaturas canônicas verificáveis e alvos inteiros; o importador verifica
hash do MIDI, pista/canal/programa, dificuldade, cobertura e tamanho das frases.
Arquivos incompatíveis falham explicitamente, sem fallback silencioso.

O projeto salva a identidade da projeção, mas não incorpora seu conteúdo. Pode ser
aberto, editado e exportado sem Python; para regenerar em modo UMAP é necessário
recarregar o artefato correspondente. Na GUI: **Algorithm → UMAP (experimental)**,
informar **UMAP artifact**, clicar **Load projection**, depois **Generate**.

## Vetores e determinismo

São 24 características por ataque: alturas superior, inferior e média relativas
à frase; extensão e tamanho do acorde; intervalos melódicos anterior e seguinte;
durações mínima e máxima das vozes; distâncias temporais aos ataques vizinhos;
fase rítmica relativa ao início da frase; e 12 indicadores de intervalos do acorde
relativos ao baixo. Alturas são normalizadas/clampadas e ritmos usam beats do MIDI.
Velocity, lanes geradas e charts de referência não entram nos vetores.

Uma representante por assinatura estrutural evita sobrepesar refrões repetidos.
Vetores idênticos são deduplicados e ordenados antes do ajuste. UMAP usa distância
euclidiana, um componente, `min_dist=0.1`, inicialização aleatória, uma thread e
sementes explícitas. Por padrão são 15 vizinhos (limitados ao número de contextos)
e 150 épocas. A reflexão arbitrária do eixo é orientada pela altura musical de
origem. Os percentis 5–95 são normalizados e quantizados em inteiros de 0 a 48;
os centros das cinco lanes correspondem a 0, 12, 24, 36 e 48. A busca aplica custos
de contorno e ergonomia ao escolher a sequência final.

É um alvo suave, não uma atribuição direta de cada cluster a uma cor. A redução
para um eixo pode perder relações harmônicas, e o resultado pode variar bastante
entre sementes. Menos de três contextos distintos ou uma projeção degenerada
produzem erro explícito.

Semente fixa e execução com uma thread controlam a reprodução no mesmo ambiente;
não prometemos igualdade numérica do ajuste Python entre arquiteturas/versões.
O artefato registra versões e parâmetros, e permite repetir o mapeamento nativo
sem executar UMAP novamente. Consulte a documentação oficial de
[reprodutibilidade](https://umap-learn.readthedocs.io/en/latest/reproducibility.html)
e [parâmetros](https://umap-learn.readthedocs.io/en/latest/parameters.html).

## Executar uma conversão

Dependências Python são opcionais e instaladas separadamente; não são baixadas
pelo CMake. Exemplo com Python 3.12, além do build habitual do Chart Studio:

```sh
python3 -m venv build/umap-venv
build/umap-venv/bin/pip install -r tools/chart_studio/experiments/requirements.txt
mkdir -p build/chart-studio/my-umap
build/bin/gpp_score_to_chart features "build/tabs/Foghat - Slow Ride.mid" \
  --track 1 --output build/chart-studio/my-umap/features.json
build/umap-venv/bin/python tools/chart_studio/experiments/umap_projection.py \
  build/chart-studio/my-umap/features.json --seed 42 \
  --output build/chart-studio/my-umap/projection.json
build/bin/gpp_score_to_chart convert "build/tabs/Foghat - Slow Ride.mid" \
  --track 1 --algorithm umap \
  --projection build/chart-studio/my-umap/projection.json \
  --output build/chart-studio/my-umap/slow-ride.chart
```

No Windows, use `build/umap-venv/Scripts/python.exe`. Os comandos `features` e
`events PROJECT` exportam JSON para inspeção. `features` usa a segmentação nativa
e uma geração baseline em memória; o script não lê suas máscaras. Nenhuma decisão
é promovida automaticamente à biblioteca global. Use caminhos novos para cada
experimento; saídas existentes são protegidas.

## Comparação controlada

Crie um manifesto JSON com seleção explícita, por exemplo:

```json
[{"midi":"build/tabs/Foghat - Slow Ride.mid","track":1}]
```

```sh
build/umap-venv/bin/python tools/chart_studio/experiments/compare.py \
  --cli build/bin/gpp_score_to_chart --manifest manifest.json \
  --output build/chart-studio/new-umap-evaluation --seeds 42 43 44
```

O diretório de saída deve ser novo. O relatório guarda hash dos MIDIs/artefatos,
configuração, métricas e checks de reprodução. Cada variante começa em projeto
novo sem biblioteca global. Isso evita que decisões antigas escondam o efeito do
algoritmo. Depois se testa novamente com o cache persistido. A primeira semente
também é reajustada em outro processo Python e comparada byte a byte.

As métricas são descritivas, sem uma pontuação agregada de qualidade:

- **Contorno:** proporção de movimentos das lanes no mesmo sentido que a voz
  superior do MIDI; reversões são reportadas separadamente. Movimentos planos não
  contam como acerto nem reversão. Só pares com altura diferente entram no divisor.
- **Ergonomia aproximada:** deslocamento médio do centro das lanes, proporção de
  saltos maiores que duas lanes, mudanças de frets por XOR e proporção de ataques
  cuja extensão de acorde excede duas lanes. Não substitui playtest.
- **Repetição:** igualdade de máscaras para pitches consecutivos iguais e
  igualdade das sequências nas ocorrências da mesma assinatura estrutural.
- **Estabilidade:** igualdade de tick, máscara e remoção por ID entre execuções
  e entre sementes. Uma saída estável pode ainda ser musicalmente ruim.

Transições só entram nas métricas locais quando separadas por até dois beats;
silêncios maiores ficam fora. Categorias sem exemplos retornam `null`. Ataques,
pares melódicos, pares repetidos e grupos de frases acompanham os resultados.
Os relatórios não usam charts profissionais como alvo ou critério de seleção.

## Testes opcionais

```sh
cmake -S . -B build -DBUILD_CHART_STUDIO_UMAP_TESTS=ON \
  -DCHART_STUDIO_UMAP_PYTHON="$PWD/build/umap-venv/bin/python"
ctest --test-dir build -R '^chart_studio' --output-on-failure
```

`test_experiment.py` usa MIDI sintético: reprodução, invariância à transposição e
às máscaras de saída, cache persistido, confirmação/bloqueio, artefatos inválidos e preservação
de saídas em falhas. O teste C++ verifica também reabertura de projeto, cobertura
incompleta e identidade de entrada. A opção Python fica OFF por padrão; os testes
nativos da integração não precisam instalar UMAP.

No GitHub Actions, `BuildGuitarPP.yml` ativa explicitamente essa opção e instala
`requirements.txt` em um venv próprio com Python 3.12 do Ubuntu 24.04. O job roda
`pip check` e verifica os imports antes do build; os testes usam somente fixtures
sintéticas. A instalação do jogo e os demais workflows não exigem UMAP. Os três
workflows instalam SQLite, PNG e as dependências nativas de seus respectivos
studios; BASS/BASS_FX continuam sendo obtidos dos arquivos oficiais x86-64.

## Limites e próximos experimentos

A igualdade de frases repetidas é garantida pelo agrupamento/cache existente,
não demonstra uma capacidade aprendida pelo UMAP. O teste de contorno usa a voz
superior como aproximação; MIDI polifônico com vozes cruzadas merece avaliação
separada. As métricas de ergonomia não modelam anatomia, velocidade de dedos ou
strumming. As duas versões de Magia são entradas de avaliação, não evidências
independentes de qualidade.

Antes de considerar trocar o padrão, experimentar uma mistura explícita entre
o alvo de altura e o alvo UMAP, medir a sensibilidade aos vizinhos/às sementes e
inspecionar trechos com reversões de contorno. Uma restrição de contorno pode ser
mais útil do que aumentar a dimensão da projeção. A escolha de acordes de duas
ou três lanes é outro experimento: nesta rodada a quantidade de lanes não mudou.
Qualquer ajuste deve continuar comparando o mesmo MIDI e as mesmas configurações,
com avaliação auditiva/manual, sem usar charts profissionais como objetivo.

## Avaliação local — 13/10/2026

Executados os 12 MIDIs locais, com seleção explícita de pista, Expert, pesos
padrão, cutoff de sustain de meia batida e sementes 42, 43 e 44. A tabela mostra
a semente 42 sem selecioná-la por qualidade; estabilidade mostra o intervalo das
três comparações entre sementes. Contorno em %, movimento em lanes por transição.

| MIDI | Track | Contorno beam → UMAP | Movimento beam → UMAP | Estabilidade entre sementes |
|---|---:|---:|---:|---:|
| DragonForce - Through The Fire And Flames | 6 | 95.2 → 90.2 | 1.15 → 0.76 | 39.0–47.8% |
| Foghat - Slow Ride | 1 | 95.3 → 86.1 | 0.98 → 0.56 | 11.9–53.9% |
| Heart - Barracuda | 3 | 95.9 → 88.4 | 0.47 → 0.43 | 8.5–15.8% |
| Kalafina - Magia (r571207) | 1 | 98.3 → 98.3 | 1.11 → 0.81 | 24.6–44.5% |
| Kalafina - Magia | 1 | 98.3 → 98.3 | 1.11 → 0.81 | 24.6–44.5% |
| Mindflow - 01 - Destructive Device | 1 | 99.4 → 95.9 | 1.55 → 1.06 | 32.4–54.8% |
| Mindflow - Destructive Device | 1 | 99.3 → 97.7 | 1.45 → 1.05 | 37.3–44.4% |
| Mindflow - Honesty | 1 | 98.2 → 94.2 | 1.37 → 0.96 | 42.8–51.7% |
| Muse - Knights Of Cydonia | 4 | 94.4 → 92.7 | 0.59 → 0.50 | 18.9–44.8% |
| Paramore - Still Into You | 3 | 94.2 → 72.6 | 0.50 → 0.31 | 11.0–62.9% |
| Slipknot - Before I Forget | 2 | 100.0 → 96.1 | 1.10 → 0.94 | 15.7–40.8% |
| Weezer - My Name Is Jonas | 6 | 97.7 → 90.4 | 0.66 → 0.52 | 18.6–42.0% |

Os 36 casos UMAP reproduziram o `.chart` byte a byte em projetos novos e ao
reabrir o cache persistido (reutilização de 100%). Os 12 reajustes Python da
semente 42 reproduziram o artefato byte a byte. Beam e UMAP mantiveram 100% de
consistência nos grupos de frases repetidas. Todos os exports passaram pela
validação do parser nativo. Isso comprova reprodução neste ambiente, não
independência de plataforma do UMAP.

Nesta configuração, o UMAP reduziu o movimento médio, mas frequentemente piorou
o contorno e mostrou grande sensibilidade à semente. **Não há evidência para
promovê-lo a algoritmo padrão.** Em Honesty, por exemplo, saltos maiores que duas
lanes caíram de 11,4% para 1,1%, enquanto o contorno caiu de 98,2% para 94,2%.

Artefatos completos locais: `build/chart-studio/umap/evaluation-v1/summary.json`
e subdiretórios `00`–`11` com baseline, três charts, projetos, projeções e relatórios.
O manifesto fica em `build/chart-studio/umap/manifest.json`. Nenhum arquivo de
`songs` foi substituído; nenhum MIDI comercial foi adicionado ao repositório.

Validação desta alteração: cinco entradas CTest do studio passaram (28 testes
C++ de núcleo, dois de integração com runtime, CLI, GUI e integração Python UMAP).
As quatro entradas nativas/GUI também passaram com ASan/UBSan no NativeLab, mais
a integração Python invocando a CLI sanitizada. GUI verificada via Xvfb/Mesa
software/OpenGL 2.0, inclusive abertura de projeto UMAP. Usadas as supressões já
existentes de vendor/Mesa. `clang-format --dry-run --Werror` passou; `clang-tidy`
com analyzer e checks selecionados de lifetime/expressões não apresentou achados
nas seis unidades de produção examinadas. Não equivale a passar todos os checks
de estilo do `.clang-tidy` nem a um playtest completo.
