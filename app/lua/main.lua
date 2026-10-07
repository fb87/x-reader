local shell = require("shell")
local model = require("model")
local routes = require("routes")
local plugins = require("enabled_plugins")

local M = {}

local function active_page()
    local current = shell.router.current()
    return current and current.page or nil, current
end

function M.init(reload)
    routes.register()
    if not plugins.init() then return false end
    if not reload then
        shell.state.set("app.page.current", model.page.splash)
        shell.state.set("app.route.current", "/splash")
        shell.state.set("app.menu.selected", 0)
        shell.state.set("app.home.card.focused", true)
        shell.state.set("app.focus.area", 0)
        shell.state.set("app.dock.selected", 0)
        shell.state.set("app.reader.chrome", false)
        shell.state.set("app.dialog.book_info", false)
        shell.state.set("app.dialog.about", false)
    end
    shell.router.restore(reload and (shell.state.get("app.route.current") or "/") or "/splash")
    if model.current_page() == model.page.home then shell.state.set("app.menu.selected", -1) end
    return true
end

function M.render()
    local page, request = active_page()
    if not page then return false end
    page.render(request)
    shell.api.present()
    return true
end

function M.event(ev)
    local page, request = active_page()
    return page and page.event and page.event(ev, request) or false
end

function M.tick(now)
    local page, request = active_page()
    if page and page.tick then page.tick(now, request) end
end

return M
