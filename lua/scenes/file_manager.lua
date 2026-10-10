local icons = require("icons")
local scene = { id = "file_manager", chrome = "standard" }
function scene.create()
    return { selected = 1, dock_selected = 1, path = "/", focus_scope = "content" }
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
function scene.draw()
    canvas.text(24, 82, icons.folder, 4, 0)
    canvas.text(24, 90, "SD Card", 2, 0)
    canvas.text(24, 150, "No files indexed yet", 2, 5)
    canvas.text(24, 198, "The storage service will populate this view.", 1, 0)
end
return scene
