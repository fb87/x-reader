local bookshelf = require("bookshelf")
local reader = require("reader")
local settings = require("settings")

local screen = "splash"
local selected = 1
local splash_until = 0
local menu_selected = 1
local current_book = bookshelf.books[1]
local icons = {
    book = "\u{E000}",
    folder = "\u{E001}",
    bookmark = "\u{E002}",
    plus = "\u{E003}",
    arrow = "\u{E004}",
}

local chapters = {
    { title = "Phan 1: Gap go va dinh uoc", page = 1 },
    { title = "Phan 2: Gia bien", page = 2 },
    { title = "Phan 3: Luu lac", page = 3 },
}

local function touch_up(event)
    return event.type == 4
end

local function draw_status(title)
    canvas.rectangle(0, 0, 540, 64, 14)
    canvas.rectangle(0, 63, 540, 1, 8)
    if settings.show_clock then
        canvas.text(18, 20, "10:27", 2, 0)
    end
    canvas.text(190, 20, title, 2, 0)
    canvas.text(456, 20, "95%", 1, 0)
end

local function draw_bottom(left, center, right)
    canvas.rectangle(0, 870, 540, 90, 13)
    canvas.rectangle(0, 870, 540, 1, 8)
    canvas.rectangle(180, 870, 1, 90, 8)
    canvas.rectangle(360, 870, 1, 90, 8)
    canvas.text(28, 908, left, 1, 0)
    canvas.text(208, 908, center, 1, 0)
    canvas.text(388, 908, right, 1, 0)
end

local function draw_splash()
    canvas.clear(15)
    canvas.rectangle(210, 220, 120, 120, 2)
    canvas.rectangle(238, 248, 64, 64, 15)
    canvas.text(126, 400, "X-READER", 4, 0)
    canvas.text(158, 458, "EPUB READER FOR ESP32", 1, 4)
    canvas.text(218, 700, "LOADING", 1, 0)
    canvas.rectangle(90, 740, 360, 12, 10)
    canvas.rectangle(90, 740, 220, 12, 4)
end

local function draw_library()
    draw_status("LIBRARY")
    canvas.text(24, 96, "SD CARD", 2, 0)
    canvas.text(400, 96, "BOOKS", 1, 5)
    for index, book in ipairs(bookshelf.books) do
        local y = 150 + (index - 1) * 130
        local shade = index == selected and 3 or 14
        canvas.rectangle(20, y, 500, 106, shade)
        canvas.text(46, y + 38, icons.book, 4, index == selected and 15 or 0)
        canvas.text(126, y + 18, book.title, 2, index == selected and 15 or 0)
        canvas.text(126, y + 54, book.author, 1, index == selected and 15 or 0)
        canvas.text(
            126,
            y + 76,
            "EPUB  " .. tostring(book.pages) .. " P",
            1,
            index == selected and 15 or 0
        )
    end
    draw_bottom("OPEN", "DELETE", "SETTINGS")
end

local function draw_files()
    draw_status("SD CARD  >  BOOKS")
    canvas.text(26, 98, icons.arrow .. "  .. (BACK)", 2, 0)
    local folders = { "VIETNAMESE", "ENGLISH", "COMICS" }
    for index, folder in ipairs(folders) do
        local y = 160 + (index - 1) * 80
        canvas.text(26, y - 2, icons.folder, 2, 0)
        canvas.text(76, y, folder, 2, 0)
        canvas.text(76, y + 30, tostring(index * 4) .. " ITEMS", 1, 6)
    end
    local y = 410
    for index, book in ipairs(bookshelf.books) do
        canvas.text(28, y + 4, icons.book, 2, index == selected and 2 or 8)
        canvas.text(76, y + 4, book.title .. ".EPUB", 1, 0)
        canvas.text(76, y + 26, tostring(book.pages) .. " MB", 1, 6)
        y = y + 70
    end
    draw_bottom("OPEN", "", "OPTIONS")
end

local function draw_book_info()
    draw_status("BOOK INFORMATION")
    canvas.rectangle(24, 112, 160, 220, 11)
    canvas.text(50, 144, "BOOK", 3, 0)
    canvas.text(210, 140, current_book.title, 2, 0)
    canvas.text(210, 180, current_book.author, 1, 0)
    canvas.text(210, 230, "FORMAT: EPUB", 1, 0)
    canvas.text(210, 260, "SIZE: 1.2 MB", 1, 0)
    canvas.text(210, 290, "LANGUAGE: ENGLISH", 1, 0)
    canvas.text(24, 382, "A sample publication prepared for the", 1, 0)
    canvas.text(24, 408, "Lua-controlled e-paper reader interface.", 1, 0)
    canvas.rectangle(24, 520, 492, 64, 3)
    canvas.text(164, 544, "OPEN BOOK", 2, 15)
    draw_bottom("BACK", "", "OPEN")
