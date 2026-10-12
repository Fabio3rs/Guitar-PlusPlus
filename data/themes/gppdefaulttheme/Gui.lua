-- Declarative modal definitions shared with GUI Studio.
-- The runtime binds behavior by control ID; order and presentation are editable.
return {
    {
        id = "uiRenameMenu", modal = true,
        background = "data/sprites/interfacebg.tga",
        options = {
            { id = "title", type = 7, group = 2, x = -0.1, y = 0.15,
              listWidth = 1.5, style = { textSize = 0.1,
                textColor = { r = 63/255, g = 81/255, b = 181/255, a = 1 } } },
            { id = "name", type = 1, text = "aaaa", value = "type here", y = 0.05 },
            { id = "close", type = 3, text = "Close", y = -0.5, back = true,
              style = { textColor = { r = 1, g = 0, b = 0, a = 1 } } },
        },
    },
    {
        id = "uiCreateProfileMenu", modal = true,
        background = "data/sprites/interfacebg.tga",
        options = {
            { id = "title", type = 7, group = 2, x = -0.1, y = 0.15,
              listWidth = 1.5, style = { textSize = 0.1,
                textColor = { r = 63/255, g = 81/255, b = 181/255, a = 1 } } },
            { id = "name", type = 1, text = "aaaa", value = "type here", y = 0.05 },
            { id = "path", type = 8, text = "Path:", y = -0.05 },
            { id = "close", type = 3, text = "Close", y = -0.5, back = true,
              style = { textColor = { r = 1, g = 0, b = 0, a = 1 } } },
        },
    },
}
