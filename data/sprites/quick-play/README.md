# Ícones do Quick Play

Cinco desenhos SVG originais, criados por **Codex (OpenAI)** em 13/10/2026
para esta tarefa do Guitar++. Não foram extraídos dos conceitos, de bancos
de ícones ou de capas. Não alteram o logotipo original do jogo.

| Arquivo-base | Uso | Controle |
| --- | --- | --- |
| `folders` | Pasta com aba superior | `selectSongMenu.tabIcon0` |
| `artist` | Pessoa/artista | `selectSongMenu.tabIcon1` |
| `music` | Notas musicais | `selectSongMenu.tabIcon2` |
| `charter` | Folha de chart | `selectSongMenu.tabIcon3` |
| `no-cover` | Placeholder neutro, sem capa inventada | `quickPlayPreview.coverPlaceholder` |

Cada nome possui o SVG editável e sua exportação TGA RGBA de 64×64 pixels.
O manifesto desta tabela é documental; os cinco nomes são carregados pelo host
em `SelectionMenus.cpp`, através do gerenciador de texturas existente. Não há
novo formato de tema nem carregador paralelo. Os TGAs devem ser distribuídos em
`data/sprites/quick-play/`; o CMake copia os cinco arquivos para
`build/bin/data/sprites/quick-play/` na configuração do build.

Reexportar cada desenho (Inkscape + ImageMagick):

```sh
inkscape data/sprites/quick-play/folders.svg --export-width=64 --export-height=64 --export-background-opacity=0 --export-filename=/tmp/qp-folders.png
convert /tmp/qp-folders.png -flip -alpha on -type TrueColorAlpha data/sprites/quick-play/folders.tga
```

**Orientação:** o `-flip` é intencional: compensa a orientação de sprites da UI
existente. A aba superior da pasta e a dobra superior da folha foram conferidas
nas capturas reais. Não remover a inversão sem rever o consumidor.

São cinco texturas pequenas (80 KiB RGBA antes de eventual alocação do driver),
carregadas pelo cache existente. Texturas individuais evitam alterar o contrato
do controle `image`, que hoje desenha a textura completa. Se o conjunto crescer,
um atlas e recortes UV, semelhantes aos das fontes, podem reduzir trocas de
textura; essa mudança não faz parte desta rodada.
