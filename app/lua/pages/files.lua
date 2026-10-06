local shell = require("shell")
local widget = shell.widget
local state = shell.state
local i18n = shell.i18n
local model = require("model")
local M = { id = model.page.files }

function M.render()
    shell.api.clear(widget.gray.white)
    widget.status(i18n.t("file_manager"))
    local selected = state.get("reader.library.selected") or 0
    for i = 0, math.min(shell.library.count() - 1, 9) do
        local book = shell.library.book(i)
        widget.row { y = 58 + i * 76, primary = book.title, secondary = i18n.t("epub"),
                     selected = selected == i and (state.get("app.focus.area") or 0) == 0 }
    end
    widget.dock { i18n.t("up_back") }
end

function M.event(ev)
    local count = shell.library.count()
    if ev.type == "tap" and ev.y >= 58 and ev.y < 58 + count * 76 then
        state.set("reader.library.selected", math.floor((ev.y - 58) / 76))
        if shell.reader.open_selected() then model.set_page(model.page.reader) end
        return true
    end
    if ev.type ~= "key" then return false end
    if ev.key == "back" then model.set_page(model.page.home); return true end
    if ev.key == "down" then model.move("reader.library.selected", 1, count); return true end
    if ev.key == "up" then model.move("reader.library.selected", -1, count); return true end
    if ev.key == "ok" and shell.reader.open_selected() then model.set_page(model.page.reader); return true end
    return false
end
return M
