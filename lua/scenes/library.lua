local bookshelf = require("bookshelf")
local icons = require("icons")
local scene = { id = "library", chrome = "standard" }
function scene.create()
    return { selected = 1, dock_selected = 1, scroll = 0, focus_scope = "content" }
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
    if event.y >= 880 then
        stage.focus_scope = "dock"
        stage.dock_selected = math.floor(event.x / 180) + 1
        if stage.dock_selected == 1 then
            context.push("reading", { book = bookshelf.books[stage.selected] })
        end
        if stage.dock_selected == 3 then
            context.pop()
        end
    elseif event.y >= 120 and event.y < 820 then
        stage.focus_scope = "content"
        stage.selected =
            math.max(1, math.min(#bookshelf.books, math.floor((event.y - 120) / 130) + 1))
    end
end
function scene.draw(stage)
    for index, book in ipairs(bookshelf.books) do
        local y = 90 + (index - 1) * 130
        local highlighted = stage.focus_scope == "content" and index == stage.selected
        canvas.rectangle(20, y, 500, 106, highlighted and 3 or 14)
        canvas.text(48, y + 32, icons.book, 4, highlighted and 15 or 0)
        canvas.text(112, y + 38, book.title, 2, highlighted and 15 or 0)
        canvas.text(112, y + 68, book.author, 1, highlighted and 15 or 0)
    end
end
return scene
