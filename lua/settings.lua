local settings = {
    font_size = "medium",
    partial_refresh = false,
    brightness = 50,
    show_page_number = true,
    show_clock = true,
}

local defaults = {
    font_size = "medium",
    partial_refresh = false,
    brightness = 50,
    show_page_number = true,
    show_clock = true,
}

function settings.reset()
    for key, value in pairs(defaults) do
        settings[key] = value
    end
end

function settings.apply()
    return true
end

function settings.serialize()
    return table.concat({
        "font_size=" .. settings.font_size,
        "partial_refresh=" .. tostring(settings.partial_refresh),
        "brightness=" .. tostring(settings.brightness),
        "show_page_number=" .. tostring(settings.show_page_number),
        "show_clock=" .. tostring(settings.show_clock),
    }, "\n")
end

function settings.restore(values)
    if
        values.font_size == "small"
        or values.font_size == "medium"
        or values.font_size == "large"
    then
        settings.font_size = values.font_size
    end
    settings.partial_refresh = values.partial_refresh == "true"
    settings.brightness = values.brightness == "75" and 75 or 50
    settings.show_page_number = values.show_page_number ~= "false"
    settings.show_clock = values.show_clock ~= "false"
end

function settings.toggle_text_scale()
    if settings.font_size == "small" then
        settings.font_size = "medium"
    elseif settings.font_size == "medium" then
        settings.font_size = "large"
    else
        settings.font_size = "small"
    end
end

function settings.font_scale()
    if settings.font_size == "small" then
        return 1
    elseif settings.font_size == "large" then
        return 3
    end
    return 2
end

function settings.decrease_font_size()
    if settings.font_size == "large" then
        settings.font_size = "medium"
    elseif settings.font_size == "medium" then
        settings.font_size = "small"
    end
end

function settings.increase_font_size()
    if settings.font_size == "small" then
        settings.font_size = "medium"
    elseif settings.font_size == "medium" then
        settings.font_size = "large"
    end
end

function settings.toggle_refresh()
    settings.partial_refresh = not settings.partial_refresh
end

function settings.toggle_brightness()
    settings.brightness = settings.brightness == 50 and 75 or 50
end

function settings.toggle_page_number()
    settings.show_page_number = not settings.show_page_number
end

function settings.toggle_clock()
    settings.show_clock = not settings.show_clock
end

return settings
