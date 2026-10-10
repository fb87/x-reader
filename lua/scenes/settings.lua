local icons = require("icons")
-- watcher verification
local scene = { id = "settings", chrome = "standard" }
local categories = { "Reading", "Display", "Storage", "System" }
function scene.create()
    return { selected = 1, dock_selected = 1, page = "root", focus_scope = "content" }
end
function scene.dock()
    return {
        { label = "Back", icon = icons.arrow },
        { label = "Defaults", icon = icons.circle },
        { label = "Apply", icon = icons.bookmark },
    }
end
function scene.move(stage, direction)
    if stage.page == "root" then
        stage.selected = ((stage.selected - 1 + direction) % #categories) + 1
    end
end
function scene.activate_content(stage)
    if stage.page == "root" then
        stage.page = categories[stage.selected]
    end
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 then
        if stage.page == "root" then
            context.pop()
        else
            stage.page = "root"
        end
    end
end
function scene.event(stage, event, context)
    if event.kind ~= "touch" or event.phase ~= "up" then
        return
    end
    if event.y >= 880 then
        stage.focus_scope = "dock"
        stage.dock_selected = math.floor(event.x / 180) + 1
        if stage.dock_selected == 1 then
            if stage.page == "root" then
                context.pop()
            else
                stage.page = "root"
            end
        end
    elseif stage.page == "root" and event.y >= 90 and event.y < 650 then
        stage.focus_scope = "content"
        stage.selected = math.max(1, math.min(4, math.floor((event.y - 90) / 130) + 1))
        stage.page = categories[stage.selected]
    end
end
function scene.draw(stage)
    if stage.page == "root" then
        for index, category in ipairs(categories) do
            local y = 90 + (index - 1) * 130
            local highlighted = stage.focus_scope == "content" and index == stage.selected
            canvas.rectangle(24, y, 492, 96, highlighted and 3 or 14)
            canvas.text(52, y + 32, category, 2, highlighted and 15 or 0)
        end
    else
        canvas.text(24, 92, "Settings for " .. stage.page, 2, 0)
        canvas.text(24, 160, "Service-backed controls will appear here.", 1, 5)
    end
end
return scene
