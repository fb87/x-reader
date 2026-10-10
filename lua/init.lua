local app = require("app").create()

return {
    start = function(snapshot)
        app.start(snapshot)
    end,
    event = function(event)
        app.event(event)
    end,
    update = function(now)
        app.update(now)
    end,
    draw = function()
        app.draw()
    end,
    snapshot = function()
        return app.snapshot()
    end,
    stop = function(reason)
        app.stop(reason)
    end,
}
