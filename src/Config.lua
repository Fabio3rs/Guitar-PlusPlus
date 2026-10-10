-- Carregado do diretorio de trabalho ao iniciar o jogo.
-- Janela: defaults C++ < data/saves/settings.json < valores definidos aqui.
-- O jogo salva janela/idioma ao sair normalmente; nao reescreve este script.

-- Video. Dimensao 0 usa a resolucao do monitor. Valores negativos sao ignorados.
WindowWidth = 800
WindowHeight = 600
FullScreen = false
-- Borderless = false          -- Janela sem bordas, independente de FullScreen.
-- AntiAliasing = 0            -- Amostras MSAA: 0 desliga; 2/4/8 dependem da GPU.
-- customTitle = "Minha janela" -- Sufixo do titulo Guitar++.
-- VSync = VSYNC_AUTO          -- OFF=0, ON=1, HALF=2, AUTO=3.
-- Limitacao atual: menus/gameplay tambem alteram VSync depois da criacao da janela.

-- Controles: codigos numericos de teclado GLFW, usados por setConfigs().
-- Exemplo Q/W/E/R/T; controles de jogador/perfil podem ter seus proprios caminhos.
-- Green = 81
-- Red = 87
-- Yellow = 69
-- Blue = 82
-- Orange = 84
-- fretOne = 265               -- Seta para cima (palhetada).
-- fretTwo = 264               -- Seta para baixo (palhetada).

-- Gameplay. Estas opcoes continuam sendo configuradas por Lua, nao pelo save.
-- hyperSpeed = 1.0            -- Multiplicador positivo de velocidade da pista.
-- Song = "Nome da pasta"      -- data/songs/...; selecao no menu pode substituir.
-- setGameVar("botEnabled", false)
-- setGameVar("usarPalheta", true)
-- setGameVar("defaultGuitar", "guitar test") -- Nome da guitarra instalada.

-- Cenario: definicao em data/scenes/<id>.lua (garagem por padrao).
-- setScene("garage") -- Seleciona para o startup/proxima sessao, sem trocar no meio da musica.
-- Alternativa: setGameVar("backgroundScene", "garage").
-- Qualidade grafica: usar setGameVar, nao globais Lua de mesmo nome.
-- 0 original, 1 iluminacao, 2 sombras 512px, 3 sombras 1024px.
-- setGameVar("enableShaders", true)
setGameVar("backgroundQuality", 2)
setGameVar("backgroundAO", true) -- Usa atlas AO quando disponivel.
setGameVar("backgroundShadowBias", 0.001) -- Ajuste avancado das sombras.

-- Multiplayer legado; endereco e porta como strings.
-- setGameVar("multiplayerClientIP", "127.0.0.1")
-- setGameVar("multiplayerPort", "7777")

setConfigs() -- Aplica as variaveis globais acima a janela/controles/gameplay.
doNotRunAgain() -- Evita reexecucao automatica deste script.
