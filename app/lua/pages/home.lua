local shell = require("shell")
local widget = shell.widget
local state = shell.state
local i18n = shell.i18n
local model = require("model")

local M = { id = model.page.home }
local function entries() return shell.router.menu("home") end

function M.render()
    shell.api.clear(widget.gray.white)
    widget.status(i18n.t("home"))
    local current = state.get("reader.book.current") or -1
    widget.book_card {
        x = 18, y = 62, w = shell.api.width() - 36, h = 180,
        eyebrow = i18n.t("continue_reading"),
        title = current >= 0 and (state.get("reader.book.title") or i18n.t("book"))
                             or i18n.t("no_book_open"),
        active = (state.get("app.menu.selected") or 0) == -1,
    }
    local selected = state.get("app.menu.selected") or 0
    for i, route in ipairs(entries()) do
        widget.row { y = 265 + (i - 1) * 76, primary = i18n.t(route.title), selected = selected == i - 1 }
    end
    widget.label { x = 24, y = shell.api.height() - 50,
                   text = i18n.t("books_count", shell.library.count()), gray = widget.gray.dark }
end

function M.event(ev)
    local selected = state.get("app.menu.selected") or 0
    local menu = entries()
    local count = #menu
    if ev.type == "tap" then
        if ev.y >= 265 and ev.y < 265 + count * 76 then
            selected = math.floor((ev.y - 265) / 76)
            state.set("app.menu.selected", selected)
            model.push(menu[selected + 1].path)
            return true
        end
        return false
    end
    if ev.type ~= "key" then return false end
    if ev.key == "up" then model.move("app.menu.selected", -1, count); return true end
    if ev.key == "down" then model.move("app.menu.selected", 1, count); return true end
    if ev.key == "ok" and menu[selected + 1] then model.push(menu[selected + 1].path); return true end
    return false
end

return M
