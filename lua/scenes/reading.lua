local reader = require("reader")
local settings = require("settings")
local icons = require("icons")
local layout = require("layout")
local theme = require("theme")
local ActionBar = require("ui.action_bar")
local scene = { id = "reading", chrome = "fullscreen", status = true, status_title = true }

function scene.title(stage)
    return stage.book and stage.book.title or "Reader"
end

local function truncate_text(text, size, width)
    local suffix = "..."
    if canvas.measure(text, size) <= width then
        return text
    end
    local length = #text
    while length > 0 and canvas.measure(text:sub(1, length) .. suffix, size) > width do
        length = length - 1
        while length > 0 do
            local byte = text:byte(length)
            if byte < 128 or byte >= 192 then
                break
            end
            length = length - 1
        end
    end
    return text:sub(1, length) .. suffix
end

function scene.create(context)
    local actions = {
        {
            label = "Close",
            icon = icons.close,
            invoke = function()
                context.pop()
            end,
        },
        {
            label = "A-",
            icon = icons.remove,
            invoke = function()
                settings.decrease_font_size()
            end,
        },
        {
            label = "A+",
            icon = icons.add,
            invoke = function()
                settings.increase_font_size()
            end,
        },
    }
    return {
        book = nil,
        dock_visible = false,
        focus_scope = "content",
        dock_selected = 1,
        action_bar = ActionBar({ actions = actions, selected = 1 }),
    }
end
function scene.show(stage, params)
    stage.book = params and params.book or stage.book
    if stage.book then
        reader.open(stage.book)
    end
end
function scene.dock(stage)
    return stage.action_bar.actions
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
    stage.action_bar.selected = stage.dock_selected
    stage.action_bar:activate()
    if stage.dock_selected == 1 then
        stage.dock_visible = false
        stage.focus_scope = "content"
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
    -- The dock overlay's own touches are handled centrally by app.lua
    -- (scene.activate_dock); this only covers the reading panel itself,
    -- which is hidden behind the dock while it's open.
    if stage.dock_visible or stage.panel_bounds == nil then
        return
    end
    local panel = stage.panel_bounds
    local third = math.floor(panel.width / 3)
    if event.x < panel.x + third then
        reader.previous_page()
    elseif event.x > panel.x + third * 2 then
        reader.next_page()
    elseif stage.footer_bounds and event.y >= stage.footer_bounds.y then
        stage.dock_visible = true
        stage.focus_scope = "dock"
    end
end
function scene.draw(stage, bounds)
    local scale = settings.font_scale()
    local side_margin = layout.scale_x(8)
    local text_margin = layout.scale_x(8)
    local regions = layout.column(bounds, { padding = 0 }, {
        { fill = true },
        { size = 48 },
    })
    local panel, footer = regions[1], regions[2]
    panel = {
        x = panel.x + side_margin,
        y = panel.y,
        width = panel.width - side_margin * 2,
        height = panel.height,
    }
    footer = {
        x = footer.x + side_margin,
        y = footer.y,
        width = footer.width - side_margin * 2,
        height = footer.height,
    }
    stage.panel_bounds = panel
    stage.footer_bounds = footer
    local text_x = panel.x + text_margin
    local text_width = panel.width - text_margin * 2
    local text_right = text_x + text_width
    local header_height = layout.scale_y(24)
    local _, font_height = canvas.measure("Ag", scale)
    local line_height = font_height + 2
    canvas.rectangle(panel.x, panel.y, panel.width, panel.height, theme.color.surface)
    if reader.error ~= nil then
        canvas.text(text_x, panel.y + layout.scale_y(90), "EPUB error", 2, 0)
        canvas.text(text_x, panel.y + layout.scale_y(130), reader.error, 1, 0)
    else
        local line_limit = math.floor((panel.height - header_height) / line_height)
        for index, line in ipairs(reader.page_lines(scale, line_limit, text_width)) do
            canvas.text(text_x, panel.y + header_height + (index - 1) * line_height, line, scale, 0)
        end
    end
    local progress_width = reader.progress_width(text_width)
    local page_label = string.format("%d / %d", reader.page, reader.page_count)
    local chapter = truncate_text(reader.chapter_name(), 1, text_width - layout.scale_x(100))
    local page_label_x = text_right - canvas.measure(page_label, 1)
    canvas.rectangle(footer.x, footer.y, footer.width, footer.height, theme.color.surface)
    canvas.text(text_x, footer.y + layout.scale_y(5), chapter, 1, 0)
    canvas.text(page_label_x, footer.y + layout.scale_y(5), page_label, 1, 0)
    canvas.rectangle(
        text_x,
        footer.y + layout.scale_y(31),
        text_width,
        layout.scale_y(6),
        theme.color.divider
    )
    canvas.rectangle(
        text_x,
        footer.y + layout.scale_y(31),
        progress_width,
        layout.scale_y(6),
        theme.color.secondary
    )
end
return scene
