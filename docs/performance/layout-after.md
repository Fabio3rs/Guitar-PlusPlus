# Member layout

Offsets and line numbers are relative to an object at a 64-byte boundary.
Natural alignment does not guarantee that address; actual sharing depends on allocation.
Container sizes describe headers, not separately allocated storage.

## CPlayer

Size 1672; alignment 8; inter-member/tail padding 21 (nested padding excluded).

| Member | Byte interval (inclusive) | Size | Relative cache lines |
|---|---:|---:|---:|
| `points` | 0–7 | 8 | 0–0 |
| `combo` | 8–15 | 8 | 0–0 |
| `experience` | 16–23 | 8 | 0–0 |
| `songScoreStart` | 24–31 | 8 | 0–0 |
| `songCorrectStart` | 32–39 | 8 | 0–0 |
| `songMaxCombo` | 40–47 | 8 | 0–0 |
| `BPMNowUpdate` | 48–55 | 8 | 0–0 |
| `npPsetted` | 56–63 | 8 | 0–0 |
| `lastHOPO` | 64–67 | 4 | 1–1 |
| `musicRunningTime` | 72–79 | 8 | 1–1 |
| `lastFretsPressed` | 80–84 | 5 | 1–1 |
| `bRenderP` | 85–85 | 1 | 1–1 |
| `bUpdateP` | 86–86 | 1 | 1–1 |
| `bPlusStrike` | 87–87 | 1 | 1–1 |
| `fretsPressed` | 88–92 | 5 | 1–1 |
| `plusEnabled` | 93–93 | 1 | 1–1 |
| `canDoHOPO` | 94–94 | 1 | 1–1 |
| `fretsPressedTime` | 96–135 | 40 | 1–2 |
| `strklinenttime` | 136–143 | 8 | 2–2 |
| `lastNotePlusType` | 144–147 | 4 | 2–2 |
| `notesSlide` | 152–191 | 40 | 2–2 |
| `strklinent` | 192–199 | 8 | 3–3 |
| `palhetaKeyLast` | 200–203 | 4 | 3–3 |
| `palhetaKey` | 204–207 | 4 | 3–3 |
| `usarPalheta` | 208–208 | 1 | 3–3 |
| `enableBot` | 209–209 | 1 | 3–3 |
| `aError` | 210–210 | 1 | 3–3 |
| `remoteControls` | 211–211 | 1 | 3–3 |
| `startTime` | 216–223 | 8 | 3–3 |
| `plusThunterStrikeStart` | 224–231 | 8 | 3–3 |
| `plusPower` | 232–239 | 8 | 3–3 |
| `plusParticleEffectPosition` | 240–247 | 8 | 3–3 |
| `maxPlusPower` | 248–255 | 8 | 3–3 |
| `plusLoadF` | 256–263 | 8 | 4–4 |
| `plusLoadB` | 264–271 | 8 | 4–4 |
| `plusLoadInterval` | 272–279 | 8 | 4–4 |
| `publicAprov` | 280–287 | 8 | 4–4 |
| `correctNotes` | 288–295 | 8 | 4–4 |
| `correctNotesMarathon` | 296–303 | 8 | 4–4 |
| `maxPublicAprov` | 304–311 | 8 | 4–4 |
| `Notes` | 312–671 | 360 | 4–10 |
| `songAudioID` | 672–675 | 4 | 10–10 |
| `instrumentSound` | 676–679 | 4 | 10–10 |
| `instrumentAudioName` | 680–711 | 32 | 10–11 |
| `backingStems` | 712–759 | 48 | 11–11 |
| `activeAudioStems` | 760–783 | 24 | 11–12 |
| `backingVolume` | 784–787 | 4 | 12–12 |
| `playerHudOffsetX` | 792–799 | 8 | 12–12 |
| `playerHudOffsetY` | 800–807 | 8 | 12–12 |
| `guitar` | 808–815 | 8 | 12–12 |
| `lvlUpAlert` | 816–895 | 80 | 12–13 |
| `notesStreakAlert` | 896–975 | 80 | 14–15 |
| `playerCamera` | 976–1047 | 72 | 15–16 |
| `targetCamera` | 1048–1119 | 72 | 16–17 |
| `playerSave` | 1120–1151 | 32 | 17–17 |
| `playerName` | 1152–1183 | 32 | 18–18 |
| `plname` | 1184–1215 | 32 | 18–18 |
| `playerType` | 1216–1219 | 4 | 19–19 |
| `multiPlayerInfo` | 1224–1239 | 16 | 19–19 |
| `instrument` | 1240–1271 | 32 | 19–19 |
| `songName` | 1272–1303 | 32 | 19–20 |
| `songArtist` | 1304–1335 | 32 | 20–20 |
| `songCharter` | 1336–1367 | 32 | 20–21 |
| `songFullPath` | 1368–1399 | 32 | 21–21 |
| `renderState` | 1400–1671 | 272 | 21–26 |

