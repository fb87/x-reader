local shell = require("shell")
local widget = shell.widget
local state = shell.state
local i18n = shell.i18n
local model = require("model")
local M = { id = model.page.reader }

function M.render()
    shell.api.clear(widget.gray.white)
    local chrome = state.get("app.reader.chrome") == true
    if chrome then widget.status(state.get("reader.book.title") or i18n.t("reading")) end

    local top = chrome and 60 or 28
    local bottom = chrome and 88 or 36
    local footer_y = shell.api.height() - bottom - 20
    local scale = math.min(4, 2 + (state.get("reader.settings.font_size") or 1))
    local y = top
    for i = 0, shell.reader.line_count() - 1 do
        shell.api.text(28, y, shell.reader.line(i), scale, widget.gray.black)
        y = y + 7 * scale + 4
    end

    local progress = shell.reader.progress()
    if state.get("reader.settings.show_progress") ~= false then
        shell.api.hline(28, footer_y, shell.api.width() - 56, widget.gray.light)
        shell.api.fill(28, footer_y - 1,
                       (shell.api.width() - 56) * progress / 100, 3, widget.gray.dark)
    end
    shell.api.text(28, footer_y + 16, shell.reader.chapter(), 1, widget.gray.dark)
    local pages = string.format("%d / %d", shell.reader.page() + 1, shell.reader.page_count())
    shell.api.text(shell.api.width() - 28 - string.len(pages) * 6, footer_y + 16,
                   pages, 1, widget.gray.dark)
    if chrome then widget.dock { i18n.t("close"), "A-", "A+" } end
end

local function dock_action()
    local selected = state.get("app.dock.selected") or 0
    if selected == 0 then model.back()
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
    if (state.get("app.focus.area") or 0) == 1 then
        local selected = state.get("app.dock.selected") or 0
        if ev.key == "up" then
            if selected > 0 then state.set("app.dock.selected", selected - 1)
            else state.set("app.focus.area", 0) end
            return true
        end
        if ev.key == "left" and selected > 0 then state.set("app.dock.selected", selected - 1); return true end
        if ev.key == "right" and selected < 2 then state.set("app.dock.selected", selected + 1); return true end
        if ev.key == "down" then
            if selected < 2 then state.set("app.dock.selected", selected + 1) end
            return true
        end
        if ev.key == "ok" then dock_action(); return true end
    end
    if ev.key == "back" then
        if state.get("app.reader.chrome") then state.set("app.reader.chrome", false)
        else model.back() end
        return true
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
