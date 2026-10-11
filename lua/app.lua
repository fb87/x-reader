local chrome = require("chrome")
local input = require("input")
local layout = require("layout")
local ActionBar = require("ui.action_bar")
local StatusBar = require("ui.status_bar")
local theme = require("theme")
local persistence = require("persistence")
local settings = require("settings")

local scenes = {
    loader = require("scenes.loader"),
    launcher = require("scenes.launcher"),
    library = require("scenes.library"),
    file_manager = require("scenes.file_manager"),
    settings = require("scenes.settings"),
    reading = require("scenes.reading"),
    sleep = require("scenes.sleep"),
}

local function create_application()
    local stages = {}
    local stack = { "loader" }
    local context = {}
    local held_keys = {}
    local last_activity = 0

    local function activate_key(scene, stage, key, kind)
        if stage.dialog and stage.dialog.visible then
            if kind == "long" then
                return
            elseif key == "up" or key == "down" then
                stage.dialog:move(key == "up" and -1 or 1)
            elseif key == "push" then
                stage.dialog:activate()
            end
            return
        end
        if kind == "long" then
            if key == "push" then
                if scene.id == "sleep" then
                    context.pop()
                else
                    context.push("sleep")
                end
            elseif key == "up" or key == "down" then
                stage.focus_scope = stage.focus_scope == "dock" and "content" or "dock"
                if scene.id == "reading" then
                    stage.dock_visible = stage.focus_scope == "dock"
                end
            end
            return
        end
        if key == "up" or key == "down" then
            local direction = key == "up" and -1 or 1
            if stage.focus_scope == "dock" then
                local count = #scene.dock(stage)
                stage.dock_selected = ((stage.dock_selected - 1 + direction) % count) + 1
            elseif scene.move then
                scene.move(stage, direction)
            end
        elseif key == "push" then
            if stage.focus_scope == "dock" and scene.activate_dock then
                scene.activate_dock(stage, context)
            elseif stage.focus_scope ~= "dock" and scene.activate_content then
                scene.activate_content(stage, context)
            end
        end
    end

    local function stage_for(id)
        if stages[id] == nil then
            stages[id] = scenes[id].create(context)
        end
        return stages[id]
    end

    local function active_id()
        return stack[#stack]
    end

    function context.push(id, params)
        for index, value in ipairs(stack) do
            if value == id then
                while #stack > index do
                    table.remove(stack)
                end
                scenes[id].resume(stage_for(id), context)
                return
            end
        end
        if scenes[active_id()].pause then
            scenes[active_id()].pause(stage_for(active_id()))
        end
        stack[#stack + 1] = id
        if scenes[id].show then
            scenes[id].show(stage_for(id), params, "push")
        end
    end

    function context.pop()
        if #stack <= 1 then
            if active_id() ~= "launcher" then
                context.replace("launcher")
            end
            return
        end
        local leaving = table.remove(stack)
        if scenes[leaving].pause then
            scenes[leaving].pause(stage_for(leaving))
        end
        if scenes[active_id()].resume then
            scenes[active_id()].resume(stage_for(active_id()), context)
        end
    end

    function context.replace(id, params)
        local leaving = table.remove(stack)
        if leaving and scenes[leaving].pause then
            scenes[leaving].pause(stage_for(leaving))
        end
        stack[#stack + 1] = id
        if scenes[id].show then
            scenes[id].show(stage_for(id), params, "replace")
        end
    end

    function context.active()
        local id = active_id()
        return scenes[id], stage_for(id)
    end

    local application = {}

    local function configure_layout()
        local width, height = display.size()
        layout.configure(width, height)
        chrome.content = layout.standard().content
    end

    local function prepare_chrome(scene, stage)
        if stage.status_bar == nil then
            stage.status_bar = StatusBar({ title = scene.id })
        end
        if stage.action_bar == nil then
            stage.action_bar = ActionBar({ actions = {} })
        end
        stage.status_bar.title = scene.title and scene.title(stage) or scene.id
        stage.action_bar.actions = scene.dock(stage)
        stage.action_bar.selected = stage.dock_selected or 1
    end

    function application.start(snapshot)
        configure_layout()
        persistence.load()
        last_activity = system.monotonic_ms()
        if snapshot and snapshot.screen and scenes[snapshot.screen] then
            context.replace(snapshot.screen)
        else
            if scenes.loader.show then
                scenes.loader.show(stage_for("loader"), nil, "start")
            end
        end
    end

    function application.event(raw_event)
        local event = input.normalize(raw_event)
        if event.kind ~= "none" then
            last_activity = system.monotonic_ms()
        end
        local scene, stage = context.active()
        if event.kind == "key" then
            if event.phase == "down" then
                if held_keys[event.key] == nil then
                    held_keys[event.key] = { started = event.time or 0, long = false }
                end
            elseif event.phase == "up" then
                local held = held_keys[event.key]
                if held ~= nil and not held.long then
                    activate_key(scene, stage, event.key, "press")
                end
                held_keys[event.key] = nil
            end
        elseif
            event.kind == "touch"
            and event.phase == "up"
            and (scene.chrome == "standard" or stage.dock_visible)
            and event.y >= layout.standard().dock.y
        then
            local actions = scene.dock(stage)
            stage.focus_scope = "dock"
            stage.dock_selected =
                math.max(1, math.min(#actions, math.floor(event.x / (540 / #actions)) + 1))
            if scene.activate_dock then
                scene.activate_dock(stage, context)
            end
        else
            scene.event(stage, event, context)
        end
    end

    function application.update(now)
        local scene, stage = context.active()
        local redraw_needed = false
        if
            scene.id ~= "sleep"
            and scene.id ~= "loader"
            and now - last_activity >= settings.auto_sleep_minutes * 60 * 1000
        then
            last_activity = now
            context.push("sleep")
            redraw_needed = true
        end
        scene, stage = context.active()
        for key, held in pairs(held_keys) do
            if not held.long and now - held.started >= 700 then
                held.long = true
                activate_key(scene, stage, key, "long")
                redraw_needed = true
            end
        end
        if scene.update and scene.update(stage, now, context) then
            redraw_needed = true
        end
        return redraw_needed
    end

    function application.draw()
        local scene, stage = context.active()
        canvas.clear(theme.color.background)
        if scene.chrome == "standard" then
            prepare_chrome(scene, stage)
            scene.draw(stage, chrome.content)
            if stage.dialog and stage.dialog.visible then
                stage.dialog:draw()
            end
            -- Status bar and dock are drawn last and always repaint their own
            -- background so content never bleeds through and the panel rows
            -- they cover get freshly redriven on every frame.
            stage.status_bar:draw()
            stage.action_bar:draw(stage.focus_scope == "dock")
        else
            if stage.dock_visible then
                scene.draw(stage, chrome.content)
            else
                scene.draw(stage, { x = 0, y = 0, width = 540, height = 960 })
            end
            if stage.dialog and stage.dialog.visible then
                stage.dialog:draw()
            end
            if scene.status then
                chrome.status(scene.title and scene.title(stage) or scene.id, scene.status_title)
            end
            if stage.dock_visible then
                if stage.action_bar then
                    stage.action_bar.selected = stage.dock_selected or 1
                    stage.action_bar:draw()
                else
                    chrome.dock(
                        scene.dock(stage),
                        stage.dock_selected or 1,
                        stage.focus_scope == "dock"
                    )
                end
            end
        end
        local dialog_visible = stage.dialog and stage.dialog.visible
        display.present(false, dialog_visible == true)
    end

    function application.snapshot()
        persistence.save()
        local scene = context.active()
        return { screen = scene.id }
    end

    function application.stop()
        persistence.save()
    end

    function application.stop(_) end

    return application
end

return { create = create_application }
