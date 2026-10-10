local input = {}

local keys = { [1073741906] = "up", [1073741905] = "down", [13] = "push" }

function input.normalize(event)
    if event.type == 4 then
        return { kind = "touch", phase = "up", x = event.x, y = event.y }
    end
    if event.type == 2 then
        return { kind = "touch", phase = "down", x = event.x, y = event.y }
    end
    if event.type == 5 and keys[event.button] then
        return {
            kind = "key",
            phase = event.pressed and "down" or "up",
            key = keys[event.button],
            time = event.time,
        }
    end
    return { kind = "none" }
end

return input
