local layout = require("layout")
local theme = require("theme")
local widget = require("ui.widget")

return function(properties)
    local button = widget.base(properties)
    button.label = properties.label or ""
    button.icon = properties.icon
    button.size = properties.size or 1
    button.selected = false
    button.on_activate = properties.on_activate
    function button:draw()
        local color = self.selected and theme.color.selected or theme.color.surface
        local text_color = self.selected and theme.color.selected_text or theme.color.text
        canvas.rectangle(self.bounds.x, self.bounds.y, self.bounds.width, self.bounds.height, color)
        local icon_width = self.icon and canvas.measure(self.icon, 6) or 0
        local text_width = canvas.measure(self.label, self.size)
        local gap = self.icon and theme.spacing.small or 0
        local x = self.bounds.x
            + math.floor((self.bounds.width - icon_width - gap - text_width) / 2)
        if self.icon then
            canvas.text(x, layout.text_y(self.bounds, self.icon, 6), self.icon, 6, text_color)
            x = x + icon_width + gap
        end
        canvas.text(
            x,
            layout.text_y(self.bounds, self.label, self.size),
            self.label,
            self.size,
            text_color
        )
    end
    function button:activate()
        if self.enabled and self.on_activate then
            self.on_activate()
        end
    end
    return button
end
