local widget = {}

function widget.base(properties)
    local object = properties or {}
    object.bounds = object.bounds or { x = 0, y = 0, width = 0, height = 0 }
    object.visible = object.visible ~= false
    object.enabled = object.enabled ~= false
    function object:layout(bounds)
        self.bounds = bounds
    end
    function object:hit(x, y)
        local r = self.bounds
        return self.visible
            and self.enabled
            and x >= r.x
            and x < r.x + r.width
            and y >= r.y
            and y < r.y + r.height
    end
    return object
end

return widget
