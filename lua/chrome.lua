local layout = require("layout")
local icons = require("icons")
local settings = require("settings")
local theme = require("theme")
local chrome = {}
-- The simulator watcher reloads this module after every Lua edit.

chrome.content = layout.standard().content

local function truncate_title(title, width)
    local suffix = "..."
    if canvas.measure(title, 1) <= width then
        return title
    end
    local length = #title
    while length > 0 and canvas.measure(title:sub(1, length) .. suffix, 1) > width do
        length = length - 1
        while length > 0 do
            local byte = title:byte(length)
            if byte < 128 or byte >= 192 then
                break
            end
            length = length - 1
        end
    end
    return title:sub(1, length) .. suffix
end

function chrome.status(title, show_title)
    local area = layout.standard().status
    canvas.rectangle(area.x, area.y, area.width, area.height, theme.color.background)
    canvas.rectangle(area.x, area.y + area.height - 1, area.width, 1, theme.color.divider)
    if settings.show_clock then
        canvas.text(layout.scale_x(18), layout.text_y(area, "10:27", 1), "10:27", 1, 0)
    end
    if show_title and title ~= nil and title ~= "" then
        local title_x = layout.scale_x(96)
        local title_right = area.x + area.width - layout.scale_x(126)
        title = truncate_title(title, title_right - title_x - 4)
        local title_width = canvas.measure(title, 1)
        canvas.text(title_x, layout.text_y(area, title, 1), title, 1, 0)
    end
    canvas.text(
        area.x + area.width - layout.scale_x(110),
        layout.text_y(area, "95%", 1),
        "95%",
        1,
        0
    )
    canvas.text(
        area.x + area.width - layout.scale_x(78),
        layout.text_y(area, icons.battery, 4),
        icons.battery,
        4,
        0
    )
    canvas.text(
        area.x + area.width - layout.scale_x(52),
        layout.text_y(area, icons.wifi, 4),
        icons.wifi,
        4,
        0
    )
    canvas.text(
        area.x + area.width - layout.scale_x(26),
        layout.text_y(area, icons.bluetooth, 4),
        icons.bluetooth,
        4,
        0
    )
end

function chrome.dock(actions, selected, active)
    -- Keep the active dock action unmistakable on grayscale displays.
    local area = layout.standard().dock
    canvas.rectangle(area.x, area.y, area.width, area.height, theme.color.background)
    canvas.rectangle(area.x, area.y, area.width, 1, theme.color.selected)
    local width = math.floor(area.width / math.max(1, #actions))
    for index, action in ipairs(actions) do
        local slot =
            { x = area.x + (index - 1) * width, y = area.y, width = width, height = area.height }
        local highlighted = active and index == selected
        if highlighted then
            canvas.rectangle(slot.x, slot.y, slot.width, slot.height, theme.color.selected)
        end
        local icon_width = action.icon and canvas.measure(action.icon, 6) or 0
        local label_width = canvas.measure(action.label, 1)
        local gap = action.icon and layout.scale_x(8) or 0
        local group_width = icon_width + gap + label_width
        local group_x = slot.x + math.floor((slot.width - group_width) / 2)
        canvas.text(
            group_x + icon_width + gap,
            layout.text_y(slot, action.label, 1),
            action.label,
            1,
            theme.color.text
        )
        if action.icon ~= nil then
            canvas.text(
                group_x,
                layout.text_y(slot, action.icon, 6),
                action.icon,
                6,
                theme.color.text
            )
        end
    end
end

return chrome
