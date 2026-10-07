local shell = require("shell")
local state = shell.state
local router = shell.router
local M = {}

-- Compatibility IDs are retained for state/tests while navigation is URI based.
M.page = {
    splash = 0, home = 1, library = 2, favorites = 3, files = 4,
    reader = 5, settings = 6, connectivity = 7, sleep = 8,
}
M.route = {
    [M.page.splash] = "/splash",
    [M.page.home] = "/",
    [M.page.library] = "/library",
    [M.page.favorites] = "/library/favorites",
    [M.page.files] = "/files",
    [M.page.reader] = "/book/0/reader",
    [M.page.settings] = "/settings",
    [M.page.connectivity] = "/settings/network",
    [M.page.sleep] = "/sleep",
}

function M.current_page()
    return state.get("app.page.current") or M.page.splash
end

local function page_for_uri(uri)
    if uri == "/splash" then return M.page.splash end
    if uri == "/" then return M.page.home end
    if uri == "/library" then return M.page.library end
    if uri == "/library/favorites" then return M.page.favorites end
    if uri == "/files" then return M.page.files end
    if uri:match("^/book/[^/]+/reader") then return M.page.reader end
    if uri == "/settings" then return M.page.settings end
    if uri == "/settings/network" then return M.page.connectivity end
    if uri == "/sleep" then return M.page.sleep end
    return M.page.home
end

function M.push(uri)
    if router.push(uri) then state.set("app.page.current", page_for_uri(uri)); return true end
    return false
end
function M.replace(uri)
    if router.replace(uri) then state.set("app.page.current", page_for_uri(uri)); return true end
    return false
end
function M.set_page(value)
    local previous = M.current_page()
    local ok = M.push(M.route[value] or "/")
    if ok and value == M.page.home then
        state.set("app.menu.selected", 0)
        state.set("app.home.card.focused", previous == M.page.splash)
    end
    return ok
end
function M.back()
    if router.back() then
        local cur = router.current()
        state.set("app.page.current", page_for_uri(cur and cur.path or "/"))
        return true
    end
    return M.replace("/")
end

function M.move(key, delta, count)
    local value = (state.get(key) or 0) + delta
    if value < 0 then value = 0 end
    if count > 0 and value >= count then value = count - 1 end
    state.set(key, value)
    return value
end

return M
