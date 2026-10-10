local scene = { id = "launcher", chrome = "standard" }
local icons = require("icons")
local items = { "Resume", "Library", "Files", "Settings" }
local item_icons = { icons.bookmark, icons.book, icons.folder, icons.circle }
function scene.create()
    return { selected = 1, dock_selected = 1, focus_scope = "content" }
end
function scene.dock()
    return { { label = "Sleep", icon = icons.circle }, { label = "Power", icon = icons.bookmark } }
end
function scene.move(stage, direction)
    stage.selected = ((stage.selected - 1 + direction) % #items) + 1
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
        local index = math.floor((event.y - 90) / 150) + 1
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
    for index, label in ipairs(items) do
        local y = 90 + (index - 1) * 150
        local highlighted = stage.focus_scope == "content" and index == stage.selected
        canvas.rectangle(24, y, 492, 112, highlighted and 3 or 14)
        canvas.text(56, y + 32, item_icons[index], 4, highlighted and 15 or 0)
        canvas.text(112, y + 38, label, 2, highlighted and 15 or 0)
    end
end
return scene
