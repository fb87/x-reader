local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local scene = { id = "file_manager", chrome = "standard" }
function scene.create()
    local rows = {
        {
            id = "empty",
            title = "No files indexed yet",
            subtitle = "The storage service will populate this view.",
            icon = icons.folder,
        },
    }
    return {
        selected = 1,
        dock_selected = 1,
        path = "/",
        focus_scope = "content",
        list = List({ rows = rows, selected_id = "empty", row_height = 100 }),
    }
end
function scene.dock()
    return {
        { label = "Open", icon = icons.book },
        { label = "Up", icon = icons.arrow },
        { label = "Back", icon = icons.arrow },
    }
end
function scene.move(stage, direction)
    stage.selected = math.max(1, stage.selected + direction)
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 3 then
        context.pop()
    end
end
function scene.event(stage, event, context)
    if event.kind == "touch" and event.phase == "up" and event.y >= 880 then
        stage.dock_selected = math.floor(event.x / 180) + 1
        if stage.dock_selected == 3 then
            context.pop()
        end
    end
end
function scene.draw(stage)
    local header = { x = 20, y = 70, width = 500, height = 64 }
    canvas.text(24, layout.text_y(header, icons.folder, 6), icons.folder, 6, 0)
    canvas.text(76, layout.text_y(header, "SD Card", 2), "SD Card", 2, 0)
    stage.list.active = stage.focus_scope == "content"
    stage.list:layout({ x = 20, y = 160, width = 500, height = layout.content_height(160, 40) })
    stage.list:draw()
end
return scene
