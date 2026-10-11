local chrome = require("chrome")
local layout = require("layout")
local widget = require("ui.widget")

return function(properties)
    local bar = widget.base(properties)
    bar.actions = properties.actions or {}
    bar.selected = properties.selected or 1
    function bar:draw(active)
        chrome.dock(self.bounds, self.actions, self.selected, active ~= false)
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
    -- Maps a touch point to a 1-based slot index, matching whichever
    -- orientation chrome.dock() rendered the slots in (row or rail).
    function bar:hit(x, y)
        return layout.slot_index(self.bounds, x, y, #self.actions)
    end
    return bar
end
