-- Paths are relative to this file. Keep the existing garage assets in place.
-- Future shared models can use ../models/model.obj and scene-specific MTLs
-- can live next to this file or in a scene subdirectory.
return {
    model = "../../test/garage_gpp.obj",
    materials = "../../test/garage_gpp.mtl",
    lightPosition = {3.5, 2.88, -6.05},
    garageMaterials = true,

    -- Optional hooks run only at transitions, on the main/render thread.
    -- No animation updates run per frame in this version.
    -- onLoaded = function(context) end,
    -- onSessionStarted = function(context) end,
    -- onSessionEnded = function(context) end,
    -- onUnloaded = function(context) end,
}
