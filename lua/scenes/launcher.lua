local scene = { id = "launcher", chrome = "standard" }
scene.persist_fields = {
    { key = "selected", kind = "number" },
    { key = "dock_selected", kind = "number" },
    { key = "focus_scope", kind = "string" },
}
local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local reader = require("reader")
local items = { "Library", "Files", "Settings" }
local item_icons = { icons.library, icons.folder_open, icons.settings }
function scene.title()
    return "Home"
end
function scene.create()
    local rows = {}
    for index, label in ipairs(items) do
        rows[index] = { id = label:lower(), title = label, icon = item_icons[index] }
    end
    return {
        selected = 1,
        dock_selected = 1,
        focus_scope = "content",
        list = List({ rows = rows, selected_id = "library", row_height = 84 }),
    }
end
function scene.dock()
    return { { label = "Sleep", icon = icons.sleep }, { label = "Power", icon = icons.power } }
end
function scene.move(stage, direction)
    stage.selected = ((stage.selected + direction - 1) % (#items + 1)) + 1
    if stage.selected > 1 then
        stage.list.selected_id = items[stage.selected - 1]:lower()
        local page_size = stage.list.page_size or 1
        stage.list.page = math.floor((stage.selected - 2) / page_size) + 1
    end
    stage.list.active = stage.focus_scope == "content"
end
function scene.activate_content(stage, context)
    if stage.selected == 1 then
        if reader.last_book ~= nil then
            context.push("reading", { book = reader.last_book })
        else
            context.push("library")
        end
    elseif stage.selected == 2 then
        context.push("library")
    elseif stage.selected == 3 then
        context.push("file_manager")
    elseif stage.selected == 4 then
        context.push("settings")
    end
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 or stage.dock_selected == 2 then
        context.push("sleep")
    end
end
function scene.event(stage, event, context)
    if event.kind ~= "touch" or event.phase ~= "up" then
        return
    end
    if stage.card_bounds and layout.contains(stage.card_bounds, event.x, event.y) then
        stage.selected = 1
        stage.focus_scope = "content"
        scene.activate_content(stage, context)
    elseif layout.contains(stage.list.bounds, event.x, event.y) then
        local page_size = stage.list.page_size or 1
        local local_index = math.floor((event.y - stage.list.bounds.y) / layout.scale_y(84))
        local index = (stage.list.page - 1) * page_size + local_index + 2
        if index >= 2 and index <= #items + 1 then
            stage.focus_scope = "content"
            stage.selected = index
            scene.activate_content(stage, context)
        end
    end
end
function scene.draw(stage, bounds)
    local book = reader.last_book
    local card_selected = stage.selected == 1 and stage.focus_scope == "content"
    local regions = layout.column(bounds, { padding = 24, gap = 12 }, {
        { size = 60 },
        { size = 216 },
        { fill = true },
    })
    local header, card, list_area = regions[1], regions[2], regions[3]
    canvas.text(header.x, layout.text_y(header, "Home", 3), "Home", 3, 0)
    canvas.rectangle(card.x, card.y, card.width, card.height, card_selected and 12 or 14)
    stage.card_bounds = card
    if book ~= nil then
        local inner = layout.column(card, { padding = 24, gap = 6 }, {
            { size = 34 },
            { size = 34 },
            { size = 28 },
            { fill = true },
            { size = 20 },
            { size = 6 },
        })
        local label_box, title_box, author_box, page_box, bar_box =
            inner[1], inner[2], inner[3], inner[5], inner[6]
        canvas.text(
            label_box.x,
            layout.text_y(label_box, "Continue Reading", 3),
            "Continue Reading",
            3,
            0
        )
        local title = book.title or "Unknown title"
        canvas.text(title_box.x, layout.text_y(title_box, title, 2), title, 2, 0)
        local author = book.author ~= "" and book.author or "Unknown author"
        canvas.text(author_box.x, layout.text_y(author_box, author, 1), author, 1, 5)
        local page_label = string.format("%d / %d", reader.page, reader.page_count)
        local page_width = canvas.measure(page_label, 1)
        canvas.text(
            page_box.x + page_box.width - page_width,
            layout.text_y(page_box, page_label, 1),
            page_label,
            1,
            5
        )
        canvas.rectangle(bar_box.x, bar_box.y, bar_box.width, bar_box.height, 10)
        canvas.rectangle(
            bar_box.x,
            bar_box.y,
            reader.progress_width(bar_box.width),
            bar_box.height,
            5
        )
    else
        local inner = layout.column(
            card,
            { padding = 24, gap = 6 },
            { { size = 34 }, { fill = true } }
        )
        canvas.text(
            inner[1].x,
            layout.text_y(inner[1], "No book to resume", 2),
            "No book to resume",
            2,
            5
        )
    end
    stage.list.selected_id = stage.selected > 1 and items[stage.selected - 1]:lower() or nil
    stage.list.active = stage.focus_scope == "content" and stage.selected > 1
    stage.list:layout(list_area)
    stage.list:draw()
end
return scene
