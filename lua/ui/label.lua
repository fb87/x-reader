local layout = require("layout")
local widget = require("ui.widget")

return function(properties)
    local label = widget.base(properties)
    label.text = properties.text or ""
    label.size = properties.size or 1
    label.gray = properties.gray or 0
    function label:draw()
        canvas.text(
            self.bounds.x,
            layout.text_y(self.bounds, self.text, self.size),
            self.text,
            self.size,
            self.gray
        )
    end
    return label
end
