local shell = require("shell")
local widget = shell.widget
local i18n = shell.i18n
local model = require("model")

local M = { id = model.page.splash, entered = nil }

function M.render()
    shell.api.clear(widget.gray.white)
    widget.center { x = 0, y = 320, w = shell.api.width(), h = 80,
                    text = i18n.t("reader_name"), scale = 5 }
    widget.center { x = 0, y = 410, w = shell.api.width(), h = 40,
                    text = i18n.t("reader_tagline"), scale = 2, gray = widget.gray.dark }
    widget.center { x = 0, y = shell.api.height() - 90, w = shell.api.width(), h = 30,
                    text = i18n.t("loading_library"), scale = 2, gray = widget.gray.dark }
end

function M.tick(now)
    if not M.entered then M.entered = now end
    if now - M.entered >= 1500 then model.set_page(model.page.home) end
end

function M.event(_)
    model.set_page(model.page.home)
    return true
end

return M
