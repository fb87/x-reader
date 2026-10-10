local chrome = require("chrome")
local widget = require("ui.widget")

return function(properties)
    local bar = widget.base(properties)
    bar.title = properties.title or ""
    function bar:draw()
        chrome.status(self.title, true)
    end
    return bar
end
