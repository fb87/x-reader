local shell = assert(_G.shell)
local api = shell.api
local theme = require("shell.theme")
local M = { gray = theme.gray }
M.icon = { book = 1, folder = 2, settings = 4, close = 12, sd_card = 11 }
local function args(kind, style)
  local s = theme.resolve(kind, style)
  return s.background, s.foreground, s.secondary, s.focus_background,
         s.focus_foreground, s.divider, s.border_width, s.text_scale
end
function M.status(title, style) api.status(title, args("status", style)) end
function M.label(spec) api.text(spec.x, spec.y, spec.text or "", spec.scale or 2, spec.gray or M.gray.black) end
function M.center(spec) api.center(spec.x, spec.y, spec.w, spec.h, spec.text or "", spec.scale or 2, spec.gray or M.gray.black) end
function M.row(spec)
  local background, foreground, secondary, focus_background, focus_foreground, divider, border_width, text_scale = args("row", spec.style)
  api.row(spec.x or 12, spec.y, spec.w or (api.width() - 24), spec.h or 68,
           spec.primary or "", spec.secondary or "", spec.selected == true,
           background, foreground, secondary, focus_background, focus_foreground, divider,
           border_width, text_scale, spec.icon or 0)
end
function M.button(spec)
  api.button(spec.x, spec.y, spec.w, spec.h, spec.text or "", spec.active == true,
             args("button", spec.style))
end
function M.progress(spec)
  api.progress(spec.x, spec.y, spec.w, spec.h, spec.value or 0, args("progress", spec.style))
end
function M.book_card(spec)
  api.book_card(spec.x, spec.y, spec.w, spec.h, spec.eyebrow or "", spec.title or "",
                spec.active == true, args("book_card", spec.style))
end
function M.dock(labels, style)
  local count=#labels; if count==0 then return end
  local w=api.width(); local y=api.height()-64; local item_w=math.floor(w/count)
  local focused=(shell.state.get("app.focus.area") or 0)==1
  local selected=shell.state.get("app.dock.selected") or 0
  api.hline(0,y,w,M.gray.black)
  for i,label in ipairs(labels) do
    M.button {x=(i-1)*item_w,y=y+1,w=item_w,h=63,text=label,
              active=focused and selected==i-1,style=style}
  end
end
return M
