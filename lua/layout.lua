local layout = {
    width = 540,
    height = 960,
    design_width = 540,
    design_height = 960,
}

local STATUS_HEIGHT = 48
local DOCK_BAND = 64
-- The design canvas every scene is written against: a tall portrait shape.
-- scale_x/scale_y stay close to 1 as long as the real canvas is roughly the
-- same shape. In landscape the real canvas is wide instead of tall, so the
-- design reference swaps too -- otherwise every vertically-sized element
-- would scale against a 960-unit-tall reference while the actual logical
-- height shrank to a few hundred pixels, squashing everything.
local BASE_DESIGN_WIDTH = 540
local BASE_DESIGN_HEIGHT = 960

function layout.configure(width, height)
    layout.width = width
    layout.height = height
    if width > height then
        layout.design_width = BASE_DESIGN_HEIGHT
        layout.design_height = BASE_DESIGN_WIDTH
    else
        layout.design_width = BASE_DESIGN_WIDTH
        layout.design_height = BASE_DESIGN_HEIGHT
    end
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

function layout.contains(box, x, y)
    return x >= box.x and x < box.x + box.width and y >= box.y and y < box.y + box.height
end

-- Maps a touch point inside `box` to a 1-based slot index, splitting evenly
-- along whichever axis is longer (so a wide dock strip indexes by x, a
-- narrow side rail indexes by y) without the caller needing to know which.
function layout.slot_index(box, x, y, count)
    if count <= 0 or not layout.contains(box, x, y) then
        return nil
    end
    if box.width >= box.height then
        return math.max(1, math.min(count, math.floor((x - box.x) / (box.width / count)) + 1))
    end
    return math.max(1, math.min(count, math.floor((y - box.y) / (box.height / count)) + 1))
end

-- Carves a fixed-size strip off one edge of `box` (in design units, scaled on
-- the axis that edge belongs to) and returns `edge_box, remainder_box`. This
-- is how structural chrome (status bar, dock) gets pinned to a side
-- regardless of which way the display is oriented.
function layout.split(box, edge, size)
    if edge == "top" or edge == "bottom" then
        local amount = math.min(box.height, layout.scale_y(size))
        if edge == "top" then
            return { x = box.x, y = box.y, width = box.width, height = amount }, {
                x = box.x,
                y = box.y + amount,
                width = box.width,
                height = box.height - amount,
            }
        end
        return {
            x = box.x,
            y = box.y + box.height - amount,
            width = box.width,
            height = amount,
        }, { x = box.x, y = box.y, width = box.width, height = box.height - amount }
    elseif edge == "left" or edge == "right" then
        local amount = math.min(box.width, layout.scale_x(size))
        if edge == "left" then
            return { x = box.x, y = box.y, width = amount, height = box.height }, {
                x = box.x + amount,
                y = box.y,
                width = box.width - amount,
                height = box.height,
            }
        end
        return {
            x = box.x + box.width - amount,
            y = box.y,
            width = amount,
            height = box.height,
        }, { x = box.x, y = box.y, width = box.width - amount, height = box.height }
    end
    error("layout.split: unknown edge " .. tostring(edge))
end

-- Flows `items` along one axis of `box`, stretching each to the full
-- cross-axis extent. Each item is either `{ size = N }` (fixed, in design
-- units along the main axis) or `{ fill = true }` / `{ fill = weight }`
-- (shares whatever space is left over, proportional to weight). Returns an
-- array of pixel-space boxes, one per item, in the same order.
function layout.stack(box, direction, options, items)
    options = options or {}
    local pad_x = layout.scale_x(options.padding or 0)
    local pad_y = layout.scale_y(options.padding or 0)
    local gap = direction == "row" and layout.scale_x(options.gap or 0)
        or layout.scale_y(options.gap or 0)
    local inner = {
        x = box.x + pad_x,
        y = box.y + pad_y,
        width = box.width - pad_x * 2,
        height = box.height - pad_y * 2,
    }
    local main_extent = direction == "row" and inner.width or inner.height
    local fixed_total = 0
    local fill_weight_total = 0
    for _, item in ipairs(items) do
        if item.size ~= nil then
            fixed_total = fixed_total
                + (direction == "row" and layout.scale_x(item.size) or layout.scale_y(item.size))
        elseif item.fill then
            fill_weight_total = fill_weight_total + (item.fill == true and 1 or item.fill)
        end
    end
    local gap_total = gap * math.max(0, #items - 1)
    local remaining = math.max(0, main_extent - fixed_total - gap_total)
    local boxes = {}
    local cursor = direction == "row" and inner.x or inner.y
    for index, item in ipairs(items) do
        local extent = 0
        if item.size ~= nil then
            extent = direction == "row" and layout.scale_x(item.size) or layout.scale_y(item.size)
        elseif item.fill then
            local weight = item.fill == true and 1 or item.fill
            extent = fill_weight_total > 0 and math.floor(remaining * weight / fill_weight_total)
                or 0
        end
        if direction == "row" then
            boxes[index] = { x = cursor, y = inner.y, width = extent, height = inner.height }
        else
            boxes[index] = { x = inner.x, y = cursor, width = inner.width, height = extent }
        end
        cursor = cursor + extent + gap
    end
    return boxes
end

function layout.column(box, options, items)
    return layout.stack(box, "column", options, items)
end

function layout.row(box, options, items)
    return layout.stack(box, "row", options, items)
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

-- Structural chrome regions, adapting to orientation: the status bar is
-- always a strip pinned to the top; the dock is a strip pinned to the
-- bottom in portrait, or a rail pinned to the left in landscape, so it
-- never eats into the short dimension of the display.
function layout.regions()
    local root = { x = 0, y = 0, width = layout.width, height = layout.height }
    local status, rest = layout.split(root, "top", STATUS_HEIGHT)
    local dock, content
    if layout.width > layout.height then
        dock, content = layout.split(rest, "left", DOCK_BAND)
    else
        dock, content = layout.split(rest, "bottom", DOCK_BAND)
    end
    return { status = status, content = content, dock = dock }
end

-- The full display minus the status strip, dock included. Used by
-- fullscreen-chrome scenes that draw their own status bar via chrome.status
-- but have no persistent dock (only an occasional overlay).
function layout.below_status()
    local root = { x = 0, y = 0, width = layout.width, height = layout.height }
    local _, rest = layout.split(root, "top", STATUS_HEIGHT)
    return rest
end

function layout.fullscreen()
    return { x = 0, y = 0, width = layout.width, height = layout.height }
end

return layout