end

local function draw_reader()
    draw_status(current_book.title)
    canvas.rectangle(20, 86, 500, 770, 14)
    canvas.rectangle(28, 94, 484, 754, 15)
    canvas.text(44, 118, "CHAPTER ONE", 2, 5)
    local scale = settings.font_scale()
    local line_limit = math.floor((760 - 166) / (30 * scale))
    for index, line in ipairs(reader.page_lines(scale, line_limit)) do
        canvas.text(44, 166 + (index - 1) * 30 * scale, line, scale, 0)
    end
    canvas.rectangle(44, 768, 452, 2, 8)
    canvas.rectangle(44, 786, 452, 10, 11)
    canvas.rectangle(44, 786, reader.progress_width(452), 10, 4)
    canvas.text(44, 812, reader.is_bookmarked() and "BOOKMARKED" or "NO BOOKMARK", 1, 5)
    if settings.show_page_number then
        canvas.text(240, 812, tostring(reader.page) .. " / 3", 2, 0)
    end
    draw_bottom("MENU", "PREV", "NEXT")
end

local function draw_menu()
    draw_status("READER MENU")
    local entries = {
        "TABLE OF CONTENTS",
        "BOOKMARKS",
        "DISPLAY SETTINGS",
        "READING SETTINGS",
        "BOOK INFORMATION",
        "EXIT TO LIBRARY",
    }
    for index, entry in ipairs(entries) do
        local y = 100 + (index - 1) * 92
        local shade = index == menu_selected and 3 or 14
        canvas.rectangle(20, y, 500, 70, shade)
        canvas.text(48, y + 24, entry, 1, index == menu_selected and 15 or 0)
        canvas.text(484, y + 24, ">", 2, index == menu_selected and 15 or 0)
    end
    draw_bottom("BACK", "", "SELECT")
end

local function draw_toc()
    draw_status("TABLE OF CONTENTS")
    for index, chapter in ipairs(chapters) do
        local y = 110 + (index - 1) * 92
        canvas.rectangle(20, y, 500, 70, index == selected and 3 or 14)
        canvas.text(44, y + 24, chapter.title, 1, index == selected and 15 or 0)
        canvas.text(470, y + 24, tostring(chapter.page), 1, index == selected and 15 or 0)
    end
    draw_bottom("BACK", "", "OPEN")
end

local function draw_bookmarks()
    draw_status("BOOKMARKS")
    local y = 112
    for page = 1, 3 do
        if reader.bookmarks[page] then
            canvas.rectangle(20, y, 500, 74, 14)
            canvas.text(48, y + 24, "BOOKMARK PAGE " .. tostring(page), 1, 0)
            canvas.text(460, y + 24, tostring(page), 1, 0)
            y = y + 92
        end
    end
    if y == 112 then
        canvas.text(42, 150, "NO BOOKMARKS YET", 2, 5)
    end
    draw_bottom("BACK", "", "OPEN")
end

local function draw_display_settings()
    draw_status("DISPLAY SETTINGS")
    canvas.text(24, 116, "BRIGHTNESS", 2, 0)
    canvas.text(420, 116, tostring(settings.brightness) .. "%", 1, 0)
    canvas.rectangle(24, 150, 490, 2, 8)
    canvas.text(24, 212, "REFRESH MODE", 2, 0)
    canvas.text(380, 212, settings.partial_refresh and "FAST" or "FULL", 1, 0)
    canvas.text(24, 308, "SHOW PAGE NUMBER", 2, 0)
    canvas.text(450, 308, settings.show_page_number and "ON" or "OFF", 1, 0)
    canvas.text(24, 404, "SHOW CLOCK", 2, 0)
    canvas.text(450, 404, settings.show_clock and "ON" or "OFF", 1, 0)
    draw_bottom("BACK", "", "TOGGLE")
end

local function draw_reading_settings()
    draw_status("READING SETTINGS")
    canvas.text(24, 116, "FONT SIZE", 2, 0)
    canvas.text(
        430,
        116,
        settings.font_size == "small" and "A" or settings.font_size == "large" and "A A A" or "A A",
        1,
        0
    )
    canvas.text(24, 212, "LINE SPACING", 2, 0)
    canvas.text(430, 212, "NORMAL", 1, 0)
    canvas.text(24, 308, "PAGE TURN AREA", 2, 0)
    canvas.text(350, 308, "LEFT / RIGHT", 1, 0)
    draw_bottom("BACK", "", "TOGGLE")
