local shell = require("shell")
local page = require("plugins.wifi.lua.pages.wifi")

local M = {}

function M.init()
    shell.router.register {
        path = "/settings/network",
        title = "wifi",
        page = page,
        menu = {
            section = "settings",
            order = 10,
            value = function()
                return shell.state.get("network.wifi.connected") and shell.i18n.t("connected") or shell.i18n.t("off")
            end,
        },
    }
    return true
end

return M
