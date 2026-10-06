local M = { registry = {} }

function M.new(spec)
    assert(type(spec) == "table")
    assert(spec.id ~= nil)
    return spec
end

function M.register(name, value)
    M.registry[name] = value
    return value
end

function M.get(name)
    return assert(M.registry[name], "unknown page: " .. tostring(name))
end

return M
