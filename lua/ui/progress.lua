local widget = require("ui.widget")

return function(properties)
    local progress = widget.base(properties)
    progress.value = properties.value or 0
    function progress:draw()
        canvas.rectangle(self.bounds.x, self.bounds.y, self.bounds.width, self.bounds.height, 10)
        canvas.rectangle(
            self.bounds.x,
            self.bounds.y,
            math.floor(self.bounds.width * self.value),
            self.bounds.height,
            4
        )
    end
    return progress
end
