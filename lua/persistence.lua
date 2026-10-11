local bookshelf = require("bookshelf")
local reader = require("reader")
local settings = require("settings")

local persistence = {}

local function parse(data)
    local values = {}
    for line in data:gmatch("[^\n]+") do
        local key, value = line:match("^([^=]+)=(.*)$")
        if key ~= nil then
            values[key] = value
        end
    end
    return values
end

function persistence.load()
    local data = system.state_load()
    if data == nil then
        bookshelf.start()
        return
    end
    local values = parse(data)
    settings.restore(values)
    local library_count = tonumber(values.library_count)
    if library_count ~= nil and library_count >= 0 then
        local cached = {}
        for index = 1, library_count do
            local path = values["library_" .. index .. "_path"]
            if path == nil then
                cached = {}
                break
            end
            cached[#cached + 1] = {
                id = path,
                path = path,
                title = values["library_" .. index .. "_title"] or path,
                author = values["library_" .. index .. "_author"] or "",
                size = tonumber(values["library_" .. index .. "_size"]) or 0,
                pages = 0,
            }
        end
        if #cached == library_count then
            bookshelf.restore(cached)
        else
            bookshelf.start()
        end
    else
        bookshelf.start()
    end
    if values.book_path == nil then
        return
    end
    for _, book in ipairs(bookshelf.books) do
        if book.path == values.book_path then
            reader.restore_position(
                book,
                tonumber(values.book_page) or 1,
                tonumber(values.book_spine) or 1
            )
            return
        end
    end
end

function persistence.save()
    local values = { settings.serialize() }
    values[#values + 1] = "library_count=" .. tostring(#bookshelf.books)
    for index, book in ipairs(bookshelf.books) do
        values[#values + 1] = "library_" .. index .. "_path=" .. (book.path or "")
        values[#values + 1] = "library_" .. index .. "_title=" .. (book.title or "")
        values[#values + 1] = "library_" .. index .. "_author=" .. (book.author or "")
        values[#values + 1] = "library_" .. index .. "_size=" .. tostring(book.size or 0)
    end
    if reader.last_book ~= nil then
        values[#values + 1] = "book_path=" .. (reader.last_book.path or "")
        values[#values + 1] = "book_page=" .. tostring(reader.page)
        values[#values + 1] = "book_spine=" .. tostring(reader.spine_index)
    end
    system.state_save(table.concat(values, "\n"))
end

return persistence
