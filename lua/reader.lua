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

local function wrap_paragraph(paragraph, width, size, output)
    if paragraph == "" then
        output[#output + 1] = ""
        return
    end
    local line = ""
    for word in paragraph:gmatch("%S+") do
        local candidate = line == "" and word or line .. " " .. word
        if line ~= "" and canvas.measure(candidate, size) > width then
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

local function has_supported_text(text)
    for _, codepoint in utf8.codes(text) do
        if
            (codepoint >= 0x30 and codepoint <= 0x7a)
            or (codepoint >= 0xc0 and codepoint <= 0x24f)
            or (codepoint >= 0x1e00 and codepoint <= 0x1eff)
        then
            return true
        end
    end
    return false
end

reader.book = nil
reader.page = 1
reader.bookmarks = {}
reader.document = nil
reader.content = nil
reader.error = nil
reader.spine = nil
reader.spine_index = 1
reader.page_count = #pages

function reader.open(book)
    reader.book = book
    reader.page = 1
    reader.bookmarks = {}
    reader.document = nil
    reader.content = nil
    reader.error = nil
    reader.spine = nil
    reader.spine_index = 1
    reader.page_count = #pages
    if book.path ~= nil then
        local document, error_message = epub.open(book.path)
        if document ~= nil then
            reader.document = document
            local metadata = epub.metadata(document)
            local spine = epub.spine(document)
            book.title = metadata.title ~= nil and metadata.title ~= "" and metadata.title
                or book.title
            book.author = metadata.creator ~= nil and metadata.creator or book.author
            reader.spine = spine
            reader.page_count = 1
            reader.load_spine = function(index)
                reader.spine_index = index
                reader.page = 1
                local content, text_error = epub.text(document, index)
                reader.content = content
                reader.error = content == nil and (text_error or "Unable to read EPUB content")
                    or nil
            end
            reader.load_spine(1)
        else
            reader.error = error_message or "Unable to open EPUB"
        end
    end
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
    if reader.book ~= nil and reader.page < reader.page_count then
        reader.page = reader.page + 1
    elseif reader.spine ~= nil and reader.spine_index < #reader.spine then
        reader.load_spine(reader.spine_index + 1)
    end
end

function reader.previous_page()
    if reader.page > 1 then
        reader.page = reader.page - 1
    elseif reader.spine ~= nil and reader.spine_index > 1 then
        reader.load_spine(reader.spine_index - 1)
        reader.page = reader.page_count
    end
end

function reader.go_to_page(page)
    reader.page = math.max(1, math.min(#pages, page))
end

function reader.progress_width(width)
    if reader.book == nil or reader.page_count == 0 then
        return 0
    end
    return math.floor(width * reader.page / reader.page_count)
end

function reader.chapter_name()
    if reader.content ~= nil then
        for line in reader.content:gmatch("[^\n]+") do
            local text = line:gsub("\194\160", " "):gsub("&nbsp;", " "):match("^%s*(.-)%s*$")
            if text ~= "" then
                return text
            end
        end
    end
    if reader.spine == nil then
        return "Sample chapter"
    end
    local item = reader.spine[reader.spine_index]
    local href = item and item.href or nil
    if href == nil or href == "" then
        return string.format("Chapter %d", reader.spine_index)
    end
    local name = href:gsub("[?#].*$", ""):match("([^/]+)$") or href
    name = name:gsub("%.[^%.]+$", "")
    return name ~= "" and name or string.format("Chapter %d", reader.spine_index)
end

function reader.page_lines(scale, limit, max_width)
    if reader.content ~= nil then
        local wrapped = {}
        local pending_break = false
        for paragraph in (reader.content .. "\n"):gmatch("(.-)\n") do
            local normalized = paragraph
                :gsub("\194\160", " ")
                :gsub("&nbsp;", " ")
                :gsub("%s+", " ")
                :match("^%s*(.-)%s*$")
            if normalized == "" then
                pending_break = #wrapped > 0
            elseif has_supported_text(normalized) then
                if pending_break and wrapped[#wrapped] ~= "" then
                    wrapped[#wrapped + 1] = ""
                end
                wrap_paragraph(normalized, max_width or 452, scale, wrapped)
                pending_break = false
            end
        end
        local pages_available = math.max(1, math.ceil(#wrapped / limit))
        reader.page_count = pages_available
        local first = (reader.page - 1) * limit + 1
        local result = {}
        for index = first, math.min(#wrapped, first + limit - 1) do
            result[#result + 1] = wrapped[index]
        end
        return result
    end
    local width = max_width or 452
    local wrapped = {}
    for _, paragraph in ipairs(pages[reader.page] or pages[1]) do
        wrap_paragraph(paragraph, width, scale, wrapped)
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
