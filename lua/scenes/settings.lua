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
            "Orientation",
            function()
                return settings.orientation == "landscape" and "Landscape" or "Portrait"
            end,
        },
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
        stage.list.page = math.floor((stage.selected - 1) / (stage.list.page_size or 1)) + 1
    else
        local rows = page_rows[stage.page] or {}
        if #rows > 0 then
            stage.control_selected = ((stage.control_selected - 1 + direction) % #rows) + 1
            local page_size = stage.row_page_size or 1
            stage.row_page = math.floor((stage.control_selected - 1) / page_size) + 1
        end
    end
end
function scene.activate_content(stage, context)
    if stage.page == "root" then
        stage.page = categories[stage.selected]
        stage.control_selected = 1
        stage.row_page = 1
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
            settings.toggle_orientation()
            if context and context.set_orientation then
                context.set_orientation(settings.orientation == "landscape")
            end
        elseif stage.control_selected == 2 then
            settings.toggle_brightness()
        elseif stage.control_selected == 3 then
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
    -- Dock touches are handled centrally by app.lua for standard-chrome
    -- scenes; only the row list below is ours to hit-test.
    if stage.page ~= "root" and stage.row_boxes ~= nil then
        local page_size = stage.row_page_size or 1
        for local_index, row_box in ipairs(stage.row_boxes) do
            if layout.contains(row_box, event.x, event.y) then
                stage.focus_scope = "content"
                stage.control_selected = (stage.row_page - 1) * page_size + local_index
                scene.activate_content(stage, context)
                return
            end
        end
    elseif stage.page == "root" and layout.contains(stage.list.bounds, event.x, event.y) then
        local page_size = stage.list.page_size or 1
        local local_index = math.floor((event.y - stage.list.bounds.y) / layout.scale_y(84))
        stage.focus_scope = "content"
        stage.selected =
            math.max(1, math.min(#categories, (stage.list.page - 1) * page_size + local_index + 1))
        stage.list.selected_id = "settings:" .. categories[stage.selected]
        stage.page = categories[stage.selected]
        stage.control_selected = 1
        stage.row_page = 1
    end
end
function scene.draw(stage, bounds)
    local regions = layout.column(bounds, { padding = 24, gap = 8 }, {
        { size = 60 },
        { fill = true },
        { size = 28 },
    })
    local header, content, footer = regions[1], regions[2], regions[3]
    if stage.page == "root" then
        stage.list.selected_id = "settings:" .. categories[stage.selected]
        stage.list.active = stage.focus_scope == "content"
        canvas.text(header.x, layout.text_y(header, "Settings", 3), "Settings", 3, 0)
        stage.list:layout(content)
        stage.list:draw()
        stage.row_boxes = nil
    else
        canvas.text(
            header.x,
            layout.text_y(header, stage.page .. " Settings", 2),
            stage.page .. " Settings",
            2,
            0
        )
        local rows = page_rows[stage.page] or {}
        local page_size = math.max(1, math.floor(content.height / layout.scale_y(84)))
        stage.row_page_size = page_size
        local page_count = math.max(1, math.ceil(#rows / page_size))
        stage.row_page = math.max(1, math.min(page_count, stage.row_page or 1))
        local first = (stage.row_page - 1) * page_size + 1
        local last = math.min(#rows, first + page_size - 1)
        local row_items = {}
        for _ = first, last do
            row_items[#row_items + 1] = { size = 84 }
        end
        stage.row_boxes = #row_items > 0 and layout.stack(content, "column", {}, row_items) or {}
        for local_index = 1, #stage.row_boxes do
            local index = first + local_index - 1
            local row = rows[index]
            local row_bounds = stage.row_boxes[local_index]
            local selected = stage.focus_scope == "content" and stage.control_selected == index
            if selected then
                canvas.rectangle(
                    row_bounds.x,
                    row_bounds.y - 1,
                    row_bounds.width,
                    row_bounds.height + 1,
                    theme.color.selected
                )
            end
            local text_color = selected and theme.color.selected_text or theme.color.text
            canvas.text(
                row_bounds.x + layout.scale_x(16),
                layout.text_y(row_bounds, row[1], 1),
                row[1],
                1,
                text_color
            )
            local value = row[2]()
            canvas.text(
                row_bounds.x + row_bounds.width - layout.scale_x(16) - canvas.measure(value, 1),
                layout.text_y(row_bounds, value, 1),
                value,
                1,
                text_color
            )
            if local_index < #stage.row_boxes and not selected then
                canvas.rectangle(
                    row_bounds.x + layout.scale_x(12),
                    row_bounds.y + row_bounds.height - 1,
                    row_bounds.width - layout.scale_x(24),
                    1,
                    theme.color.divider
                )
            end
        end
        if stage.notice == nil and page_count > 1 then
            local page_label = "Page " .. tostring(stage.row_page) .. " / " .. tostring(page_count)
            local label_width = canvas.measure(page_label, 1)
            canvas.text(
                footer.x + math.floor((footer.width - label_width) / 2),
                layout.text_y(footer, page_label, 1),
                page_label,
                1,
                5
            )
        end
    end
    if stage.notice ~= nil then
        canvas.text(footer.x, layout.text_y(footer, stage.notice, 1), stage.notice, 1, 5)
    end
end
return scene
