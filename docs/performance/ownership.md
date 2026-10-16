# Inventário de ownership

U = worker de gameplay; R = main/OpenGL; A = tarefa BPM `std::async`; L = loader; T = inicialização/transição quiescente; MP = callbacks multiplayer Windows.

Classificação por membro direto (inclui removidos). Funções agrupadas por papel; não é uma prova de ausência de acessos indiretos. Os cabeçalhos STL não descrevem as alocações que apontam.

## CPlayer

| Membro | Escritores/leitores e frequência | Evidência/limite |
|---|---|---|
| `points` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `combo` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `experience` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `songScoreStart` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `songCorrectStart` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `songMaxCombo` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `Notes` | misto | U: notePos/plusPos/lastNotePicked/fretsNotePickedTime/flags. R: leitura. L/editor: vetores/BPM/metadados. Antes: async também escrevia BPMMinPosition/chartEnd; removido do render. |
| `BPMNowUpdate` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `tailsData` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `rangle` | antes U→R; depois R | Antes updatePlayer escrevia cada update; agora cálculo uma vez por renderPlayer no cache visual. |
| `npPsetted` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `updatedMusicRunningTime` | R, redundante | Escrito por renderPlayer/reset; nenhum leitor encontrado; removido. |
| `lastHOPO` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `BPMNowBuffer` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `musicRunningTime` | U→R, quente | update/updatePlayer escrevem; renderPlayer/renderBackground leem. Editor escreve em execução sequencial; risco legado. |
| `lastFretsPressed` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `bRenderP` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `bUpdateP` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `bPlusStrike` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `fretsPressed` | U→R, quente | update/updatePlayer escrevem; renderPlayer/renderBackground leem. Editor escreve em execução sequencial; risco legado. |
| `plusEnabled` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `canDoHOPO` | frio/sem consumidor ativo | Inicialização/reset apenas nas referências atuais; mantidos, sem limpeza incidental. |
| `fretsPressedTime` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `strklinenttime` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `lastNotePlusType` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `notesSlide` | U→R, quente | update/updatePlayer escrevem; renderPlayer/renderBackground leem. Editor escreve em execução sequencial; risco legado. |
| `strklinent` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `palhetaKeyLast` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `palhetaKey` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `usarPalheta` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `enableBot` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `aError` | U, quente | CPlayer::update e CGamePlay::updatePlayer; resets no loader/main quiescente. |
| `remoteControls` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `startTime` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `plusThunterStrikeStart` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `plusPower` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `plusParticleEffectPosition` | frio/sem consumidor ativo | Inicialização/reset apenas nas referências atuais; mantidos, sem limpeza incidental. |
| `maxPlusPower` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `plusLoadF` | U→R, atômico | updatePlayer escreve F/B; HUD lê B. Interval só inicializado. Atomicidade individual não publica outros campos. |
| `plusLoadB` | U→R, atômico | updatePlayer escreve F/B; HUD lê B. Interval só inicializado. Atomicidade individual não publica outros campos. |
| `plusLoadInterval` | U→R, atômico | updatePlayer escreve F/B; HUD lê B. Interval só inicializado. Atomicidade individual não publica outros campos. |
| `publicAprov` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `correctNotes` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `correctNotesMarathon` | U→R, quente | updatePlayer/doNote/processError/addPoints escrevem; renderPlayer/HUD/resultados leem. Sem publicação, salvo fronteiras quiescentes. |
| `maxPublicAprov` | T/L→U/R, frio | Construtor/reset/transições configuram; gameplay/render/resultados consultam. Não são setters concorrentes novos. |
| `plusCircleBuffer` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `plusLoadBuffer` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `publicApprovBuffer` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `correctNotesBuffer` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `multiplierBuffer` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `songAudioID` | L/T, frio; U lê IDs | loadSongAudio/releaseSong/takeLoadedSong/playSongAudio/volume; U usa instrumentPause/mute/unmute; estruturas de stems não são alteradas pelo render. |
| `instrumentSound` | L/T, frio; U lê IDs | loadSongAudio/releaseSong/takeLoadedSong/playSongAudio/volume; U usa instrumentPause/mute/unmute; estruturas de stems não são alteradas pelo render. |
| `instrumentAudioName` | L/T, frio; U lê IDs | loadSongAudio/releaseSong/takeLoadedSong/playSongAudio/volume; U usa instrumentPause/mute/unmute; estruturas de stems não são alteradas pelo render. |
| `backingStems` | L/T, frio; U lê IDs | loadSongAudio/releaseSong/takeLoadedSong/playSongAudio/volume; U usa instrumentPause/mute/unmute; estruturas de stems não são alteradas pelo render. |
| `activeAudioStems` | L/T, frio; U lê IDs | loadSongAudio/releaseSong/takeLoadedSong/playSongAudio/volume; U usa instrumentPause/mute/unmute; estruturas de stems não são alteradas pelo render. |
| `backingVolume` | L/T, frio; U lê IDs | loadSongAudio/releaseSong/takeLoadedSong/playSongAudio/volume; U usa instrumentPause/mute/unmute; estruturas de stems não são alteradas pelo render. |
| `playerHudOffsetX` | R/T, visual | resetCamera/transições/câmera em GPPGame; render consulta. Alerts apenas inicialização/observadores legados. Ponteiros de recurso têm ownership externo. |
| `playerHudOffsetY` | R/T, visual | resetCamera/transições/câmera em GPPGame; render consulta. Alerts apenas inicialização/observadores legados. Ponteiros de recurso têm ownership externo. |
| `playerParticles` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `plusLightning` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |
| `guitar` | R/T, visual | resetCamera/transições/câmera em GPPGame; render consulta. Alerts apenas inicialização/observadores legados. Ponteiros de recurso têm ownership externo. |
| `lvlUpAlert` | R/T, visual | resetCamera/transições/câmera em GPPGame; render consulta. Alerts apenas inicialização/observadores legados. Ponteiros de recurso têm ownership externo. |
| `notesStreakAlert` | R/T, visual | resetCamera/transições/câmera em GPPGame; render consulta. Alerts apenas inicialização/observadores legados. Ponteiros de recurso têm ownership externo. |
| `playerCamera` | R/T, visual | resetCamera/transições/câmera em GPPGame; render consulta. Alerts apenas inicialização/observadores legados. Ponteiros de recurso têm ownership externo. |
| `targetCamera` | R/T, visual | resetCamera/transições/câmera em GPPGame; render consulta. Alerts apenas inicialização/observadores legados. Ponteiros de recurso têm ownership externo. |
| `playerSave` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `playerName` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `plname` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `playerType` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `multiPlayerInfo` | MP/T, frio | callbackFun escreve ready sob playersMutex; sessões consultam. Mutex não é um lock de render. |
| `instrument` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `songName` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `songArtist` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `songCharter` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `songFullPath` | T/L, frio | CChart/loadSong, menus, cereal playerName, áudio/editor; U lê plname no multiplayer. Formatos não alterados. |
| `renderState` | R/A, quente | renderPlayer/helpers escrevem caches/efeitos; drawBPMLines usa BPMNowBuffer em async seguido de join. Reset em transição quiescente; editor reinicia índice. Extraído para PlayerRenderState. |

