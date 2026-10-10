local scene = { id = "loader", chrome = "fullscreen" }
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
    end
end
function scene.draw()
    canvas.clear(15)
    canvas.rectangle(210, 220, 120, 120, 2)
    canvas.text(126, 400, "X-Reader", 3, 0)
    canvas.text(158, 458, "EPUB reader for ESP32", 1, 4)
    canvas.text(218, 700, "Loading", 1, 0)
end
return scene
