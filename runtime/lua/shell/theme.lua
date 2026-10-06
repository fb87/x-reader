local M = {}
M.gray = { black = 0, dark = 5, light = 10, white = 15 }
M.current = {
  status = {}, row = {}, button = {}, progress = {}, book_card = {}, dialog = {},
}
local defaults = {
  background = 15, foreground = 0, secondary = 5,
  focus_background = 0, focus_foreground = 15, divider = 10,
  border_width = 2, text_scale = 2,
}
function M.resolve(kind, override)
  local out = {}; for k,v in pairs(defaults) do out[k]=v end
  for k,v in pairs(M.current[kind] or {}) do out[k]=v end
  for k,v in pairs(override or {}) do out[k]=v end
  return out
end
function M.set(kind, values) M.current[kind] = values or {} end
function M.reset() for k,_ in pairs(M.current) do M.current[k] = {} end end
return M
