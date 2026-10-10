local layout = require("layout")
local theme = require("theme")
local widget = require("ui.widget")

return function(properties)
    local list = widget.base(properties)
    list.rows = properties.rows or {}
    list.selected_id = properties.selected_id
    list.active = properties.active ~= false
    list.row_height = properties.row_height or 96
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
            canvas.rectangle(
                bounds.x,
                bounds.y,
                bounds.width,
                bounds.height,
                selected and theme.color.selected or theme.color.surface
            )
            local text_gray = selected and theme.color.selected_text or theme.color.text
            if row.icon then
                canvas.text(
                    bounds.x + 24,
                    layout.text_y(bounds, row.icon, 6),
                    row.icon,
                    6,
                    text_gray
                )
            end
            local text_x = bounds.x + (row.icon and 88 or 24)
            if row.subtitle then
                local text_y = layout.text_group_y(bounds, {
                    { text = row.title, size = 2 },
                    { text = row.subtitle, size = 1 },
                }, 4)
                canvas.text(text_x, text_y[1], row.title, 2, text_gray)
                canvas.text(text_x, text_y[2], row.subtitle, 1, text_gray)
            else
                canvas.text(text_x, layout.text_y(bounds, row.title, 2), row.title, 2, text_gray)
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
        end
    end
    return list
end
