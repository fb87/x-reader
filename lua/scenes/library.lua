local bookshelf = require("bookshelf")
local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local Dialog = require("ui.dialog")
local scene = { id = "library", chrome = "standard" }
scene.persist_fields = {
    { key = "selected", kind = "number" },
    { key = "dock_selected", kind = "number" },
    { key = "focus_scope", kind = "string" },
}
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
            / math.max(
                1,
                math.floor(stage.list.bounds.height / layout.scale_y(stage.list.row_height))
            )
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
            and event.y >= bounds.y + bounds.height - layout.scale_y(64)
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
    -- Dock touches are handled centrally by app.lua for standard-chrome
    -- scenes; only the list and the prev/next footer zones are ours.
    if stage.footer_prev and layout.contains(stage.footer_prev, event.x, event.y) then
        local page_size = stage.list.page_size or 1
        stage.selected = math.max(1, stage.selected - page_size)
        stage.list.page = math.floor((stage.selected - 1) / page_size) + 1
    elseif stage.footer_next and layout.contains(stage.footer_next, event.x, event.y) then
        local page_size = stage.list.page_size or 1
        stage.selected = math.min(#bookshelf.books, stage.selected + page_size)
        stage.list.page = math.floor((stage.selected - 1) / page_size) + 1
    elseif layout.contains(stage.list.bounds, event.x, event.y) then
        stage.focus_scope = "content"
        stage.selected = math.max(
            1,
            math.min(
                #bookshelf.books,
                math.floor((event.y - stage.list.bounds.y) / layout.scale_y(stage.list.row_height))
                    + 1
            )
        )
        stage.list.page = math.floor((stage.selected - 1) / (stage.list.page_size or 1)) + 1
    end
end
function scene.draw(stage, bounds)
    stage.list.selected_id = "book:" .. tostring(stage.selected)
    local regions = layout.scene(bounds, { padding = 20 })
    local header, list_area, footer = regions.header, regions.content, regions.footer
    canvas.text(header.x, layout.text_y(header, "Library", 3), "Library", 3, 0)
    stage.list:layout(list_area)
    stage.list:draw()
    local halves = layout.row(footer, {}, { { fill = true }, { fill = true } })
    stage.footer_prev, stage.footer_next = halves[1], halves[2]
    local page_label = "Page "
        .. tostring(stage.list.page)
        .. " / "
        .. tostring(stage.list.page_count)
    layout.center_text(footer, page_label, 1, 5)
end
return scene
