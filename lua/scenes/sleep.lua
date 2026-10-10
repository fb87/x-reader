local scene = { id = "sleep", chrome = "fullscreen" }
function scene.create()
    return {}
end
function scene.dock()
    return {}
end
function scene.draw()
    canvas.clear(0)
    canvas.text(190, 420, "X-Reader", 2, 15)
    canvas.text(194, 500, "10:27", 2, 15)
    canvas.text(160, 580, "Hold Push to wake", 1, 15)
end
return scene
