local scene = { id = "launcher", chrome = "standard" }
local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local items = { "Resume", "Library", "Files", "Settings" }
local item_icons = { icons.bookmark, icons.book, icons.folder, icons.circle }
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
        list = List({ rows = rows, selected_id = "resume", row_height = 84 }),
    }
end
function scene.dock()
    return { { label = "Sleep", icon = icons.circle }, { label = "Power", icon = icons.bookmark } }
end
function scene.move(stage, direction)
    stage.selected = ((stage.selected - 1 + direction) % #items) + 1
    stage.list.selected_id = items[stage.selected]:lower()
    stage.list.active = stage.focus_scope == "content"
end
function scene.activate_content(stage, context)
    if stage.selected == 2 then
        context.push("library")
    end
    if stage.selected == 3 then
        context.push("file_manager")
    end
    if stage.selected == 4 then
        context.push("settings")
    end
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 then
        context.push("sleep")
    end
end
function scene.event(stage, event, context)
    if event.kind == "touch" and event.phase == "up" then
        local index = math.floor((event.y - 113) / 84) + 1
        if index >= 1 and index <= 4 then
            stage.focus_scope = "content"
            stage.selected = index
            if index == 2 then
                context.push("library")
            end
            if index == 3 then
                context.push("file_manager")
            end
            if index == 4 then
                context.push("settings")
            end
        end
    end
end
function scene.draw(stage)
    stage.list.selected_id = items[stage.selected]:lower()
    canvas.text(24, 60, "Home", 3, 0)
    stage.list:layout({ x = 24, y = 113, width = 492, height = layout.content_height(113, 40) })
    stage.list:draw()
end
return scene
