dofile("app/lua/init.lua")
local shell = require("shell")
shell.theme.set("row", {
    foreground = 5,
    divider = 0,
    border_width = 3,
})
shell.theme.set("status", {
    foreground = 5,
})
