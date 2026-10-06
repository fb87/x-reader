local shell = require("shell")
local widget = shell.widget
local i18n = shell.i18n
local model = require("model")
local M = { id = model.page.sleep }
function M.render()
    shell.api.clear(widget.gray.black)
    widget.center { x = 0, y = 400, w = shell.api.width(), h = 100,
                    text = i18n.t("sleeping"), scale = 4, gray = widget.gray.white }
end
function M.event(ev)
    if ev.type == "key" or ev.type == "tap" then model.set_page(model.page.home); return true end
    return false
end
return M
