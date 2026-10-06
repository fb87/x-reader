local shell = assert(_G.shell)
local state = shell.state
local M = {}

local supported = {
  { code = "en", file = "lang/en.txt" },
  { code = "vi", file = "lang/vi.txt" },
  { code = "zh_CN", file = "lang/zh_CN.txt" },
}

local cache = {}

local function trim(value)
  return (value:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function language()
  local value = state.get("system.language") or "en"
  for _, item in ipairs(supported) do
    if item.code == value then return value end
  end
  return "en"
end

local function parse_file(path)
  local file, err = io.open(path, "r")
  if not file then return nil, err end
  local values = {}
  for line in file:lines() do
    line = line:gsub("\r$", "")
    if line ~= "" and not line:match("^%s*#") then
      local key, value = line:match("^%s*([^=]+)%s*=(.*)$")
      if key then values[trim(key)] = trim(value) end
    end
  end
  file:close()
  return values
end

local function load(code)
  if cache[code] then return cache[code] end
  for _, item in ipairs(supported) do
    if item.code == code then
      local values = parse_file(item.file)
      if values then
        cache[code] = values
        return values
      end
      break
    end
  end
  if code ~= "en" then return load("en") end
  cache.en = cache.en or { language_name = "ENGLISH" }
  return cache.en
end

function M.language()
  return language()
end

function M.set_language(code)
  for _, item in ipairs(supported) do
    if item.code == code then
      state.set("system.language", code)
      return true
    end
  end
  return false
end

function M.cycle()
  local current = language()
  for i, item in ipairs(supported) do
    if item.code == current then
      local next_item = supported[i % #supported + 1]
      state.set("system.language", next_item.code)
      return next_item.code
    end
  end
  state.set("system.language", supported[1].code)
  return supported[1].code
end

function M.language_name()
  local locale = load(language())
  return locale.language_name or language()
end

function M.t(key, ...)
  local locale = load(language())
  local fallback = load("en")
  local value = locale[key] or fallback[key] or key
  if select("#", ...) > 0 then return string.format(value, ...) end
  return value
end

function M.supported()
  local out = {}
  for i, item in ipairs(supported) do
    local locale = load(item.code)
    out[i] = { code = item.code, name = locale.language_name or item.code }
  end
  return out
end

-- Translation data is plain UTF-8 key=value text.
return M
