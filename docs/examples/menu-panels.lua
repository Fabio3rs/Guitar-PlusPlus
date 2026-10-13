-- Synthetic content for ui::readMenus / ui::instantiate.
-- The host attaches these definitions with pushPanel; these are not Quick Play.
return {
    {
        id = "exampleResults",
        flow = 1, flowX = -0.6, flowY = 0.5, flowGroup = 1,
        viewportEnabled = true,
        viewport = {left = -0.6, bottom = -0.7, right = 0.7, top = 0.7},
        tableColumns = {0.7, 0.6},
        style = {rowStep = 0.12, textSize = 0.06},
        options = {
            {id = "first", type = 9, group = 1,
             cells = {"Canção de exemplo", "Artista"}, active = true},
            {id = "second", type = 9, group = 1,
             cells = {"Outro exemplo", "Outro artista"}},
        },
    },
    {
        id = "examplePreview",
        options = {
            {id = "background", type = 11, x = 0.8, y = -0.7,
             width = 0.6, height = 1.4,
             style = {background = {r = 0.03, g = 0.04, b = 0.05, a = 0.9},
                      border = {r = 0.5, g = 0.5, b = 0.5, a = 1},
                      borderWidth = 0.004}},
            -- Empty texture is intentional: no external artwork is required.
            {id = "cover", type = 10, x = 0.85, y = 0.2,
             width = 0.5, height = 0.4, texture = ""},
            {id = "title", type = 8, x = 0.85, y = 0.1,
             text = "Prévia de exemplo", style = {textSize = 0.055}},
            {id = "difficulty", type = 3, x = 0.85, y = -0.1,
             text = "Expert", width = 0.5, height = 0.1, active = true},
        },
    },
}
