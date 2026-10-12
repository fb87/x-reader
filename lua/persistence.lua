local bookshelf = require("bookshelf")
local reader = require("reader")
local settings = require("settings")

local persistence = {}

-- Set once by app.lua to `context.navigation_snapshot`, so this module can
-- include the nav stack + per-scene stage fields in the same SD-card record
-- without owning navigation state itself (that stays in app.lua's closure).
local navigation_provider = nil

function persistence.set_navigation_provider(provider)
    navigation_provider = provider
end

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
    if values.book_path ~= nil then
        for _, book in ipairs(bookshelf.books) do
            if book.path == values.book_path then
                reader.restore_position(
                    book,
                    tonumber(values.book_page) or 1,
                    tonumber(values.book_spine) or 1
                )
                break
            end
        end
    end
    persistence.navigation = nil
    if values.stage_stack ~= nil and values.stage_stack ~= "" then
        local stack = {}
        for id in values.stage_stack:gmatch("[^,]+") do
            stack[#stack + 1] = id
        end
        local stages = {}
        for _, id in ipairs(stack) do
            local raw = values["stage_" .. id]
            if raw ~= nil then
                local fields = {}
                for pair in raw:gmatch("[^|]+") do
                    local field, value = pair:match("^([^:]+):(.*)$")
                    if field ~= nil then
                        fields[field] = value
                    end
                end
                stages[id] = fields
            end
        end
        persistence.navigation = { stack = stack, stages = stages }
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
    local navigation = navigation_provider and navigation_provider()
    if navigation ~= nil and navigation.stack ~= nil and #navigation.stack > 0 then
        values[#values + 1] = "stage_stack=" .. table.concat(navigation.stack, ",")
        for _, id in ipairs(navigation.stack) do
            local fields = navigation.stages[id]
            if fields ~= nil then
                local parts = {}
                for key, value in pairs(fields) do
                    parts[#parts + 1] = key .. ":" .. tostring(value)
                end
                if #parts > 0 then
                    values[#values + 1] = "stage_" .. id .. "=" .. table.concat(parts, "|")
                end
            end
        end
    end
    system.state_save(table.concat(values, "\n"))
end

return persistence
