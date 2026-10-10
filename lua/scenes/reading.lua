local reader = require("reader")
local settings = require("settings")
local icons = require("icons")
local scene = { id = "reading", chrome = "fullscreen" }
function scene.create()
    return { book = nil, dock_visible = false, focus_scope = "content", dock_selected = 1 }
end
function scene.show(stage, params)
    stage.book = params and params.book or stage.book
    if stage.book then
        reader.open(stage.book)
    end
end
function scene.dock()
    return {
        { label = "Close", icon = icons.arrow },
        { label = "A-", icon = icons.circle },
        { label = "A+", icon = icons.add },
    }
end
function scene.move(_, direction)
    if direction < 0 then
        reader.previous_page()
    else
        reader.next_page()
    end
end
function scene.activate_content(stage)
    stage.dock_visible = not stage.dock_visible
    stage.focus_scope = stage.dock_visible and "dock" or "content"
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 then
        stage.dock_visible = false
        stage.focus_scope = "content"
        context.pop()
        return
    elseif stage.dock_selected == 2 then
        settings.decrease_font_size()
    elseif stage.dock_selected == 3 then
        settings.increase_font_size()
    end
end
function scene.event(stage, event, context)
    if event.kind ~= "touch" or event.phase ~= "up" then
        return
    end
    if stage.dock_visible then
        if event.y >= 880 then
            if event.x < 180 then
                stage.dock_visible = false
                stage.focus_scope = "content"
                context.pop()
            elseif event.x < 360 then
                reader.previous_page()
            else
                reader.next_page()
            end
        end
    elseif event.x < 180 then
        reader.previous_page()
    elseif event.x > 360 then
        reader.next_page()
    elseif event.y >= 680 then
        stage.dock_visible = true
        stage.focus_scope = "dock"
    end
end
function scene.draw(stage)
    local scale = settings.font_scale()
    local top = stage.dock_visible and 76 or 20
    local bottom = stage.dock_visible and 850 or 920
    canvas.rectangle(20, top, 500, bottom - top, 15)
    canvas.text(42, top + 28, stage.book and stage.book.title or "Reading", 2, 0)
    local line_limit = math.floor((bottom - top - 90) / (30 * scale))
    for index, line in ipairs(reader.page_lines(scale, line_limit)) do
        canvas.text(44, top + 90 + (index - 1) * 30 * scale, line, scale, 0)
    end
end
return scene
