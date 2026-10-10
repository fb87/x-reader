local chrome = require("chrome")
local widget = require("ui.widget")

return function(properties)
    local bar = widget.base(properties)
    bar.actions = properties.actions or {}
    bar.selected = properties.selected or 1
    function bar:draw()
        chrome.dock(self.actions, self.selected, true)
    end
    function bar:move(direction)
        self.selected = ((self.selected - 1 + direction) % #self.actions) + 1
    end
    function bar:activate()
        local action = self.actions[self.selected]
        if action and action.invoke then
            action.invoke()
        end
    end
    return bar
end
