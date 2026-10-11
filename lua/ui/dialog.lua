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
        canvas.rectangle(0, 0, layout.width, layout.height, 8)
        canvas.rectangle(bounds.x, bounds.y, bounds.width, bounds.height, theme.color.background)
        canvas.text(
            bounds.x + layout.scale_x(24),
            bounds.y + layout.scale_y(24),
            self.title,
            2,
            theme.color.text
        )
        if self.cover ~= nil then
            local cover_x = bounds.x + layout.scale_x(24)
            local cover_y = bounds.y + layout.scale_y(72)
            local cover_width = layout.scale_x(96)
            local cover_height = layout.scale_y(136)
            canvas.rectangle(cover_x, cover_y, cover_width, cover_height, theme.color.divider)
            canvas.text(
                bounds.x + layout.scale_x(48),
                layout.text_y(
                    { x = cover_x, y = cover_y, width = cover_width, height = cover_height },
                    self.cover,
                    6
                ),
                self.cover,
                6,
                theme.color.text
            )
        end
        local message_x = self.cover ~= nil and bounds.x + layout.scale_x(144)
            or bounds.x + layout.scale_x(24)
        local message_width = self.cover ~= nil and bounds.width - layout.scale_x(168)
            or bounds.width - layout.scale_x(48)
        local _, _, message_top = canvas.measure("Ag", 1)
        local message_y = bounds.y + layout.scale_y(72) - message_top
        local line_height = layout.scale_y(24)
        for paragraph in self.message:gmatch("[^\n]+") do
            local line = ""
            for word in paragraph:gmatch("%S+") do
                local candidate = line == "" and word or line .. " " .. word
                if line ~= "" and canvas.measure(candidate, 1) > message_width then
                    canvas.text(message_x, message_y, line, 1, theme.color.text)
                    message_y = message_y + line_height
                    line = word
                else
                    line = candidate
                end
            end
            if line ~= "" then
                canvas.text(message_x, message_y, line, 1, theme.color.text)
                message_y = message_y + line_height
            end
        end
        local width = math.floor(bounds.width / #self.actions)
        local button_y = bounds.y + bounds.height - layout.scale_y(64)
        local button_height = layout.scale_y(48)
        for index, action in ipairs(self.actions) do
            local x = bounds.x + (index - 1) * width
            if index == self.selected then
                canvas.rectangle(
                    x + layout.scale_x(12),
                    button_y,
                    width - layout.scale_x(24),
                    button_height,
                    theme.color.selected
                )
            end
            local icon_width = action.icon and canvas.measure(action.icon, 6) or 0
            local label_width = canvas.measure(action.label, 1)
            local gap = action.icon and layout.scale_x(8) or 0
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
        if bounds ~= nil then
            self.bounds = bounds
            return
        end
        local width = layout.scale_x(492)
        local height = layout.scale_y(307)
        self.bounds = {
            x = math.floor((layout.width - width) / 2),
            y = math.floor((layout.height - height) / 2),
            width = width,
            height = height,
        }
    end

    dialog:layout()
    return dialog
end
