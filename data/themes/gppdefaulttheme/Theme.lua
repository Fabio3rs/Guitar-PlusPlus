-- Gui.lua in this directory supplies declarative modals after applyTheme.
-- Same fixed-cell atlas as the built-in font. Descenders are already in the image.
local function applyDefaultFont()
    assert(addTextureToFont("default", "data/sprites", "FONT.tga",
        "0123456789ABCDEFGHIJKLMNOPQRSTUV\nWXYZabcdefghijklmnopqrstuvwxyz.,\n" ..
        "ÃÂÁÀÄÊÉÈËÎÍÌÏÕÔÓÒÖŨÛÚÙÜãâáàäêéèë\n" ..
        "îíìïõôóòöũûúùüÑñÇç:\\|/<>()-+=*[]\n?!{}@#$%&ªº£¬_", 32, 8))
    assert(setFontMetrics("default", 14/64, 50/64, 14/64))
end
setCallBackToEvent("applyTheme", applyDefaultFont)
