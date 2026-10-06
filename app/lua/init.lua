package.path = "build/generated/?.lua;./?.lua;app/lua/?.lua;app/lua/?/init.lua;runtime/lua/?.lua;runtime/lua/?/init.lua;" .. package.path

local application = require("main")

function init(reload) return application.init(reload) end
function render() return application.render() end
function on_event(ev) return application.event(ev) end
function tick(now_ms) application.tick(now_ms) end
