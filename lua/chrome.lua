local layout = require("layout")
local chrome = {}
-- The simulator watcher reloads this module after every Lua edit.

chrome.content = layout.standard().content

function chrome.status(title)
    local area = layout.standard().status
    canvas.rectangle(area.x, area.y, area.width, area.height, 14)
    canvas.rectangle(area.x, area.y + area.height - 1, area.width, 1, 8)
    canvas.text(layout.scale_x(18), layout.scale_y(18), "10:27", 1, 0)
    canvas.text(layout.scale_x(210), layout.scale_y(18), title, 1, 0)
    canvas.text(layout.scale_x(476), layout.scale_y(18), "95%", 1, 0)
end

function chrome.dock(actions, selected, active)
    -- Keep the active dock action unmistakable on grayscale displays.
    local area = layout.standard().dock
    canvas.rectangle(area.x, area.y, area.width, area.height, 13)
    canvas.rectangle(area.x, area.y, area.width, 1, 8)
    local width = math.floor(area.width / math.max(1, #actions))
    for index, action in ipairs(actions) do
        local highlighted = active and index == selected
        if highlighted then
            canvas.rectangle(
                area.x + (index - 1) * width + 3,
                area.y + 3,
                width - 6,
                area.height - 6,
                0
            )
        end
        if index > 1 then
            canvas.rectangle(area.x + (index - 1) * width, area.y, 1, area.height, 8)
        end
        canvas.text(
            area.x + (index - 1) * width + layout.scale_x(24),
            layout.scale_y(906),
            action.label,
            1,
            highlighted and 15 or 0
        )
        if action.icon ~= nil then
            canvas.text(
                area.x + (index - 1) * width + layout.scale_x(18),
                layout.scale_y(906),
                action.icon,
                4,
                highlighted and 15 or 0
            )
        end
    end
end

return chrome
