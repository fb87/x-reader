local shell = require("shell")
local widget = shell.widget
local i18n = shell.i18n
local model = require("model")

local M = { id = model.page.splash, entered = nil }

function M.render()
    shell.api.clear(widget.gray.white)
    shell.api.fill(202, 370, 64, 96, widget.gray.black)
    shell.api.fill(273, 370, 64, 96, widget.gray.black)
    for _, y in ipairs({387, 407, 429, 449}) do
        shell.api.hline(214, y, 40, widget.gray.white)
        shell.api.hline(285, y, 40, widget.gray.white)
    end
    shell.api.title(212, 500, "X-Reader", widget.gray.black)
    widget.center { x = 0, y = 544, w = shell.api.width(), h = 30,
                    text = "the ebook reader for e-ink", scale = 2, gray = widget.gray.dark }
    widget.center { x = 0, y = shell.api.height() - 70, w = shell.api.width(), h = 30,
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