## CGamePlay

| Membro | Escritores/leitores e frequência | Evidência/limite |
|---|---|---|
| `engine` | T→U/R/A | Construtor/setHyperSpeed/setMusicSpeed/compensatePause/editor. Clocks e velocidades configurados nas fronteiras; leituras no update/render/BPM. |
| `speedMp` | T→U/R/A | Construtor/setHyperSpeed/setMusicSpeed/compensatePause/editor. Clocks e velocidades configurados nas fronteiras; leituras no update/render/BPM. |
| `gSpeed` | T→U/R/A | Construtor/setHyperSpeed/setMusicSpeed/compensatePause/editor. Clocks e velocidades configurados nas fronteiras; leituras no update/render/BPM. |
| `startSongTime` | T→U/R/A | Construtor/setHyperSpeed/setMusicSpeed/compensatePause/editor. Clocks e velocidades configurados nas fronteiras; leituras no update/render/BPM. |
| `fretboardLightFade` | R, quente | renderPlayer/renderNote/renderHoposLight/renderFretBoard/renderLyrics escrevem. resetModule/configuração em T. |
| `fireText` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `pfireText` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `BPMTextID` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `hopoLightText` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `enableTails` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `bFretboardLightFading` | R, quente | renderPlayer/renderNote/renderHoposLight/renderFretBoard/renderLyrics escrevem. resetModule/configuração em T. |
| `showBPMLines` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `bRenderHUD` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `hopostp` | R, quente | renderPlayer/renderNote/renderHoposLight/renderFretBoard/renderLyrics escrevem. resetModule/configuração em T. |
| `BPMl` | A→R, quente | drawBPMLines limpa/preenche; render aguarda future.get antes do draw. Tarefa delegada sequencial por jogador; não toca update. |
| `fretboardLData` | R, quente | renderPlayer/renderNote/renderHoposLight/renderFretBoard/renderLyrics escrevem. resetModule/configuração em T. |
| `hoposLight` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `plusNoteLight` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `players` | L/MP/T→U/R | loadThread/resetModule/installMarathonSong e callbacks MP; update segura playersMutex; render não. Vetor de shared_ptr possui alocação própria. |
| `chartInstruments` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `songlyrics` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `songlyricsIndex` | R, quente | renderPlayer/renderNote/renderHoposLight/renderFretBoard/renderLyrics escrevem. resetModule/configuração em T. |
| `fretsTextures` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `BPMLineText` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `backgroundTexture` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `preRenderPlayerSEvent` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `posRenderPlayerSEvent` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `bIsACharterGP` | T→U/R/A | Construtor/setHyperSpeed/setMusicSpeed/compensatePause/editor. Clocks e velocidades configurados nas fronteiras; leituras no update/render/BPM. |
| `showBPMVlaues` | T/L→R/A, frio | Construtor/loadSongLyrics/editor/resetModule; render consome. enableTails/event IDs/showBPMVlaues têm usos limitados ou ausentes nesta versão. |
| `activeDelta` | U, quente | update/marathonUpdate/startUpdateDelta; último chamado em fronteiras quiescentes. |
| `updateLastTimeCalled` | U→R, quente | update/marathonUpdate escrevem; HUD debug lê sem publicação; race legada mantida. |
| `lastiFPSUpdated` | U, quente | update/marathonUpdate/startUpdateDelta; último chamado em fronteiras quiescentes. |
| `iFPS` | U→R, quente | update/marathonUpdate escrevem; HUD debug lê sem publicação; race legada mantida. |
| `iFPSCount` | U, quente | update/marathonUpdate/startUpdateDelta; último chamado em fronteiras quiescentes. |

