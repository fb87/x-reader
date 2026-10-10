local settings = {
    font_size = "medium",
    partial_refresh = false,
    brightness = 50,
    show_page_number = true,
    show_clock = true,
}

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
