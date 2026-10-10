local reader = {}

local pages = {
    {
        "The first page is rendered by Lua.",
        "C provides the bounded canvas service,",
        "while the reader decides what to draw.",
        "",
        "The page surface remains independent of",
        "the physical e-paper controller.",
        "",
        "Text, navigation, and display composition",
        "are all application policy.",
    },
    {
        "A book is opened as a document session.",
        "Later pages will come from EPUB XHTML",
        "and the portable layout service.",
        "",
        "The same page model will run on the",
        "M5Paper and the desktop simulator.",
        "",
        "Storage and parsing stay behind the",
        "high-level Lua service boundary.",
    },
    {
        "The simulator makes page design fast.",
        "Edit this script, save it, and watch the",
        "running application reload transactionally.",
        "",
        "A failed reload keeps the previous page",
        "visible while the new VM is checked.",
        "",
        "This is the development loop for the",
        "reader application.",
    },
}

local function wrap_paragraph(paragraph, width, output)
    if paragraph == "" then
        output[#output + 1] = ""
        return
    end
    local line = ""
    for word in paragraph:gmatch("%S+") do
        local candidate = line == "" and word or line .. " " .. word
        if line ~= "" and #candidate > width then
            output[#output + 1] = line
            line = word
        else
            line = candidate
        end
    end
    if line ~= "" then
        output[#output + 1] = line
    end
end

reader.book = nil
reader.page = 1
reader.bookmarks = {}

function reader.open(book)
    reader.book = book
    reader.page = 1
    reader.bookmarks = {}
end

function reader.close()
    reader.book = nil
end

function reader.toggle_bookmark()
    reader.bookmarks[reader.page] = not reader.bookmarks[reader.page]
end

function reader.is_bookmarked()
    return reader.bookmarks[reader.page] == true
end

function reader.next_page()
    if reader.book ~= nil and reader.page < #pages then
        reader.page = reader.page + 1
    end
end

function reader.previous_page()
    if reader.page > 1 then
        reader.page = reader.page - 1
    end
end

function reader.go_to_page(page)
    reader.page = math.max(1, math.min(#pages, page))
end

function reader.progress_width(width)
    if reader.book == nil or #pages == 0 then
        return 0
    end
    return math.floor(width * reader.page / #pages)
end

function reader.page_lines(scale, limit)
    local width = scale == 1 and 50 or scale == 2 and 26 or 17
    local wrapped = {}
    for _, paragraph in ipairs(pages[reader.page] or pages[1]) do
        wrap_paragraph(paragraph, width, wrapped)
        if limit ~= nil and #wrapped >= limit then
            break
        end
    end
    if limit ~= nil then
        while #wrapped > limit do
            wrapped[#wrapped] = nil
        end
    end
    return wrapped
end

function reader.event(event)
    if event.type ~= 4 then
        return
    end
    if event.y < 80 then
        reader.close()
    elseif event.x >= 270 then
        reader.next_page()
    else
        reader.previous_page()
    end
end

return reader
