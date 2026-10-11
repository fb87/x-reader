local layout = require("layout")
local theme = require("theme")
local widget = require("ui.widget")

return function(properties)
    local dialog = widget.base(properties)
    dialog.title = properties.title or "Confirm"
    dialog.message = properties.message or ""
    dialog.cover = properties.cover
    dialog.actions = properties.actions or { { label = "Cancel" }, { label = "OK" } }
    dialog.selected = 1
    dialog.visible = false

    function dialog:show(title, message, actions, cover)
        self.title = title or self.title
        self.message = message or self.message
        self.actions = actions or self.actions
        self.cover = cover or self.cover
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
        if self.cover ~= nil then
            canvas.rectangle(bounds.x + 24, bounds.y + 72, 96, 136, theme.color.divider)
            canvas.text(
                bounds.x + 48,
                layout.text_y(
                    { x = bounds.x + 24, y = bounds.y + 72, width = 96, height = 136 },
                    self.cover,
                    6
                ),
                self.cover,
                6,
                theme.color.text
            )
        end
        local message_x = self.cover ~= nil and bounds.x + 144 or bounds.x + 24
        local message_width = self.cover ~= nil and bounds.width - 168 or bounds.width - 48
        local _, _, message_top = canvas.measure("Ag", 1)
        local message_y = bounds.y + 72 - message_top
        for paragraph in self.message:gmatch("[^\n]+") do
            local line = ""
            for word in paragraph:gmatch("%S+") do
                local candidate = line == "" and word or line .. " " .. word
                if line ~= "" and canvas.measure(candidate, 1) > message_width then
                    canvas.text(message_x, message_y, line, 1, theme.color.text)
                    message_y = message_y + 24
                    line = word
                else
                    line = candidate
                end
            end
            if line ~= "" then
                canvas.text(message_x, message_y, line, 1, theme.color.text)
                message_y = message_y + 24
            end
        end
        local width = math.floor(bounds.width / #self.actions)
        local button_y = bounds.y + bounds.height - 64
        local button_height = 48
        for index, action in ipairs(self.actions) do
            local x = bounds.x + (index - 1) * width
            if index == self.selected then
                canvas.rectangle(x + 12, button_y, width - 24, button_height, theme.color.selected)
            end
            local icon_width = action.icon and canvas.measure(action.icon, 6) or 0
            local label_width = canvas.measure(action.label, 1)
            local gap = action.icon and 8 or 0
            local group_width = icon_width + gap + label_width
            local group_x = x + math.floor((width - group_width) / 2)
            if action.icon ~= nil then
                canvas.text(
                    group_x,
                    layout.text_y(
                        { x = x, y = button_y, width = width, height = button_height },
                        action.icon,
                        6
                    ),
                    action.icon,
                    6,
                    theme.color.text
                )
            end
            canvas.text(
                group_x + icon_width + gap,
                layout.text_y(
                    { x = x, y = button_y, width = width, height = button_height },
                    action.label,
                    1
                ),
                action.label,
                1,
                theme.color.text
            )
        end
    end

    function dialog:layout(bounds)
        self.bounds = bounds or layout.rect(24, 300, 492, 307)
    end

    dialog:layout()
    return dialog
end
