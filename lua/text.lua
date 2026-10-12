local text = {}

-- Shortens `value` to fit `width` px at `size`, appending "...". Walks back
-- one byte at a time but skips UTF-8 continuation bytes (0x80-0xBF) so a
-- multi-byte Vietnamese character is never split mid-codepoint.
function text.truncate(value, size, width)
    local suffix = "..."
    if canvas.measure(value, size) <= width then
        return value
    end
    local length = #value
    while length > 0 and canvas.measure(value:sub(1, length) .. suffix, size) > width do
        length = length - 1
        while length > 0 do
            local byte = value:byte(length)
            if byte < 128 or byte >= 192 then
                break
            end
            length = length - 1
        end
    end
    return value:sub(1, length) .. suffix
end

return text
