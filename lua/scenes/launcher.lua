local scene = { id = "launcher", chrome = "standard" }
local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local reader = require("reader")
local items = { "Library", "Files", "Settings" }
local item_icons = { icons.library, icons.folder_open, icons.settings }
function scene.title()
    return "Home"
end
function scene.create()
    local rows = {}
    for index, label in ipairs(items) do
        rows[index] = { id = label:lower(), title = label, icon = item_icons[index] }
    end
    return {
        selected = 1,
        dock_selected = 1,
        focus_scope = "content",
        list = List({ rows = rows, selected_id = "library", row_height = 84 }),
    }
end
function scene.dock()
    return { { label = "Sleep", icon = icons.sleep }, { label = "Power", icon = icons.power } }
end
function scene.move(stage, direction)
    stage.selected = ((stage.selected + direction - 1) % (#items + 1)) + 1
    if stage.selected > 1 then
        stage.list.selected_id = items[stage.selected - 1]:lower()
    end
    stage.list.active = stage.focus_scope == "content"
end
function scene.activate_content(stage, context)
    if stage.selected == 1 then
        if reader.last_book ~= nil then
            context.push("reading", { book = reader.last_book })
        else
            context.push("library")
        end
    elseif stage.selected == 2 then
        context.push("library")
    elseif stage.selected == 3 then
        context.push("file_manager")
    elseif stage.selected == 4 then
        context.push("settings")
    end
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 or stage.dock_selected == 2 then
        context.push("sleep")
    end
end
function scene.event(stage, event, context)
    if event.kind == "touch" and event.phase == "up" then
        if event.y >= 96 and event.y < 300 then
            stage.selected = 1
            stage.focus_scope = "content"
            scene.activate_content(stage, context)
            return
        end
        local index = math.floor((event.y - 340) / 84) + 2
        if index >= 2 and index <= #items + 1 then
            stage.focus_scope = "content"
            stage.selected = index
            scene.activate_content(stage, context)
        end
    end
end
function scene.draw(stage)
    local book = reader.last_book
    local card_selected = stage.selected == 1 and stage.focus_scope == "content"
    canvas.text(24, 60, "Home", 3, 0)
    canvas.rectangle(24, 96, 492, 216, card_selected and 12 or 14)
    if book ~= nil then
        canvas.text(48, 124, "Continue Reading", 3, 0)
        canvas.text(48, 164, book.title or "Unknown title", 2, 0)
        canvas.text(48, 196, book.author ~= "" and book.author or "Unknown author", 1, 5)
        local page_label = string.format("%d / %d", reader.page, reader.page_count)
        canvas.text(496 - canvas.measure(page_label, 1), 258, page_label, 1, 5)
        canvas.rectangle(48, 278, 444, 6, 10)
        canvas.rectangle(48, 278, reader.progress_width(444), 6, 5)
    else
        canvas.text(48, 164, "No book to resume", 2, 5)
    end
    stage.list.selected_id = stage.selected > 1 and items[stage.selected - 1]:lower() or nil
    stage.list.active = stage.focus_scope == "content" and stage.selected > 1
    stage.list:layout({ x = 24, y = 340, width = 492, height = layout.content_height(340, 40) })
    stage.list:draw()
end
return scene
