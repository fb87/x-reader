local bookshelf = { books = {} }
local max_initial_books = 64

local function scan(path, depth)
    if depth > 8 or #bookshelf.books >= max_initial_books then
        return
    end
    local entries = library.list(path)
    if entries == nil then
        return
    end
    for _, entry in ipairs(entries) do
        if #bookshelf.books >= max_initial_books then
            return
        end
        if entry.directory then
            scan(entry.path, depth + 1)
        elseif entry.name:lower():sub(-5) == ".epub" then
            bookshelf.books[#bookshelf.books + 1] = {
                id = entry.path,
                title = entry.name:sub(1, -6),
                author = "",
                path = entry.path,
                size = entry.size,
                pages = 0,
            }
        end
    end
end

function bookshelf.refresh()
    bookshelf.books = {}
    scan("", 0)
end

function bookshelf.start()
    bookshelf.refresh()
end

bookshelf.start()
return bookshelf
