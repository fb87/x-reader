local state = assert(_G.shell, "native shell module is unavailable").state

local M = {}
local routes = {}
local history = {}
local current = nil

local function split_uri(uri)
    local path, query = uri:match("^([^?]*)%??(.*)$")
    local q = {}
    if query and query ~= "" then
        for item in query:gmatch("[^&]+") do
            local k, v = item:match("^([^=]+)=?(.*)$")
            if k then q[k] = v end
        end
    end
    return path ~= "" and path or "/", q
end

local function segments(path)
    local out = {}
    for part in path:gmatch("[^/]+") do out[#out + 1] = part end
    return out
end

local function match(pattern, path)
    if pattern == "/" and path == "/" then return {} end
    local a, b = segments(pattern), segments(path)
    if #a ~= #b then return nil end
    local params = {}
    for i = 1, #a do
        if a[i]:sub(1, 1) == ":" then params[a[i]:sub(2)] = b[i]
        elseif a[i] ~= b[i] then return nil end
    end
    return params
end

function M.register(def)
    assert(type(def) == "table" and type(def.path) == "string", "route.path is required")
    assert(type(def.page) == "table", "route.page module is required")
    for _, route in ipairs(routes) do
        assert(route.path ~= def.path, "duplicate route: " .. def.path)
    end
    routes[#routes + 1] = def
    return def
end

function M.resolve(uri)
    local path, query = split_uri(uri)
    for _, route in ipairs(routes) do
        local params = match(route.path, path)
        if params then
            return { uri = uri, path = path, params = params, query = query, route = route,
                     page = route.page }
        end
    end
    return nil
end

local function activate(uri, remember)
    local found = M.resolve(uri)
    if not found then return false end
    if remember and current then history[#history + 1] = current.uri end
    current = found
    state.set("app.route.current", uri)
    state.set("app.menu.selected", 0)
    state.set("app.focus.area", 0)
    state.set("app.dock.selected", 0)
    state.set("app.dialog.book_info", false)
    return true
end

function M.push(uri) return activate(uri, true) end
function M.replace(uri) return activate(uri, false) end
function M.back()
    if #history == 0 then return false end
    local uri = table.remove(history)
    return activate(uri, false)
end
function M.current() return current end
function M.routes() return routes end
function M.menu(section)
    local out = {}
    for _, route in ipairs(routes) do
        if route.menu and route.menu.section == section then out[#out + 1] = route end
    end
    table.sort(out, function(a, b) return (a.menu.order or 0) < (b.menu.order or 0) end)
    return out
end
function M.restore(default_uri)
    history = {}
    local uri = state.get("app.route.current") or default_uri or "/"
    if not activate(uri, false) then return activate(default_uri or "/", false) end
    return true
end
function M.clear()
    routes = {}
    history = {}
    current = nil
end

return M
