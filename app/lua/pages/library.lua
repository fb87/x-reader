local shell = require("shell")
local widget = shell.widget
local dialog = shell.dialog
local state = shell.state
local i18n = shell.i18n
local model = require("model")

local M = { id = model.page.library }

local function visible()
    local out = {}
    for i = 0, shell.library.count() - 1 do
        local book = shell.library.book(i)
        local favorites = model.current_page() == model.page.favorites
        if book and (not favorites or book.favorite) then out[#out + 1] = book end
    end
    return out
end

local function render_info(books)
    if not state.get("app.dialog.book_info") then return end
    local selected = state.get("reader.library.selected") or 0
    local book = books[selected + 1]
    if not book then return end
    dialog.render {
        title = i18n.t("book_info"), body = book.title,
        buttons = { book.progress > 0 and i18n.t("continue") or i18n.t("read"), i18n.t("close") },
        selected = state.get("app.dialog.selected") or 0,
    }
    shell.api.text(60, 378, book.author or "", 2, widget.gray.dark)
    shell.api.text(60, 414, string.format("EPUB  |  %d%%", book.progress or 0), 1, widget.gray.dark)
    shell.api.progress(60, 452, shell.api.width() - 120, 14, book.progress or 0)
end

function M.render()
    local books = visible()
    shell.api.clear(widget.gray.white)
    local favorites = model.current_page() == model.page.favorites
    widget.status(favorites and i18n.t("favorites") or i18n.t("library"))
    local selected = state.get("reader.library.selected") or 0
    for i, book in ipairs(books) do
        if i > 10 then break end
        widget.row {
            y = 58 + (i - 1) * 76,
            primary = book.title,
            secondary = (book.favorite and i18n.t("fav") or i18n.t("epub")) .. "  " .. tostring(book.progress) .. "%",
            selected = selected == i - 1 and (state.get("app.focus.area") or 0) == 0,
        }
    end
    if #books == 0 then
        widget.center { x = 0, y = 300, w = shell.api.width(), h = 80, text = i18n.t("no_books"),
                        scale = 3, gray = widget.gray.dark }
    end
    if (state.get("app.focus.area") or 0) == 1 then
        widget.dock { favorites and i18n.t("remove") or i18n.t("favorite"), i18n.t("delete"), i18n.t("back") }
    end
    render_info(books)
end

local function activate_dock()
    local selected = state.get("app.dock.selected") or 0
    if selected == 0 then shell.library.favorite()
    elseif selected == 1 then shell.library.delete()
    else model.back() end
end

function M.event(ev)
    local books = visible()
    if state.get("app.dialog.book_info") then
        if ev.type == "tap" then
            state.set("app.dialog.selected", ev.x < shell.api.width() / 2 and 0 or 1)
        elseif ev.type == "key" then
            if ev.key == "left" or ev.key == "up" then state.set("app.dialog.selected", 0)
            elseif ev.key == "right" or ev.key == "down" then state.set("app.dialog.selected", 1)
            elseif ev.key == "back" then state.set("app.dialog.book_info", false); return true
            elseif ev.key ~= "ok" then return true end
        else return true end
        if ev.type == "tap" or ev.key == "ok" then
            if (state.get("app.dialog.selected") or 0) == 0 and shell.reader.open_selected() then
                state.set("app.dialog.book_info", false)
                model.set_page(model.page.reader)
            else
                state.set("app.dialog.book_info", false)
            end
        end
        return true
    end

    if ev.type == "tap" and ev.y >= 58 and ev.y < 58 + #books * 76 then
        state.set("reader.library.selected", math.floor((ev.y - 58) / 76))
        state.set("app.dialog.book_info", true)
        state.set("app.dialog.selected", 0)
        return true
    end
    if ev.type ~= "key" then return false end
    if ev.key == "back" then model.back(); return true end

    if (state.get("app.focus.area") or 0) == 1 then
        local dock = state.get("app.dock.selected") or 0
        if ev.key == "up" then
            if dock > 0 then state.set("app.dock.selected", dock - 1)
            else state.set("app.focus.area", 0) end
            return true
        end
        if ev.key == "left" and dock > 0 then state.set("app.dock.selected", dock - 1); return true end
        if ev.key == "right" and dock < 2 then state.set("app.dock.selected", dock + 1); return true end
        if ev.key == "down" then
            if dock < 2 then state.set("app.dock.selected", dock + 1) end
            return true
        end
        if ev.key == "ok" then activate_dock(); return true end
    end

    local selected = state.get("reader.library.selected") or 0
    if ev.key == "down" then
        if selected + 1 < #books then state.set("reader.library.selected", selected + 1)
        else state.set("app.focus.area", 1) end
        return true
    end
    if ev.key == "up" then model.move("reader.library.selected", -1, #books); return true end
    if ev.key == "ok" and #books > 0 then
        state.set("app.dialog.book_info", true); state.set("app.dialog.selected", 0); return true
    end
    return false
end

return M
