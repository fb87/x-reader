local bookshelf = require("bookshelf")
local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local Dialog = require("ui.dialog")
local scene = { id = "library", chrome = "standard" }
function scene.title()
    return "Library"
end
function scene.create()
    local rows = {}
    for index, book in ipairs(bookshelf.books) do
        rows[index] = {
            id = "book:" .. tostring(index),
            title = book.title,
            subtitle = book.author,
            icon = icons.book,
        }
    end
    return {
        selected = 1,
        dock_selected = 1,
        scroll = 0,
        focus_scope = "content",
        list = List({ rows = rows, selected_id = "book:1", row_height = 84 }),
        dialog = Dialog({ title = "Book Info" }),
    }
end

local function book_size(bytes)
    if bytes == nil or bytes == 0 then
        return "Unknown size"
    elseif bytes >= 1024 * 1024 then
        return string.format("%.1f MB", bytes / (1024 * 1024))
    elseif bytes >= 1024 then
        return string.format("%.1f KB", bytes / 1024)
    end
    return tostring(bytes) .. " B"
end

function scene.show_info(stage, context)
    local book = bookshelf.books[stage.selected]
    if book == nil then
        return
    end
    stage.dialog:show(
        "Book Info",
        table.concat({
            book.title or "Unknown title",
            book.author ~= "" and book.author or "Unknown author",
            "EPUB  " .. book_size(book.size),
            book.path or "Unknown path",
        }, "\n"),
        {
            {
                label = "Open",
                icon = icons.book,
                invoke = function(dialog)
                    dialog:hide()
                    stage.focus_scope = "content"
                    context.push("reading", { book = book })
                end,
            },
            {
                label = "Close",
                icon = icons.close,
                invoke = function(dialog)
                    dialog:hide()
                    stage.focus_scope = "content"
                end,
            },
        },
        icons.book
    )
    stage.focus_scope = "dialog"
end
function scene.dock()
    return {
        { label = "Open", icon = icons.book },
        { label = "Info", icon = icons.info },
        { label = "Back", icon = icons.arrow_back },
    }
end
function scene.move(stage, direction)
    if #bookshelf.books == 0 then
        return
    end
    stage.selected = ((stage.selected - 1 + direction) % #bookshelf.books) + 1
    stage.list.selected_id = "book:" .. tostring(stage.selected)
    stage.list.page = math.floor(
        (stage.selected - 1)
            / math.max(1, math.floor(stage.list.bounds.height / stage.list.row_height))
    ) + 1
    stage.list.active = stage.focus_scope == "content"
end
function scene.activate_content(stage, context)
    if #bookshelf.books > 0 then
        context.push("reading", { book = bookshelf.books[stage.selected] })
    end
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 then
        if #bookshelf.books > 0 then
            context.push("reading", { book = bookshelf.books[stage.selected] })
        end
    elseif stage.dock_selected == 2 then
        scene.show_info(stage, context)
    elseif stage.dock_selected == 3 then
        context.pop()
    end
end
function scene.event(stage, event, context)
    if event.kind ~= "touch" or event.phase ~= "up" then
        return
    end
    if stage.dialog.visible then
        local bounds = stage.dialog.bounds
        if
            event.x >= bounds.x
            and event.x < bounds.x + bounds.width
            and event.y >= bounds.y + bounds.height - 64
        then
            stage.dialog.selected = math.min(
                #stage.dialog.actions,
                math.floor((event.x - bounds.x) / (bounds.width / #stage.dialog.actions)) + 1
            )
            stage.dialog:activate()
        elseif event.x < bounds.x or event.x >= bounds.x + bounds.width or event.y < bounds.y then
            stage.dialog:hide()
            stage.focus_scope = "content"
        end
        return
    end
    if event.y >= layout.standard().dock.y then
        stage.focus_scope = "dock"
        stage.dock_selected = math.floor(event.x / 180) + 1
        if stage.dock_selected == 1 then
            context.push("reading", { book = bookshelf.books[stage.selected] })
        elseif stage.dock_selected == 2 then
            scene.show_info(stage, context)
        end
        if stage.dock_selected == 3 then
            context.pop()
        end
    elseif event.y >= 820 and event.x >= 360 then
        context.pop()
    elseif event.y >= 700 then
        local page_size = stage.list.page_size or 1
        if event.x < 270 then
            stage.selected = math.max(1, stage.selected - page_size)
        else
            stage.selected = math.min(#bookshelf.books, stage.selected + page_size)
        end
        stage.list.page = math.floor((stage.selected - 1) / page_size) + 1
    elseif event.y >= 119 and event.y < 119 + #bookshelf.books * stage.list.row_height then
        stage.focus_scope = "content"
        stage.selected = math.max(
            1,
            math.min(#bookshelf.books, math.floor((event.y - 119) / stage.list.row_height) + 1)
        )
        stage.list.page = math.floor((stage.selected - 1) / (stage.list.page_size or 1)) + 1
    end
end
function scene.draw(stage)
    stage.list.selected_id = "book:" .. tostring(stage.selected)
    canvas.text(24, 60, "Library", 3, 0)
    stage.list:layout({ x = 20, y = 119, width = 500, height = layout.content_height(119, 24) })
    stage.list.page = math.floor(
        (stage.selected - 1)
            / math.max(1, math.floor(stage.list.bounds.height / stage.list.row_height))
    ) + 1
    stage.list:draw()
    canvas.text(
        210,
        layout.standard().dock.y - 28,
        "Page " .. tostring(stage.list.page) .. " / " .. tostring(stage.list.page_count),
        1,
        5
    )
end
return scene
