local shell=assert(_G.shell)
local widget=require("shell.widget")
local theme=require("shell.theme")
local M={}
local function args(style)
  local s=theme.resolve("dialog",style)
  return s.background,s.foreground,s.secondary,s.focus_background,s.focus_foreground,s.divider,s.border_width,s.text_scale
end
function M.render(spec)
  shell.api.dialog(spec.title or "", spec.body or "", args(spec.style))
  if spec.buttons then
    local box={x=40,y=260,w=shell.api.width()-80,h=330}; local count=#spec.buttons; local gap=12
    local bw=math.floor((box.w-40-gap*(count-1))/count)
    for i,label in ipairs(spec.buttons) do
      widget.button{x=box.x+20+(i-1)*(bw+gap),y=box.y+box.h-90,w=bw,h=54,text=label,
                    active=(spec.selected or 0)==i-1,style=spec.button_style}
    end
  end
end
return M
