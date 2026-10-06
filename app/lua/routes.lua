local shell = require("shell")
local router = shell.router
local i18n = shell.i18n

local M = {}

function M.register()
    router.clear()
    router.register { path = "/", title = "home", page = require("pages.home") }
    router.register { path = "/splash", title = "reader_name", page = require("pages.splash") }
    router.register { path = "/library", title = "library", page = require("pages.library"),
                      menu = { section = "home", order = 10, icon = "library" } }
    router.register { path = "/library/favorites", title = "favorites", page = require("pages.favorites"),
                      menu = { section = "home", order = 20, icon = "favorite" } }
    router.register { path = "/files", title = "file_manager", page = require("pages.files"),
                      menu = { section = "home", order = 30, icon = "folder" } }
    router.register { path = "/book/:id/reader", title = "reading", page = require("pages.reader") }
    router.register { path = "/settings", title = "settings", page = require("pages.settings"),
                      menu = { section = "home", order = 40, icon = "settings" } }
    router.register { path = "/sleep", title = "sleep", page = require("pages.sleep"),
                      menu = { section = "home", order = 50, icon = "sleep" } }
end

return M
