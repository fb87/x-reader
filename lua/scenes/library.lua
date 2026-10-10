local bookshelf = require("bookshelf")
local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local scene = { id = "library", chrome = "standard" }
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
        list = List({ rows = rows, selected_id = "book:1", row_height = 75 }),
    }
end
function scene.dock()
    return {
        { label = "Open", icon = icons.book },
        { label = "Info", icon = icons.circle },
        { label = "Back", icon = icons.arrow },
    }
end
function scene.move(stage, direction)
    stage.selected = ((stage.selected - 1 + direction) % #bookshelf.books) + 1
    stage.list.selected_id = "book:" .. tostring(stage.selected)
    stage.list.page = math.floor(
        (stage.selected - 1)
            / math.max(1, math.floor(stage.list.bounds.height / stage.list.row_height))
    ) + 1
    stage.list.active = stage.focus_scope == "content"
end
function scene.activate_content(stage, context)
    context.push("reading", { book = bookshelf.books[stage.selected] })
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 then
        context.push("reading", { book = bookshelf.books[stage.selected] })
    elseif stage.dock_selected == 3 then
        context.pop()
    end
end
function scene.event(stage, event, context)
    if event.kind ~= "touch" or event.phase ~= "up" then
        return
    end
    if event.y >= layout.standard().dock.y then
        stage.focus_scope = "dock"
        stage.dock_selected = math.floor(event.x / 180) + 1
        if stage.dock_selected == 1 then
            context.push("reading", { book = bookshelf.books[stage.selected] })
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
    elseif event.y >= 90 and event.y < 90 + #bookshelf.books * 75 then
        stage.focus_scope = "content"
        stage.selected =
            math.max(1, math.min(#bookshelf.books, math.floor((event.y - 90) / 75) + 1))
        stage.list.page = math.floor((stage.selected - 1) / (stage.list.page_size or 1)) + 1
    end
end
function scene.draw(stage)
    stage.list.selected_id = "book:" .. tostring(stage.selected)
    stage.list.page = math.floor(
        (stage.selected - 1)
            / math.max(1, math.floor(stage.list.bounds.height / stage.list.row_height))
    ) + 1
    stage.list:layout({ x = 20, y = 90, width = 500, height = layout.content_height(90, 70) })
    stage.list:draw()
    canvas.text(
        210,
        810,
        "Page " .. tostring(stage.list.page) .. " / " .. tostring(stage.list.page_count),
        1,
        5
    )
end
return scene
