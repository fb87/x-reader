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

    -- Captures just the handful of scalar fields each scene declares via
    -- `scene.persist_fields` (highlight, scroll/page, focus zone, dock
    -- selection, ...) -- not the whole stage, which also holds widget
    -- instances (List/Dialog/ActionBar) that aren't meant to survive a
    -- restart and rebuild themselves from these scalars on resume anyway.
    function context.navigation_snapshot()
        local snapshot_stack = {}
        for index, id in ipairs(stack) do
            snapshot_stack[index] = id
        end
        local snapshot_stages = {}
        for _, id in ipairs(snapshot_stack) do
            local scene_fields = scenes[id].persist_fields
            local stage = stages[id]
            if scene_fields ~= nil and stage ~= nil then
                local values = {}
                for _, field in ipairs(scene_fields) do
                    values[field.key] = stage[field.key]
                end
                snapshot_stages[id] = values
            end
        end
        return { stack = snapshot_stack, stages = snapshot_stages }
    end

    -- Replays a persisted stack (oldest first) exactly as `navigation_snapshot`
    -- captured it: recreate each stage, reapply its persisted scalars, then
    -- let the scene re-derive anything else (e.g. file_manager's row list
    -- from its persisted `path`, reading's open document from the book
    -- `persistence.load()` already restored) via an optional `scene.restore`.
    function context.restore_navigation(navigation)
        if navigation == nil or navigation.stack == nil or #navigation.stack == 0 then
            return false
        end
        local restored_stack = {}
        for _, id in ipairs(navigation.stack) do
            if scenes[id] == nil then
                return false
            end
            restored_stack[#restored_stack + 1] = id
        end
        stack = restored_stack
        for _, id in ipairs(stack) do
            local stage = stage_for(id)
            local raw = navigation.stages[id]
            local scene_fields = scenes[id].persist_fields
            if raw ~= nil and scene_fields ~= nil then
                for _, field in ipairs(scene_fields) do
                    local value = raw[field.key]
                    if value ~= nil then
                        -- A garbled numeric field (e.g. a truncated/corrupt
                        -- record that still happened to pass the CRC check)
                        -- makes tonumber() fail; leave the scene's own
                        -- create()-time default in place rather than
                        -- overwriting it with a non-number, which would
                        -- crash the first time the scene does arithmetic on
                        -- it. (Not `kind == "number" and tonumber(value) or
                        -- value` -- that idiom falls through to `value` when
                        -- tonumber returns nil, silently keeping the raw
                        -- string instead of rejecting it.)
                        local coerced
                        if field.kind == "number" then
                            coerced = tonumber(value)
                        else
                            coerced = value
                        end
                        if coerced ~= nil then
                            stage[field.key] = coerced
                        end
                    end
                end
            end
            if scenes[id].restore then
                scenes[id].restore(stage, context)
            end
        end
        return true
    end
    persistence.set_navigation_provider(context.navigation_snapshot)

    local application = {}

    local function configure_layout()
        local width, height = display.size()
        layout.configure(width, height)
    end

    local function prepare_chrome(scene, stage, regions)
        if stage.status_bar == nil then
            stage.status_bar = StatusBar({ title = scene.id })
        end
        if stage.action_bar == nil then
            stage.action_bar = ActionBar({ actions = {} })
        end
        stage.status_bar:layout(regions.status)
        stage.status_bar.title = scene.title and scene.title(stage) or scene.id
        stage.action_bar:layout(regions.dock)
        stage.action_bar.actions = scene.dock(stage)
        stage.action_bar.selected = stage.dock_selected or 1
    end

    -- Settings calls this when the user flips Display > Orientation. The
    -- platform may resize a window (simulator) or rotate pixels into a
    -- fixed panel (ESP firmware) -- either way, layout.lua just needs the
    -- resulting logical width/height, recomputed fresh everywhere else.
    function application.set_orientation(landscape)
        local width, height = display.set_orientation(landscape)
        layout.configure(width, height)
    end
    context.set_orientation = application.set_orientation

    function application.start(snapshot)
        configure_layout()
        persistence.load()
        if settings.orientation == "landscape" then
            application.set_orientation(true)
        end
        last_activity = system.monotonic_ms()
        if snapshot and snapshot.screen and scenes[snapshot.screen] then
            context.replace(snapshot.screen)
        elseif not context.restore_navigation(persistence.navigation) then
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
            and layout.contains(layout.regions().dock, event.x, event.y)
        then
            local dock = layout.regions().dock
            local actions = scene.dock(stage)
            stage.focus_scope = "dock"
            stage.dock_selected = layout.slot_index(dock, event.x, event.y, #actions)
                or stage.dock_selected
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
        local regions = layout.regions()
        canvas.clear(theme.color.background)
        if scene.chrome == "standard" then
            prepare_chrome(scene, stage, regions)
            scene.draw(stage, regions.content)
            if stage.dialog and stage.dialog.visible then
                stage.dialog:draw()
            end
            -- Status bar and dock are drawn last and always repaint their own
            -- background so content never bleeds through and the panel rows
            -- they cover get freshly redriven on every frame.
            stage.status_bar:draw()
            stage.action_bar:draw(stage.focus_scope == "dock")
        else
            local full_bounds
            if stage.dock_visible then
                full_bounds = regions.content
            elseif scene.status then
                full_bounds = layout.below_status()
            else
                full_bounds = layout.fullscreen()
            end
            scene.draw(stage, full_bounds)
            if stage.dialog and stage.dialog.visible then
                stage.dialog:draw()
            end
            if scene.status then
                chrome.status(
                    regions.status,
                    scene.title and scene.title(stage) or scene.id,
                    scene.status_title
                )
            end
            if stage.dock_visible then
                if stage.action_bar then
                    stage.action_bar:layout(regions.dock)
                    stage.action_bar.selected = stage.dock_selected or 1
                    stage.action_bar:draw()
                else
                    chrome.dock(
                        regions.dock,
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

    return application
end

return { create = create_application }
