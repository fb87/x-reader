local layout = require("layout")
local theme = require("theme")
local widget = require("ui.widget")

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

return function(properties)
    local list = widget.base(properties)
    list.rows = properties.rows or {}
    list.selected_id = properties.selected_id
    list.active = properties.active ~= false
    list.row_height = properties.row_height or 84
    list.page = 1
    list.on_activate = properties.on_activate
    function list:move(direction)
        if #self.rows == 0 then
            return
        end
        local selected = 1
        for index, row in ipairs(self.rows) do
            if row.id == self.selected_id then
                selected = index
                break
            end
        end
        selected = ((selected - 1 + direction) % #self.rows) + 1
        self.selected_id = self.rows[selected].id
        local page_size = math.max(1, math.floor(self.bounds.height / self.row_height))
        self.page_size = page_size
        self.page_count = math.max(1, math.ceil(#self.rows / page_size))
        self.page = math.floor((selected - 1) / page_size) + 1
    end
    function list:activate()
        for _, row in ipairs(self.rows) do
            if row.id == self.selected_id and self.on_activate then
                self.on_activate(row)
            end
        end
    end
    function list:draw()
        local page_size = math.max(1, math.floor(self.bounds.height / self.row_height))
        self.page_size = page_size
        self.page_count = math.max(1, math.ceil(#self.rows / page_size))
        local first = (self.page - 1) * page_size + 1
        local last = math.min(#self.rows, first + page_size - 1)
        for index = first, last do
            local row = self.rows[index]
            local bounds = {
                x = self.bounds.x,
                y = self.bounds.y + (index - first) * self.row_height,
                width = self.bounds.width,
                height = self.row_height,
            }
            local selected = self.active and row.id == self.selected_id
            if selected then
                canvas.rectangle(
                    bounds.x,
                    bounds.y - 1,
                    bounds.width,
                    bounds.height + 1,
                    theme.color.selected
                )
            end
            local text_gray = selected and theme.color.selected_text or theme.color.text
            local text_x = bounds.x + (row.icon and 88 or 24)
            local text_width = bounds.width - (text_x - bounds.x) - (row.trailing and 96 or 16)
            local title = truncate_text(row.title, 2, text_width)
            local subtitle = row.subtitle
                    and row.subtitle ~= ""
                    and truncate_text(row.subtitle, 1, text_width)
                or nil
            local text_y
            if subtitle then
                text_y = layout.text_group_y(bounds, {
                    { text = title, size = 2 },
                    { text = subtitle, size = 1 },
                }, 4)
            else
                text_y = { layout.text_y(bounds, title, 2) }
            end
            if row.icon then
                local icon_y = layout.text_y(bounds, row.icon, 6)
                if subtitle then
                    local _, _, title_top, title_bottom = canvas.measure(title, 2)
                    local _, _, subtitle_top, subtitle_bottom = canvas.measure(subtitle, 1)
                    local visual_top = text_y[1] + title_top
                    local visual_bottom = text_y[2] + subtitle_bottom
                    local _, icon_height, icon_top = canvas.measure(row.icon, 6)
                    icon_y = math.floor((visual_top + visual_bottom - icon_height) / 2 - icon_top)
                end
                canvas.text(bounds.x + 24, icon_y, row.icon, 6, text_gray)
            end
            if subtitle then
                canvas.text(text_x, text_y[1], title, 2, text_gray)
                canvas.text(text_x, text_y[2], subtitle, 1, text_gray)
            else
                canvas.text(text_x, text_y[1], title, 2, text_gray)
            end
            if row.trailing then
                canvas.text(
                    bounds.x + bounds.width - 72,
                    layout.text_y(bounds, row.trailing, 1),
                    row.trailing,
                    1,
                    text_gray
                )
            end
            if index < last and not selected then
                canvas.rectangle(
                    bounds.x + 12,
                    bounds.y + bounds.height - 1,
                    bounds.width - 24,
                    1,
                    theme.color.divider
                )
            end
        end
    end
    return list
end
