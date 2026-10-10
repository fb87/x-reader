local layout = require("layout")
local theme = require("theme")
local widget = require("ui.widget")

return function(properties)
    local dialog = widget.base(properties)
    dialog.title = properties.title or "Confirm"
    dialog.message = properties.message or ""
    dialog.actions = properties.actions or { { label = "Cancel" }, { label = "OK" } }
    dialog.selected = 1
    dialog.visible = false

    function dialog:show(title, message, actions)
        self.title = title or self.title
        self.message = message or self.message
        self.actions = actions or self.actions
        self.selected = 1
        self.visible = true
    end

    function dialog:hide()
        self.visible = false
    end

    function dialog:move(direction)
        self.selected = ((self.selected - 1 + direction) % #self.actions) + 1
    end

    function dialog:activate()
        local action = self.actions[self.selected]
        if action and action.invoke then
            action.invoke(self)
        end
    end

    function dialog:draw()
        if not self.visible then
            return
        end
        local bounds = self.bounds
        canvas.rectangle(0, 0, 540, 960, 8)
        canvas.rectangle(bounds.x, bounds.y, bounds.width, bounds.height, theme.color.background)
        canvas.text(bounds.x + 24, bounds.y + 24, self.title, 2, theme.color.text)
        canvas.text(bounds.x + 24, bounds.y + 76, self.message, 1, theme.color.text)
        local width = math.floor(bounds.width / #self.actions)
        for index, action in ipairs(self.actions) do
            local x = bounds.x + (index - 1) * width
            if index == self.selected then
                canvas.rectangle(
                    x + 4,
                    bounds.y + bounds.height - 64,
                    width - 8,
                    52,
                    theme.color.selected
                )
            end
            canvas.text(
                x + 20,
                bounds.y + bounds.height - 44,
                action.label,
                1,
                index == self.selected and theme.color.selected_text or theme.color.text
            )
        end
    end

    function dialog:layout(bounds)
        self.bounds = bounds or layout.rect(48, 330, 444, 300)
    end

    dialog:layout()
    return dialog
end
