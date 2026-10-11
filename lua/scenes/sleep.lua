local layout = require("layout")
local scene = { id = "sleep", chrome = "fullscreen" }
function scene.create()
    return {}
end
function scene.event() end
function scene.dock()
    return {}
end
function scene.draw()
    canvas.clear(0)
    local box = layout.fullscreen()
    local lines = {
        { text = "X-Reader", size = 2 },
        { text = "10:27", size = 2 },
        { text = "Hold Push to wake", size = 1 },
    }
    local positions = layout.text_group_y(box, lines, layout.scale_y(16))
    for index, line in ipairs(lines) do
        local width = canvas.measure(line.text, line.size)
        canvas.text(
            box.x + math.floor((box.width - width) / 2),
            positions[index],
            line.text,
            line.size,
            15
        )
    end
end
return scene
