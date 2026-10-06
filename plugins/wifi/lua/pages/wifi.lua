local shell = require("shell")
local widget = shell.widget
local state = shell.state
local i18n = shell.i18n
local model = require("model")
local wifi = require("modules.wifi")
local M = { id = model.page.connectivity }

local function labels()
    return { i18n.t("scan_networks"), i18n.t("connect_first"), i18n.t("back") }
end

function M.render()
    shell.api.clear(widget.gray.white)
    widget.status(i18n.t("wifi"))
    local selected = state.get("app.menu.selected") or 0
    for i, label in ipairs(labels()) do
        widget.row { y = 90 + (i - 1) * 76, primary = label, selected = selected == i - 1 }
    end
    local ssid = state.get("network.wifi.ssid") or ""
    if ssid ~= "" then widget.label { x = 24, y = 350, text = i18n.t("connected_ssid", ssid) } end
end

local function activate(selected)
    if selected == 0 then wifi.scan()
    elseif selected == 1 then wifi.connect_first()
    else model.set_page(model.page.settings) end
end

function M.event(ev)
    local count = #labels()
    if ev.type == "tap" and ev.y >= 90 and ev.y < 90 + count * 76 then
        local selected = math.floor((ev.y - 90) / 76)
        state.set("app.menu.selected", selected); activate(selected); return true
    end
    if ev.type ~= "key" then return false end
    if ev.key == "back" then model.set_page(model.page.settings); return true end
    if ev.key == "up" then model.move("app.menu.selected", -1, count); return true end
    if ev.key == "down" then model.move("app.menu.selected", 1, count); return true end
    if ev.key == "ok" then activate(state.get("app.menu.selected") or 0); return true end
    return false
end
return M