## CGamePlay

Size 704; alignment 8; inter-member/tail padding 10 (nested padding excluded).

| Member | Byte interval (inclusive) | Size | Relative cache lines |
|---|---:|---:|---:|
| `engine` | 0–7 | 8 | 0–0 |
| `speedMp` | 8–15 | 8 | 0–0 |
| `gSpeed` | 16–23 | 8 | 0–0 |
| `startSongTime` | 24–31 | 8 | 0–0 |
| `fretboardLightFade` | 32–39 | 8 | 0–0 |
| `fireText` | 40–43 | 4 | 0–0 |
| `pfireText` | 44–47 | 4 | 0–0 |
| `BPMTextID` | 48–51 | 4 | 0–0 |
| `hopoLightText` | 52–55 | 4 | 0–0 |
| `enableTails` | 56–56 | 1 | 0–0 |
| `bFretboardLightFading` | 57–57 | 1 | 0–0 |
| `showBPMLines` | 58–58 | 1 | 0–0 |
| `bRenderHUD` | 59–59 | 1 | 0–0 |
| `hopostp` | 64–87 | 24 | 1–1 |
| `BPMl` | 88–167 | 80 | 1–2 |
| `fretboardLData` | 168–247 | 80 | 2–3 |
| `hoposLight` | 248–331 | 84 | 3–5 |
| `plusNoteLight` | 332–415 | 84 | 5–6 |
| `players` | 416–439 | 24 | 6–6 |
| `chartInstruments` | 440–463 | 24 | 6–7 |
| `songlyrics` | 464–543 | 80 | 7–8 |
| `songlyricsIndex` | 544–551 | 8 | 8–8 |
| `fretsTextures` | 552–583 | 32 | 8–9 |
| `BPMLineText` | 584–615 | 32 | 9–9 |
| `backgroundTexture` | 616–647 | 32 | 9–10 |
| `preRenderPlayerSEvent` | 648–651 | 4 | 10–10 |
| `posRenderPlayerSEvent` | 652–655 | 4 | 10–10 |
| `bIsACharterGP` | 656–656 | 1 | 10–10 |
| `showBPMVlaues` | 657–657 | 1 | 10–10 |
| `activeDelta` | 664–679 | 16 | 10–10 |
| `updateLastTimeCalled` | 680–687 | 8 | 10–10 |
| `lastiFPSUpdated` | 688–695 | 8 | 10–10 |
| `iFPS` | 696–699 | 4 | 10–10 |
| `iFPSCount` | 700–703 | 4 | 10–10 |

## GPPGame

Size 2056; alignment 8; inter-member/tail padding 33 (nested padding excluded).