end

return {
    start = function(snapshot)
        splash_until = system.monotonic_ms() + 1200
        if snapshot ~= nil and snapshot.screen ~= nil and snapshot.screen ~= "splash" then
            screen = snapshot.screen
        end
    end,

    event = function(event)
        if not touch_up(event) then
            return
        end
        if screen == "splash" then
            screen = "library"
        elseif screen == "library" then
            if event.y >= 870 and event.x >= 360 then
                screen = "display_settings"
            elseif event.y >= 150 and event.y < 410 then
                selected =
                    math.max(1, math.min(#bookshelf.books, math.floor((event.y - 150) / 130) + 1))
                current_book = bookshelf.books[selected]
                screen = "book_info"
            elseif event.y >= 870 and event.x < 180 then
                screen = "files"
            end
        elseif screen == "files" then
            if event.y >= 870 and event.x < 180 then
                screen = "book_info"
            elseif event.y < 140 then
                screen = "library"
            end
        elseif screen == "book_info" then
            if event.y >= 500 then
                reader.open(current_book)
                screen = "reader"
            elseif event.y < 82 then
                screen = "library"
            end
        elseif screen == "reader" then
            if event.y >= 870 and event.x < 180 then
                screen = "menu"
            elseif event.y >= 870 and event.x < 360 then
                reader.previous_page()
            elseif event.y >= 870 then
                reader.next_page()
            elseif event.y >= 360 and event.y < 450 then
                reader.toggle_bookmark()
            elseif event.y < 82 then
                screen = "menu"
            end
        elseif screen == "menu" then
            if event.y >= 870 then
                screen = "reader"
            elseif event.y >= 100 and event.y < 650 then
                menu_selected = math.max(1, math.min(6, math.floor((event.y - 100) / 92) + 1))
                if menu_selected == 1 then
                    screen = "toc"
                elseif menu_selected == 2 then
                    screen = "bookmarks"
                elseif menu_selected == 3 then
                    screen = "display_settings"
                elseif menu_selected == 4 then
                    screen = "reading_settings"
                elseif menu_selected == 5 then
                    screen = "book_info"
                else
                    screen = "library"
                end
            end
        elseif screen == "toc" then
            if event.y >= 870 then
                screen = "menu"
            elseif event.y >= 110 and event.y < 390 then
                selected = math.max(1, math.min(#chapters, math.floor((event.y - 110) / 92) + 1))
                reader.go_to_page(chapters[selected].page)
                screen = "reader"
            end
        elseif screen == "bookmarks" then
            if event.y >= 870 then
                screen = "menu"
            end
        elseif screen == "display_settings" then
            if event.y >= 870 then
                screen = "menu"
            elseif event.y >= 80 and event.y < 180 then
                settings.toggle_brightness()
            elseif event.y >= 180 and event.y < 280 then
                settings.toggle_refresh()
            elseif event.y >= 280 and event.y < 380 then
                settings.toggle_page_number()
            elseif event.y >= 380 and event.y < 480 then
                settings.toggle_clock()
            end
        elseif screen == "reading_settings" then
            if event.y >= 870 then
                screen = "menu"
            elseif event.y >= 80 and event.y < 180 then
                settings.toggle_text_scale()
            end
        end
    end,

    update = function(now)
        if screen == "splash" and now >= splash_until then
            screen = "library"
        end
    end,

    draw = function(_)
        if screen == "splash" then
            draw_splash()
        elseif screen == "library" then
            canvas.clear(15)
            draw_library()
        elseif screen == "files" then
            canvas.clear(15)
            draw_files()
        elseif screen == "book_info" then
            canvas.clear(15)
            draw_book_info()
        elseif screen == "reader" then
            canvas.clear(15)
            draw_reader()
        elseif screen == "menu" then
            canvas.clear(15)
            draw_menu()
        elseif screen == "toc" then
            canvas.clear(15)
            draw_toc()
        elseif screen == "bookmarks" then
            canvas.clear(15)
            draw_bookmarks()
        elseif screen == "display_settings" then
            canvas.clear(15)
            draw_display_settings()
        elseif screen == "reading_settings" then
            canvas.clear(15)
            draw_reading_settings()
        end
        display.present(settings.partial_refresh)
    end,

    snapshot = function()
        return { screen = screen }
    end,

    stop = function(_) end,
}
