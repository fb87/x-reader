local M = {}

function M.column(spec)
    local y = spec.y or 0
    local spacing = spec.spacing or 0
    local items = spec.items or {}
    for _, item in ipairs(items) do
        item.x = item.x or spec.x or 0
        item.y = item.y or y
        item.w = item.w or spec.w or 0
        item.h = item.h or spec.item_h or 0
        y = item.y + item.h + spacing
    end
    return items
end

return M
