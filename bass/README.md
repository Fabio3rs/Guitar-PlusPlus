Baixe BASS e BASS_FX no site oficial da Un4seen e coloque aqui `bass.h`,
`bass_fx.h` e as respectivas bibliotecas `.so` da arquitetura desejada.
No Linux x86-64, os ZIPs contêm `c/bass.h`, `C/bass_fx.h` e
`libs/x86_64/libbass*.so`.

Validação local do Audio Workbench: BASS **2.4.18.3** (`0x02041203`) e
BASS_FX **2.4.12.6** (`0x02040c06`). O ciclo repetido de sustentação com
fade/reinício travou com a antiga BASS 2.4.14.1 no Native Lab e passou com
2.4.18.3; veja `docs/audio-studio-validation.md`. Não basta conferir os
headers: registre `BASS_GetVersion()` e `BASS_FX_GetVersion()` do binário carregado.
Atualize também cópias locais usadas por executáveis, como `build/bin/libbass*.so`.
As bibliotecas continuam externas ao Git e nenhuma atualização de glibc é exigida.
