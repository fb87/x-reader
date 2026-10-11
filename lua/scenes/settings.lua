local icons = require("icons")
local layout = require("layout")
local settings = require("settings")
local persistence = require("persistence")
local List = require("ui.list")
local theme = require("theme")
-- watcher verification
local scene = { id = "settings", chrome = "standard" }
local categories = { "Reading", "Display", "Storage", "System", "About" }
local category_icons = { icons.book, icons.display, icons.storage, icons.tune, icons.info }
local page_rows = {
    Reading = {
        {
            "Font size",
            function()
                return settings.font_size
            end,
        },
        {
            "Partial refresh",
            function()
                return settings.partial_refresh and "On" or "Off"
            end,
        },
        {
            "Show page number",
            function()
                return settings.show_page_number and "On" or "Off"
            end,
        },
    },
    Display = {
        {
            "Brightness",
            function()
                return tostring(settings.brightness) .. "%"
            end,
        },
        {
            "Show clock",
            function()
                return settings.show_clock and "On" or "Off"
            end,
        },
    },
    Storage = {
        {
            "State record",
            function()
                return system.state_root() .. "/state.bin"
            end,
        },
        { "Library root", system.library_root },
    },
    System = {
        {
            "Runtime",
            function()
                return "Simulator"
            end,
        },
        {
            "State format",
            function()
                return "XRST v1"
            end,
        },
        {
            "Auto sleep",
            function()
                return tostring(settings.auto_sleep_minutes) .. "m"
            end,
        },
    },
    About = {
        {
            "Name",
            function()
                return "X-Reader"
            end,
        },
        {
            "Author",
            function()
                return "x-reader contributors"
            end,
        },
        {
            "Version",
            function()
                return "0.1.0"
            end,
        },
        {
            "Release date",
            function()
                return "2026-10-11"
            end,
        },
        {
            "GitHub",
            function()
                return "github:fb87/x-reader"
            end,
        },
    },
}
function scene.title(stage)
    return stage.page == "root" and "Settings"
        or stage.page == "About" and "About"
        or stage.page .. " Settings"
end
function scene.create()
    local rows = {}
    for index, category in ipairs(categories) do
        rows[index] =
            { id = "settings:" .. category, title = category, icon = category_icons[index] }
    end
    return {
        selected = 1,
        dock_selected = 1,
        page = "root",
        notice = nil,
        control_selected = 1,
        focus_scope = "content",
        list = List({ rows = rows, selected_id = "settings:Reading", row_height = 84 }),
    }
end
function scene.dock()
    return {
        { label = "Back", icon = icons.arrow_back },
        { label = "Defaults", icon = icons.restart },
        { label = "Apply", icon = icons.check },
    }
end
function scene.move(stage, direction)
    if stage.page == "root" then
        stage.selected = ((stage.selected - 1 + direction) % #categories) + 1
        stage.list.selected_id = "settings:" .. categories[stage.selected]
    else
        local rows = page_rows[stage.page] or {}
        if #rows > 0 then
            stage.control_selected = ((stage.control_selected - 1 + direction) % #rows) + 1
        end
    end
end
function scene.activate_content(stage)
    if stage.page == "root" then
        stage.page = categories[stage.selected]
        stage.control_selected = 1
    elseif stage.page == "Reading" then
        if stage.control_selected == 1 then
            settings.toggle_text_scale()
        elseif stage.control_selected == 2 then
            settings.toggle_refresh()
        elseif stage.control_selected == 3 then
            settings.toggle_page_number()
        end
        persistence.save()
        stage.notice = "Applied"
    elseif stage.page == "Display" then
        if stage.control_selected == 1 then
            settings.toggle_brightness()
        elseif stage.control_selected == 2 then
            settings.toggle_clock()
        end
        persistence.save()
        stage.notice = "Applied"
    elseif stage.page == "System" and stage.control_selected == 3 then
        settings.cycle_auto_sleep()
        persistence.save()
        stage.notice = "Applied"
    end
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 then
        if stage.page == "root" then
            context.pop()
        else
            stage.page = "root"
            stage.control_selected = 1
            stage.focus_scope = "content"
        end
    elseif stage.dock_selected == 2 then
        settings.reset()
        persistence.save()
        stage.notice = "Defaults restored"
    elseif stage.dock_selected == 3 then
        settings.apply()
        persistence.save()
        stage.notice = "Settings applied"
    end
end
function scene.event(stage, event, context)
    if event.kind ~= "touch" or event.phase ~= "up" then
        return
    end
    if event.y >= layout.standard().dock.y then
        stage.focus_scope = "dock"
        stage.dock_selected = math.floor(event.x / 180) + 1
        if stage.dock_selected == 1 then
            if stage.page == "root" then
                context.pop()
            else
                stage.page = "root"
                stage.control_selected = 1
                stage.focus_scope = "content"
            end
        elseif stage.dock_selected == 2 then
            settings.reset()
            persistence.save()
            stage.notice = "Defaults restored"
        elseif stage.dock_selected == 3 then
            settings.apply()
            persistence.save()
            stage.notice = "Settings applied"
        end
    elseif
        stage.page ~= "root"
        and event.y >= 140
        and event.y < 140 + #(page_rows[stage.page] or {}) * 84
    then
        stage.focus_scope = "content"
        stage.control_selected = math.floor((event.y - 140) / 84) + 1
        scene.activate_content(stage)
    elseif stage.page == "root" and event.y >= 113 and event.y < 113 + #categories * 84 then
        stage.focus_scope = "content"
        stage.selected = math.max(1, math.min(#categories, math.floor((event.y - 113) / 84) + 1))
        stage.list.selected_id = "settings:" .. categories[stage.selected]
        stage.page = categories[stage.selected]
        stage.control_selected = 1
    end
end
function scene.draw(stage)
    if stage.page == "root" then
        stage.list.selected_id = "settings:" .. categories[stage.selected]
        stage.list.active = stage.focus_scope == "content"
        canvas.text(24, 60, "Settings", 3, 0)
        stage.list:layout({ x = 24, y = 113, width = 492, height = layout.content_height(113, 40) })
        stage.list:draw()
    else
        canvas.text(24, 92, stage.page .. " Settings", 2, 0)
        for index, row in ipairs(page_rows[stage.page] or {}) do
            local y = 140 + (index - 1) * 84
            local bounds = { x = 24, y = y, width = 492, height = 84 }
            local selected = stage.focus_scope == "content" and stage.control_selected == index
            if selected then
                canvas.rectangle(
                    bounds.x,
                    bounds.y - 1,
                    bounds.width,
                    bounds.height + 1,
                    theme.color.selected
                )
            end
            local text_color = selected and theme.color.selected_text or theme.color.text
            canvas.text(40, layout.text_y(bounds, row[1], 1), row[1], 1, text_color)
            local value = row[2]()
            canvas.text(
                496 - canvas.measure(value, 1),
                layout.text_y(bounds, value, 1),
                value,
                1,
                text_color
            )
            if index < #page_rows[stage.page] and not selected then
                canvas.rectangle(
                    bounds.x + 12,
                    y + bounds.height - 1,
                    bounds.width - 24,
                    1,
                    theme.color.divider
                )
            end
        end
    end
    if stage.notice ~= nil then
        canvas.text(24, 850, stage.notice, 1, 5)
    end
end
return scene
