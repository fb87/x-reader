local shell = require("shell")
local widget = shell.widget
local state = shell.state
local i18n = shell.i18n
local model = require("model")
local M = { id = model.page.reader }

local body = {
    "IT IS A TRUTH UNIVERSALLY ACKNOWLEDGED,",
    "THAT A SINGLE MAN IN POSSESSION OF A GOOD",
    "FORTUNE, MUST BE IN WANT OF A WIFE.",
    "",
    "HOWEVER LITTLE KNOWN THE FEELINGS OR VIEWS",
    "OF SUCH A MAN MAY BE ON HIS FIRST ENTERING",
    "A NEIGHBOURHOOD, THIS TRUTH IS SO WELL FIXED",
    "IN THE MINDS OF THE SURROUNDING FAMILIES.",
}

function M.render()
    shell.api.clear(widget.gray.white)
    local chrome = state.get("app.reader.chrome") == true
    if chrome then widget.status(state.get("reader.book.title") or i18n.t("reading")) end
    local top = chrome and 60 or 28
    widget.label { x = 28, y = top + 10,
                   text = i18n.t("chapter_page", (state.get("reader.book.page") or 0) + 1),
                   gray = widget.gray.dark }
    local y = top + 58
    local scale = math.min(3, 2 + (state.get("reader.settings.font_size") or 1))
    for _, line in ipairs(body) do
        widget.label { x = 28, y = y, text = line, scale = scale }
        y = y + 38
    end
    if state.get("reader.settings.show_progress") ~= false then
        widget.progress { x = 28, y = shell.api.height() - (chrome and 88 or 36),
                          w = shell.api.width() - 56, h = 10,
                          value = state.get("reader.book.progress") or 0 }
    end
    if chrome then widget.dock { i18n.t("close"), "A-", "A+" } end
end

local function dock_action()
    local selected = state.get("app.dock.selected") or 0
    if selected == 0 then model.set_page(model.page.home)
    elseif selected == 1 then shell.reader.adjust_font(-1)
    else shell.reader.adjust_font(1) end
end

function M.event(ev)
    if ev.type == "tap" then
        local third = math.floor(shell.api.width() / 3)
        if ev.x < third then shell.reader.previous()
        elseif ev.x >= third * 2 then shell.reader.next()
        else state.set("app.reader.chrome", not (state.get("app.reader.chrome") == true)) end
        return true
    end
    if ev.type ~= "key" then return false end
    if ev.key == "back" then
        if state.get("app.reader.chrome") then state.set("app.reader.chrome", false)
        else model.set_page(model.page.home) end
        return true
    end
    if (state.get("app.focus.area") or 0) == 1 then
        local selected = state.get("app.dock.selected") or 0
        if ev.key == "up" then state.set("app.focus.area", 0); return true end
        if ev.key == "left" and selected > 0 then state.set("app.dock.selected", selected - 1); return true end
        if ev.key == "right" and selected < 2 then state.set("app.dock.selected", selected + 1); return true end
        if ev.key == "down" then return true end
        if ev.key == "ok" then dock_action(); return true end
    end
    if ev.key == "right" or ev.key == "down" then
        if ev.key == "down" and state.get("app.reader.chrome") then state.set("app.focus.area", 1)
        else shell.reader.next() end
        return true
    end
    if ev.key == "left" or ev.key == "up" then shell.reader.previous(); return true end
    if ev.key == "menu" or ev.key == "ok" then
        state.set("app.reader.chrome", not (state.get("app.reader.chrome") == true)); return true
    end
    return false
end
return M
