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
            icon = icons.arrow,
            invoke = function()
                context.pop()
            end,
        },
        {
            label = "A-",
            icon = icons.circle,
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
    if stage.dock_visible then
        if event.y >= layout.standard().dock.y then
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
    local side_margin = 8
    local text_margin = 8
    local panel_width = 540 - side_margin * 2
    local text_x = side_margin + text_margin
    local text_width = panel_width - text_margin * 2
    local text_right = text_x + text_width
    local top = 48
    local bottom = stage.dock_visible and 880 or 947
    local footer_height = 48
    local content_bottom = bottom - footer_height
    local header_height = 24
    local _, font_height = canvas.measure("Ag", scale)
    local line_height = font_height + 2
    canvas.rectangle(side_margin, top, panel_width, content_bottom - top, theme.color.surface)
    if reader.error ~= nil then
        canvas.text(text_x, top + 90, "EPUB error", 2, 0)
        canvas.text(text_x, top + 130, reader.error, 1, 0)
    else
        local line_limit = math.floor((content_bottom - top - header_height) / line_height)
        for index, line in ipairs(reader.page_lines(scale, line_limit, text_width)) do
            canvas.text(text_x, top + header_height + (index - 1) * line_height, line, scale, 0)
        end
    end
    local footer_top = bottom - footer_height
    local progress_width = reader.progress_width(text_width)
    local page_label = string.format("%d / %d", reader.page, reader.page_count)
    local chapter = truncate_text(reader.chapter_name(), 1, text_width - 100)
    local page_label_x = text_right - canvas.measure(page_label, 1)
    canvas.rectangle(side_margin, footer_top, panel_width, footer_height, theme.color.surface)
    canvas.text(text_x, footer_top + 5, chapter, 1, 0)
    canvas.text(page_label_x, footer_top + 5, page_label, 1, 0)
    canvas.rectangle(text_x, footer_top + 31, text_width, 6, theme.color.divider)
    canvas.rectangle(text_x, footer_top + 31, progress_width, 6, theme.color.secondary)
end
return scene
