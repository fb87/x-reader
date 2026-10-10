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

function layout.text_y(rect, text, size)
    local _, height, top = canvas.measure(text, size)
    return rect.y + math.floor((rect.height - height) / 2) - top
end

function layout.text_group_y(rect, entries, gap)
    local metrics = {}
    local total = 0
    for index, entry in ipairs(entries) do
        local _, height, top = canvas.measure(entry.text, entry.size)
        metrics[index] = { height = height, top = top }
        total = total + height
        if index > 1 then
            total = total + gap
        end
    end
    local cursor = rect.y + math.floor((rect.height - total) / 2)
    local positions = {}
    for index, metric in ipairs(metrics) do
        positions[index] = cursor - metric.top
        cursor = cursor + metric.height + gap
    end
    return positions
end

function layout.standard()
    return {
        status = layout.rect(0, 0, 540, 48),
        content = layout.rect(0, 48, 540, 848),
        dock = layout.rect(0, 896, 540, 64),
    }
end

function layout.content_height(top, bottom_margin)
    local regions = layout.standard()
    return regions.content.y + regions.content.height - top - (bottom_margin or 0)
end

function layout.fullscreen()
    return layout.rect(0, 0, 540, 960)
end

return layout
