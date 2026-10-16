# Member layout

Offsets and line numbers are relative to an object at a 64-byte boundary.
Natural alignment does not guarantee that address; actual sharing depends on allocation.
Container sizes describe headers, not separately allocated storage.

## CPlayer

Size 1680; alignment 8; inter-member/tail padding 32 (nested padding excluded).

| Member | Byte interval (inclusive) | Size | Relative cache lines |
|---|---:|---:|---:|
| `points` | 0–7 | 8 | 0–0 |
| `combo` | 8–15 | 8 | 0–0 |
| `experience` | 16–23 | 8 | 0–0 |
| `songScoreStart` | 24–31 | 8 | 0–0 |
| `songCorrectStart` | 32–39 | 8 | 0–0 |
| `songMaxCombo` | 40–47 | 8 | 0–0 |
| `Notes` | 48–407 | 360 | 0–6 |
| `BPMNowUpdate` | 408–415 | 8 | 6–6 |
| `tailsData` | 416–439 | 24 | 6–6 |
| `rangle` | 440–447 | 8 | 6–6 |
| `npPsetted` | 448–455 | 8 | 7–7 |
| `updatedMusicRunningTime` | 456–463 | 8 | 7–7 |
| `lastHOPO` | 464–467 | 4 | 7–7 |
| `BPMNowBuffer` | 472–479 | 8 | 7–7 |
| `musicRunningTime` | 480–487 | 8 | 7–7 |
| `lastFretsPressed` | 488–492 | 5 | 7–7 |
| `bRenderP` | 493–493 | 1 | 7–7 |
| `bUpdateP` | 494–494 | 1 | 7–7 |
| `bPlusStrike` | 495–495 | 1 | 7–7 |
| `fretsPressed` | 496–500 | 5 | 7–7 |
| `plusEnabled` | 501–501 | 1 | 7–7 |
| `canDoHOPO` | 502–502 | 1 | 7–7 |
| `fretsPressedTime` | 504–543 | 40 | 7–8 |
| `strklinenttime` | 544–551 | 8 | 8–8 |
| `lastNotePlusType` | 552–555 | 4 | 8–8 |
| `notesSlide` | 560–599 | 40 | 8–9 |
| `strklinent` | 600–607 | 8 | 9–9 |
| `palhetaKeyLast` | 608–611 | 4 | 9–9 |
| `palhetaKey` | 612–615 | 4 | 9–9 |
| `usarPalheta` | 616–616 | 1 | 9–9 |
| `enableBot` | 617–617 | 1 | 9–9 |
| `aError` | 618–618 | 1 | 9–9 |
| `remoteControls` | 619–619 | 1 | 9–9 |
| `startTime` | 624–631 | 8 | 9–9 |
| `plusThunterStrikeStart` | 632–639 | 8 | 9–9 |
| `plusPower` | 640–647 | 8 | 10–10 |
| `plusParticleEffectPosition` | 648–655 | 8 | 10–10 |
| `maxPlusPower` | 656–663 | 8 | 10–10 |
| `plusLoadF` | 664–671 | 8 | 10–10 |
| `plusLoadB` | 672–679 | 8 | 10–10 |
| `plusLoadInterval` | 680–687 | 8 | 10–10 |
| `publicAprov` | 688–695 | 8 | 10–10 |
| `correctNotes` | 696–703 | 8 | 10–10 |
| `correctNotesMarathon` | 704–711 | 8 | 11–11 |
| `maxPublicAprov` | 712–719 | 8 | 11–11 |
| `plusCircleBuffer` | 720–723 | 4 | 11–11 |
| `plusLoadBuffer` | 724–727 | 4 | 11–11 |
| `publicApprovBuffer` | 728–731 | 4 | 11–11 |
| `correctNotesBuffer` | 732–735 | 4 | 11–11 |
| `multiplierBuffer` | 736–739 | 4 | 11–11 |
| `songAudioID` | 740–743 | 4 | 11–11 |
| `instrumentSound` | 744–747 | 4 | 11–11 |
| `instrumentAudioName` | 752–783 | 32 | 11–12 |
| `backingStems` | 784–831 | 48 | 12–12 |
| `activeAudioStems` | 832–855 | 24 | 13–13 |
| `backingVolume` | 856–859 | 4 | 13–13 |
| `playerHudOffsetX` | 864–871 | 8 | 13–13 |
| `playerHudOffsetY` | 872–879 | 8 | 13–13 |
| `playerParticles` | 880–1079 | 200 | 13–16 |
| `plusLightning` | 1080–1080 | 1 | 16–16 |
| `guitar` | 1088–1095 | 8 | 17–17 |
| `lvlUpAlert` | 1096–1175 | 80 | 17–18 |
| `notesStreakAlert` | 1176–1255 | 80 | 18–19 |
| `playerCamera` | 1256–1327 | 72 | 19–20 |
| `targetCamera` | 1328–1399 | 72 | 20–21 |
| `playerSave` | 1400–1431 | 32 | 21–22 |
| `playerName` | 1432–1463 | 32 | 22–22 |
| `plname` | 1464–1495 | 32 | 22–23 |
| `playerType` | 1496–1499 | 4 | 23–23 |
| `multiPlayerInfo` | 1504–1519 | 16 | 23–23 |
| `instrument` | 1520–1551 | 32 | 23–24 |
| `songName` | 1552–1583 | 32 | 24–24 |
| `songArtist` | 1584–1615 | 32 | 24–25 |
| `songCharter` | 1616–1647 | 32 | 25–25 |
| `songFullPath` | 1648–1679 | 32 | 25–26 |

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