## GPPGame

| Membro | Escritores/leitores e frequência | Evidência/limite |
|---|---|---|
| `songResults` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `devMode` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `mainthread` | T→todos, atômico | Inicialização publica ID do owner; loaders/recursos consultam. |
| `menus` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `glanguage` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `drawGamePlayBackground` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `showTextsTest` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `songVolume` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `textureManager` | T/L, sincronizado em partes | GPPTexture/CTextureManager: mutex de views e protocolo interno; uploads/destruição GL no owner; ponteiros e mapas aqui são cabeçalhos. |
| `textureViewsMutex` | T/L, sincronizado em partes | GPPTexture/CTextureManager: mutex de views e protocolo interno; uploads/destruição GL no owner; ponteiros e mapas aqui são cabeçalhos. |
| `textureViews` | T/L, sincronizado em partes | GPPTexture/CTextureManager: mutex de views e protocolo interno; uploads/destruição GL no owner; ponteiros e mapas aqui são cabeçalhos. |
| `unavailableTexture` | T/L, sincronizado em partes | GPPTexture/CTextureManager: mutex de views e protocolo interno; uploads/destruição GL no owner; ponteiros e mapas aqui são cabeçalhos. |
| `SPR` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `gThemes` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `gameCallbacks` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `gameCallbacksWrapper` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `gameModules` | T/L/MP, lifecycle | Sessões/loadThread/prepared handoff; playersMutex e pauseRequest/pauseAck preservados. Dados indiretos têm ownership próprio. |
| `cmdparams` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `modelManager` | T/L, recursos | GPPModel/CModelManager: trabalho CPU delegado e publicação/upload pelo owner; layout do objeto não reorganizado. |
| `basicModels` | T/L, recursos | GPPModel/CModelManager: trabalho CPU delegado e publicação/upload pelo owner; layout do objeto não reorganizado. |
| `noteOBJ` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `triggerBASEOBJ` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `triggerOBJ` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `pylmbarOBJ` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `openNoteOBJ` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `strumsTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `hopoTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `tapTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `sbaseTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `striggerTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `openNoteTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `openNoteHOPOTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `openNotePTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `openNoteHOPOPTexture3D` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `strumKeys` | T→U, leitura frequente | Configuração inicial; CPlayer::update lê teclas e processError lê som. Não há escrita periódica concorrente nesses arrays. |
| `fretOneKey` | T→U, leitura frequente | Configuração inicial; CPlayer::update lê teclas e processError lê som. Não há escrita periódica concorrente nesses arrays. |
| `fretTwoKey` | T→U, leitura frequente | Configuração inicial; CPlayer::update lê teclas e processError lê som. Não há escrita periódica concorrente nesses arrays. |
| `HUDText` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `fretboardText` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `lineText` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `HOPOSText` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `pylmBarText` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `songToLoad` | T/L/MP, lifecycle | Sessões/loadThread/prepared handoff; playersMutex e pauseRequest/pauseAck preservados. Dados indiretos têm ownership próprio. |
| `errorsSound` | T→U, leitura frequente | Configuração inicial; CPlayer::update lê teclas e processError lê som. Não há escrita periódica concorrente nesses arrays. |
| `startSound` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `endSound` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `hyperSpeed` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `botEnabled` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `usarPalheta` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `defaultGuitar` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `gameplayRunningTime` | R→Lua, quente | Loop principal copia estado do jogador; getters Lua leem. Não são escritos pelo worker, embora a origem tenha races. |
| `gamePlayPlusEnabled` | R→Lua, quente | Loop principal copia estado do jogador; getters Lua leem. Não são escritos pelo worker, embora a origem tenha races. |
| `marathonSongsList` | T/L/MP, lifecycle | Sessões/loadThread/prepared handoff; playersMutex e pauseRequest/pauseAck preservados. Dados indiretos têm ownership próprio. |
| `testobj` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `scene` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `backgroundScene` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `mainPlayer` | T/L/MP, lifecycle | Sessões/loadThread/prepared handoff; playersMutex e pauseRequest/pauseAck preservados. Dados indiretos têm ownership próprio. |
| `windowChangedParams` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `windowCFGs` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |
| `settings` | T/R, frio ou recurso | Construtor/configuração/menu/render/scene/temas/settings em main; render consulta recursos. Sem evidência de escritores quentes em threads diferentes para este membro. |

