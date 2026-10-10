local icons = require("icons")
local List = require("ui.list")
-- watcher verification
local scene = { id = "settings", chrome = "standard" }
local categories = { "Reading", "Display", "Storage", "System" }
function scene.create()
    local rows = {}
    for index, category in ipairs(categories) do
        rows[index] = { id = "settings:" .. category, title = category }
    end
    return {
        selected = 1,
        dock_selected = 1,
        page = "root",
        focus_scope = "content",
        list = List({ rows = rows, selected_id = "settings:Reading", row_height = 130 }),
    }
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
        stage.list.selected_id = "settings:" .. categories[stage.selected]
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
        stage.list.selected_id = "settings:" .. categories[stage.selected]
        stage.page = categories[stage.selected]
    end
end
function scene.draw(stage)
    if stage.page == "root" then
        stage.list.selected_id = "settings:" .. categories[stage.selected]
        stage.list.active = stage.focus_scope == "content"
        stage.list:layout({ x = 24, y = 90, width = 492, height = layout.content_height(90, 40) })
        stage.list:draw()
    else
        canvas.text(24, 92, "Settings for " .. stage.page, 2, 0)
        canvas.text(24, 160, "Service-backed controls will appear here.", 1, 5)
    end
end
return scene
