WindowWidth = 800
WindowHeight = 600
FullScreen = false

-- 0: original, 1: per-pixel lighting, 2: 512px shadows, 3: 1024px shadows
setGameVar("backgroundQuality", 2)
setGameVar("backgroundAO", true)
setGameVar("backgroundShadowBias", 0.001)

--customTitle = "teste"

setConfigs()
doNotRunAgain()
