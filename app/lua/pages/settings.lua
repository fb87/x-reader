local shell = require("shell")
local widget = shell.widget
local dialog = shell.dialog
local state = shell.state
local i18n = shell.i18n
local model = require("model")
local M = { id = model.page.settings }

local function base_labels()
    return { i18n.t("bluetooth"), i18n.t("font_size"), i18n.t("full_refresh"),
             i18n.t("progress_bar"), i18n.t("sleep_timeout"), i18n.t("language"), i18n.t("about") }
end

local function base_values()
    local fonts = { i18n.t("small"), i18n.t("medium"), i18n.t("large") }
    local full = state.get("reader.settings.full_refresh_every") or 6
    return {
        state.get("network.bluetooth.enabled") and i18n.t("on") or i18n.t("off"),
        fonts[(state.get("reader.settings.font_size") or 1) + 1],
        full == 0 and i18n.t("never") or i18n.t("every", full),
        state.get("reader.settings.show_progress") == false and i18n.t("off") or i18n.t("on"),
        i18n.t("minutes", state.get("reader.settings.sleep_timeout_minutes") or 10),
        i18n.language_name(),
        i18n.t("software_reader"),
    }
end

local function plugin_routes() return shell.router.menu("settings") end

function M.render()
    shell.api.clear(widget.gray.white)
    widget.status(i18n.t("settings"))
    local selected = state.get("app.menu.selected") or 0
    local y = 58
    local index = 0
    for _, route in ipairs(plugin_routes()) do
        local value = route.menu and route.menu.value and route.menu.value() or ""
        widget.row { y = y, primary = i18n.t(route.title), secondary = value, selected = selected == index }
        y = y + 76; index = index + 1
    end
    local names, values = base_labels(), base_values()
    for i, label in ipairs(names) do
        widget.row { y = y, primary = label, secondary = values[i], selected = selected == index }
        y = y + 76; index = index + 1
    end
    widget.dock { i18n.t("back") }
    if state.get("app.dialog.about") then
        dialog.render { title = i18n.t("about"), body = i18n.t("software_reader"),
                        buttons = { i18n.t("close") }, selected = 0 }
    end
end

local function activate_base(selected)
    if selected == 0 then state.set("network.bluetooth.enabled", not (state.get("network.bluetooth.enabled") == true))
    elseif selected == 1 then shell.reader.adjust_font(1)
    elseif selected == 2 then
        local choices = { 1, 3, 6, 10, 0 }
        local current = state.get("reader.settings.full_refresh_every") or 6
        local at = 1
        for i, value in ipairs(choices) do if value == current then at = i end end
        state.set("reader.settings.full_refresh_every", choices[at % #choices + 1])
    elseif selected == 3 then state.set("reader.settings.show_progress", state.get("reader.settings.show_progress") == false)
    elseif selected == 4 then
        local value = state.get("reader.settings.sleep_timeout_minutes") or 10
        state.set("reader.settings.sleep_timeout_minutes", value == 10 and 30 or (value == 30 and 60 or 10))
    elseif selected == 5 then i18n.cycle()
    else state.set("app.dialog.about", true) end
end

local function activate(selected)
    local plugins = plugin_routes()
    if selected < #plugins then return model.push(plugins[selected + 1].path) end
    activate_base(selected - #plugins)
    return true
end

function M.event(ev)
    if state.get("app.dialog.about") then
        if ev.type == "key" or ev.type == "tap" then state.set("app.dialog.about", false); return true end
    end
    local count = #plugin_routes() + #base_labels()
    if ev.type == "tap" and ev.y >= 58 and ev.y < 58 + count * 76 then
        local selected = math.floor((ev.y - 58) / 76)
        state.set("app.menu.selected", selected); return activate(selected)
    end
    if ev.type ~= "key" then return false end
    if ev.key == "back" then return model.set_page(model.page.home) end
    if ev.key == "up" then model.move("app.menu.selected", -1, count); return true end
    if ev.key == "down" then model.move("app.menu.selected", 1, count); return true end
    if ev.key == "ok" then return activate(state.get("app.menu.selected") or 0) end
    return false
end
return M
