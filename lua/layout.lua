local layout = {
    width = 540,
    height = 960,
    design_width = 540,
    design_height = 960,
}

function layout.configure(width, height)
    layout.width = width
    layout.height = height
end

function layout.scale_x(value)
    return math.floor(value * layout.width / layout.design_width + 0.5)
end

function layout.scale_y(value)
    return math.floor(value * layout.height / layout.design_height + 0.5)
end

function layout.rect(x, y, width, height)
    return {
        x = layout.scale_x(x),
        y = layout.scale_y(y),
        width = layout.scale_x(width),
        height = layout.scale_y(height),
    }
end

function layout.standard()
    return {
        status = layout.rect(0, 0, 540, 56),
        content = layout.rect(0, 56, 540, 824),
        dock = layout.rect(0, 880, 540, 80),
    }
end

function layout.fullscreen()
    return layout.rect(0, 0, 540, 960)
end

return layout
