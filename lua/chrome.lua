local layout = require("layout")
local icons = require("icons")
local settings = require("settings")
local theme = require("theme")
local text = require("text")
local chrome = {}
-- The simulator watcher reloads this module after every Lua edit.

function chrome.status(area, title, show_title)
    canvas.rectangle(area.x, area.y, area.width, area.height, theme.color.background)
    canvas.rectangle(area.x, area.y + area.height - 1, area.width, 1, theme.color.divider)
    if settings.show_clock then
        canvas.text(layout.scale_x(18), layout.text_y(area, "10:27", 1), "10:27", 1, 0)
    end
    if show_title and title ~= nil and title ~= "" then
        local title_x = layout.scale_x(96)
        local title_right = area.x + area.width - layout.scale_x(126)
        title = text.truncate(title, 1, title_right - title_x - 4)
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

local function fill_slots(count)
    local items = {}
    for _ = 1, math.max(1, count) do
        items[#items + 1] = { fill = true }
    end
    return items
end

-- A slot wider than it is tall (bottom dock, portrait) lays its icon and
-- label side by side; a slot taller than it is wide (side rail, landscape)
-- stacks them instead. Either way the slot decides for itself from its own
-- shape, so chrome.dock() never needs to know the display's orientation.
local function draw_slot_contents(slot, action)
    if slot.width >= slot.height then
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
        return
    end
    -- The icon stays upright near the top of the rail slot; the label
    -- rotates 90 degrees to read down the slot instead of being squeezed
    -- sideways into a column far narrower than the text itself.
    local label_top = slot.y + layout.scale_y(8)
    if action.icon ~= nil then
        local icon_width, icon_height = canvas.measure(action.icon, 6)
        canvas.text(
            slot.x + math.floor((slot.width - icon_width) / 2),
            label_top,
            action.icon,
            6,
            theme.color.text
        )
        label_top = label_top + icon_height + layout.scale_y(8)
    end
    local label_length = canvas.measure(action.label, 1)
    local available = slot.y + slot.height - label_top
    local label_y = label_top + math.floor((available - label_length) / 2)
    canvas.text(
        slot.x + math.floor(slot.width / 2),
        label_y,
        action.label,
        1,
        theme.color.text,
        true
    )
end

function chrome.dock(area, actions, selected, active)
    -- Keep the active dock action unmistakable on grayscale displays.
    canvas.rectangle(area.x, area.y, area.width, area.height, theme.color.background)
    local horizontal = area.width >= area.height
    -- The divider belongs on whichever edge of the dock touches content,
    -- not a fixed side -- a bottom-anchored dock borders content above it,
    -- a left-anchored rail borders content to its right, and so on.
    if horizontal then
        if area.y == 0 then
            canvas.rectangle(area.x, area.y + area.height - 1, area.width, 1, theme.color.selected)
        else
            canvas.rectangle(area.x, area.y, area.width, 1, theme.color.selected)
        end
    else
        if area.x == 0 then
            canvas.rectangle(area.x + area.width - 1, area.y, 1, area.height, theme.color.selected)
        else
            canvas.rectangle(area.x, area.y, 1, area.height, theme.color.selected)
        end
    end
    local slots = layout.stack(area, horizontal and "row" or "column", {}, fill_slots(#actions))
    for index, action in ipairs(actions) do
        local slot = slots[index]
        if active and index == selected then
            canvas.rectangle(slot.x, slot.y, slot.width, slot.height, theme.color.selected)
        end
        draw_slot_contents(slot, action)
    end
end

return chrome
