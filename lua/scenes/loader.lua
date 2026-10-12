local scene = { id = "loader", chrome = "fullscreen" }
local layout = require("layout")
local theme = require("theme")
function scene.create()
    return { started = 0 }
end
function scene.event() end
function scene.dock()
    return {}
end
function scene.update(stage, now, context)
    if stage.started == 0 then
        stage.started = now
    end
    if now - stage.started > 600 then
        context.replace("launcher")
        return true
    end
    return false
end
function scene.draw()
    canvas.clear(theme.color.background)
    local box_size = layout.scale_x(120)
    canvas.rectangle(
        math.floor((layout.width - box_size) / 2),
        layout.scale_y(220),
        box_size,
        layout.scale_y(120),
        2
    )
    layout.center_x(layout.scale_y(400), "X-Reader", 3, 0)
    layout.center_x(layout.scale_y(458), "EPUB reader for ESP32", 1, 4)
    layout.center_x(layout.scale_y(700), "Loading", 1, 0)
end
return scene
