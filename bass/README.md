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

No Windows x64, `python tools/ci/setup_bass_windows.py` (na raiz do projeto)
baixa os ZIPs oficiais e seleciona `c/bass.h`, `c/x64/bass.lib`, `x64/bass.dll`,
`C/bass_fx.h`, `C/x64/bass_fx.lib` e `x64/bass_fx.dll`. As DLLs são verificadas
como PE x64 e copiadas pelo CMake para o diretório dos executáveis antes da
descoberta dos testes. Não use as bibliotecas x86 da raiz dos ZIPs.

O empacotamento também exige `bass.txt` e `bass_fx.txt`, extraídos da raiz dos
respectivos ZIPs. O CI os inclui em `licenses/` junto às bibliotecas de runtime.
