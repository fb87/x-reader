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
    local card_w = shell.api.width() - 32
    shell.api.border(16, 68, card_w, 190,
                     state.get("app.home.card.focused") == true and 4 or 2,
                     widget.gray.black)
    shell.api.text(36, 88, i18n.t("continue_reading"), 1, widget.gray.dark)
    local book = current >= 0 and shell.library.book(current) or nil
    if book then
        shell.api.fill(36, 120, 82, 112, widget.gray.light)
        shell.api.border(36, 120, 82, 112, 2, widget.gray.black)
        shell.api.fill(41, 125, 72, 16, widget.gray.black)
        shell.api.text(40, 148, string.sub(book.title or "", 1, 1), 2, widget.gray.black)
        local text_x = 132
        local text_w = card_w - 134 + 18 - 18
        shell.api.text(text_x, 122, book.title or "", 3, widget.gray.black)
        shell.api.text(text_x, 168, book.author or "", 2, widget.gray.dark)
        local percent = tostring(book.progress or 0) .. "%"
        local progress_w = text_w - 30
        shell.api.border(text_x, 208, progress_w, 10, 1, widget.gray.light)
        shell.api.fill(text_x + 1, 209, (progress_w - 2) * (book.progress or 0) / 100, 8,
                       widget.gray.dark)
        shell.api.text(text_x + progress_w + 10, 204, percent, 2, widget.gray.black)
    else
        shell.api.text(36, 127, i18n.t("no_book_open"), 2, widget.gray.black)
    end
    local selected = state.get("app.menu.selected") or 0
    local icons = { library = widget.icon.book, favorites = widget.icon.book,
                    file_manager = widget.icon.folder, settings = widget.icon.settings,
                    sleep = widget.icon.close }
    local rendered = 0
    for i, route in ipairs(entries()) do
        if route.title == "sleep" then goto continue end
        rendered = rendered + 1
        widget.row { x = 16, y = 290 + (rendered - 1) * 72,
                     w = shell.api.width() - 32, h = 63, primary = i18n.t(route.title),
                     selected = state.get("app.home.card.focused") ~= true and selected == i - 1,
                     icon = icons[route.title] or 0 }
        shell.api.border(16, 290 + (rendered - 1) * 72, shell.api.width() - 32, 63, 2,
                         widget.gray.black)
        ::continue::
    end
    shell.api.border(412, shell.api.height() - 58, 112, 34, 2, widget.gray.black)
    shell.api.icon(422, shell.api.height() - 51, widget.icon.close, widget.gray.black)
    shell.api.text(454, shell.api.height() - 49, "Sleep", 1, widget.gray.black)
    local in_progress = 0
    for i = 0, shell.library.count() - 1 do
        local book = shell.library.book(i)
        if book and (book.progress or 0) > 0 and (book.progress or 0) < 100 then
            in_progress = in_progress + 1
        end
    end
    widget.center { x = 0, y = shell.api.height() - 58, w = shell.api.width(), h = 34,
                    text = string.format("%d books  |  %d in progress", shell.library.count(), in_progress),
                    scale = 1, gray = widget.gray.dark }
end

function M.event(ev)
    local current = state.get("reader.book.current") or -1
    local selected = state.get("app.menu.selected") or 0
    local menu = entries()
    local count = #menu
    if ev.type == "tap" then
        if ev.y >= shell.api.height() - 58 and ev.x >= 400 then
            return model.push("/sleep")
        end
        if ev.y >= 68 and ev.y < 258 and current >= 0 then
            state.set("reader.library.selected", current)
            if shell.reader.open_selected() then model.set_page(model.page.reader) end
            return true
        end
        if ev.y >= 290 and ev.y < 290 + count * 72 then
            selected = math.floor((ev.y - 290) / 72)
            state.set("app.menu.selected", selected)
            model.push(menu[selected + 1].path)
            return true
        end
        return false
    end
    if ev.type ~= "key" then return false end
    if ev.key == "up" then
        if selected == 0 then state.set("app.home.card.focused", true) else model.move("app.menu.selected", -1, count) end
        return true
    end
    if ev.key == "down" then
        if state.get("app.home.card.focused") == true then state.set("app.home.card.focused", false)
        else model.move("app.menu.selected", 1, count) end
        return true
    end
    if ev.key == "ok" and state.get("app.home.card.focused") == true and current >= 0 then
        state.set("reader.library.selected", current)
        if shell.reader.open_selected() then model.set_page(model.page.reader) end
        return true
    end
    if ev.key == "ok" and menu[selected + 1] then model.push(menu[selected + 1].path); return true end
    return false
end

return M
