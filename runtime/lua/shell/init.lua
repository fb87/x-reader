local shell = assert(_G.shell, "native shell module is unavailable")

shell.theme = require("shell.theme")
shell.page = require("shell.page")
shell.widget = require("shell.widget")
shell.layout = require("shell.layout")
shell.dialog = require("shell.dialog")
shell.i18n = require("shell.i18n")
shell.router = require("shell.router")

function shell.reload()
    shell.api.reload()
end

return shell