| Member | Byte interval (inclusive) | Size | Relative cache lines |
|---|---:|---:|---:|
| `songResults` | 0–23 | 24 | 0–0 |
| `devMode` | 24–24 | 1 | 0–0 |
| `mainthread` | 32–39 | 8 | 0–0 |
| `menus` | 40–511 | 472 | 0–7 |
| `glanguage` | 512–543 | 32 | 8–8 |
| `drawGamePlayBackground` | 544–544 | 1 | 8–8 |
| `showTextsTest` | 545–545 | 1 | 8–8 |
| `songVolume` | 548–551 | 4 | 8–8 |
| `textureManager` | 552–559 | 8 | 8–8 |
| `textureViewsMutex` | 560–599 | 40 | 8–9 |
| `textureViews` | 600–647 | 48 | 9–10 |
| `unavailableTexture` | 648–791 | 144 | 10–12 |
| `SPR` | 792–839 | 48 | 12–13 |
| `gThemes` | 840–887 | 48 | 13–13 |
| `gameCallbacks` | 888–935 | 48 | 13–14 |
| `gameCallbacksWrapper` | 936–983 | 48 | 14–15 |
| `gameModules` | 984–1031 | 48 | 15–16 |
| `cmdparams` | 1032–1079 | 48 | 16–16 |
| `modelManager` | 1080–1087 | 8 | 16–16 |
| `basicModels` | 1088–1111 | 24 | 17–17 |
| `noteOBJ` | 1112–1135 | 24 | 17–17 |
| `triggerBASEOBJ` | 1136–1159 | 24 | 17–18 |
| `triggerOBJ` | 1160–1183 | 24 | 18–18 |
| `pylmbarOBJ` | 1184–1207 | 24 | 18–18 |
| `openNoteOBJ` | 1208–1231 | 24 | 18–19 |
| `strumsTexture3D` | 1232–1255 | 24 | 19–19 |
| `hopoTexture3D` | 1256–1279 | 24 | 19–19 |
| `tapTexture3D` | 1280–1303 | 24 | 20–20 |
| `sbaseTexture3D` | 1304–1327 | 24 | 20–20 |
| `striggerTexture3D` | 1328–1351 | 24 | 20–21 |
| `openNoteTexture3D` | 1352–1355 | 4 | 21–21 |
| `openNoteHOPOTexture3D` | 1356–1359 | 4 | 21–21 |
| `openNotePTexture3D` | 1360–1363 | 4 | 21–21 |
| `openNoteHOPOPTexture3D` | 1364–1367 | 4 | 21–21 |
| `strumKeys` | 1368–1387 | 20 | 21–21 |
| `fretOneKey` | 1388–1391 | 4 | 21–21 |
| `fretTwoKey` | 1392–1395 | 4 | 21–21 |
| `HUDText` | 1396–1399 | 4 | 21–21 |
| `fretboardText` | 1400–1403 | 4 | 21–21 |
| `lineText` | 1404–1407 | 4 | 21–21 |
| `HOPOSText` | 1408–1411 | 4 | 22–22 |
| `pylmBarText` | 1412–1415 | 4 | 22–22 |
| `songToLoad` | 1416–1447 | 32 | 22–22 |
| `errorsSound` | 1448–1467 | 20 | 22–22 |
| `startSound` | 1468–1471 | 4 | 22–22 |
| `endSound` | 1472–1475 | 4 | 23–23 |
| `hyperSpeed` | 1480–1487 | 8 | 23–23 |
| `botEnabled` | 1488–1488 | 1 | 23–23 |
| `usarPalheta` | 1489–1489 | 1 | 23–23 |
| `defaultGuitar` | 1496–1527 | 32 | 23–23 |
| `gameplayRunningTime` | 1528–1535 | 8 | 23–23 |
| `gamePlayPlusEnabled` | 1536–1536 | 1 | 24–24 |
| `marathonSongsList` | 1544–1623 | 80 | 24–25 |
| `testobj` | 1624–1647 | 24 | 25–25 |
| `scene` | 1648–1871 | 224 | 25–29 |
| `backgroundScene` | 1872–1903 | 32 | 29–29 |
| `mainPlayer` | 1904–1919 | 16 | 29–29 |
| `windowChangedParams` | 1920–1920 | 1 | 30–30 |
| `windowCFGs` | 1928–1983 | 56 | 30–30 |
| `settings` | 1984–2055 | 72 | 31–32 |

## PlayerRenderState

Size 272; alignment 8; inter-member/tail padding 11 (nested padding excluded).

| Member | Byte interval (inclusive) | Size | Relative cache lines |
|---|---:|---:|---:|
| `tailsData` | 0–23 | 24 | 0–0 |
| `BPMNowBuffer` | 24–31 | 8 | 0–0 |
| `rangle` | 32–39 | 8 | 0–0 |
| `plusCircleBuffer` | 40–43 | 4 | 0–0 |
| `plusLoadBuffer` | 44–47 | 4 | 0–0 |
| `publicApprovBuffer` | 48–51 | 4 | 0–0 |
| `correctNotesBuffer` | 52–55 | 4 | 0–0 |
| `multiplierBuffer` | 56–59 | 4 | 0–0 |
| `playerParticles` | 64–263 | 200 | 1–4 |
| `plusLightning` | 264–264 | 1 | 4–4 |

