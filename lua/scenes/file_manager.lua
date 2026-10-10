local icons = require("icons")
local layout = require("layout")
local List = require("ui.list")
local scene = { id = "file_manager", chrome = "standard" }
function scene.title(stage)
    if stage.path == "" then
        return "SD Card"
    end
    return "SD Card > " .. (stage.path:match("([^/]+)$") or stage.path)
end
local function rows_for(path)
    local rows = {}
    local entries = library.list(path) or {}
    for _, entry in ipairs(entries) do
        rows[#rows + 1] = {
            id = entry.path,
            title = entry.name,
            subtitle = entry.directory and "Folder" or tostring(entry.size) .. " bytes",
            icon = entry.directory and icons.folder or icons.book,
        }
    end
    return rows
end
local function parent_path(path)
    local parent, name = path:match("^(.*)/([^/]+)$")
    return parent or "", name or path
end

local function enter_path(stage, path, selected_name)
    stage.path = path
    stage.list.rows = rows_for(path)
    stage.selected = 1
    if selected_name ~= nil then
        for index, row in ipairs(stage.list.rows) do
            if row.title == selected_name then
                stage.selected = index
                break
            end
        end
    end
    stage.list.selected_id = stage.list.rows[1] and stage.list.rows[1].id
    if stage.list.rows[stage.selected] then
        stage.list.selected_id = stage.list.rows[stage.selected].id
    end
end
function scene.create()
    local rows = rows_for("")
    return {
        selected = 1,
        dock_selected = 1,
        path = "",
        focus_scope = "content",
        list = List({ rows = rows, selected_id = rows[1] and rows[1].id, row_height = 84 }),
    }
end
function scene.dock()
    return {
        { label = "Open", icon = icons.book },
        { label = "Up", icon = icons.arrow },
        { label = "Back", icon = icons.arrow },
    }
end
function scene.move(stage, direction)
    if #stage.list.rows == 0 then
        return
    end
    stage.selected = math.max(1, math.min(#stage.list.rows, stage.selected + direction))
    stage.list.selected_id = stage.list.rows[stage.selected].id
    stage.list.page = math.floor((stage.selected - 1) / (stage.list.page_size or 1)) + 1
end
function scene.open_selected(stage, context)
    stage.focus_scope = "content"
    local row = stage.list.rows[stage.selected]
    if row == nil then
        return
    end
    if row.icon == icons.folder then
        enter_path(stage, row.id)
    elseif row.title:lower():sub(-5) == ".epub" then
        context.push("reading", {
            book = {
                id = row.id,
                path = row.id,
                title = row.title:sub(1, -6),
                author = "",
                pages = 0,
            },
        })
    end
end
function scene.activate_content(stage, context)
    scene.open_selected(stage, context)
end
function scene.activate_dock(stage, context)
    if stage.dock_selected == 1 then
        scene.open_selected(stage, context)
    elseif stage.dock_selected == 2 then
        local parent, selected_name = parent_path(stage.path)
        enter_path(stage, parent, selected_name)
    elseif stage.dock_selected == 3 then
        context.pop()
    end
end
function scene.event(stage, event, context)
    if event.kind == "touch" and event.phase == "up" and event.y >= layout.standard().dock.y then
        stage.dock_selected = math.floor(event.x / 180) + 1
        if stage.dock_selected == 3 then
            context.pop()
        end
    elseif event.kind == "touch" and event.phase == "up" and event.y >= 142 then
        local page_size = stage.list.page_size or 1
        local local_index = math.floor((event.y - 142) / stage.list.row_height) + 1
        stage.focus_scope = "content"
        stage.selected =
            math.max(1, math.min(#stage.list.rows, (stage.list.page - 1) * page_size + local_index))
        stage.list.selected_id = stage.list.rows[stage.selected]
            and stage.list.rows[stage.selected].id
        scene.open_selected(stage, context)
    end
end
function scene.draw(stage)
    local header = { x = 20, y = 70, width = 500, height = 64 }
    canvas.text(24, layout.text_y(header, icons.folder, 6), icons.folder, 6, 0)
    canvas.text(76, layout.text_y(header, scene.title(stage), 3), scene.title(stage), 3, 0)
    stage.list.active = stage.focus_scope == "content"
    stage.list:layout({ x = 20, y = 142, width = 500, height = layout.content_height(142, 24) })
    stage.list.page = math.floor(
        (stage.selected - 1)
            / math.max(1, math.floor(stage.list.bounds.height / stage.list.row_height))
    ) + 1
    stage.list:draw()
    canvas.text(
        220,
        layout.standard().dock.y - 28,
        "Page " .. tostring(stage.list.page) .. " / " .. tostring(stage.list.page_count),
        1,
        5
    )
end
return scene
