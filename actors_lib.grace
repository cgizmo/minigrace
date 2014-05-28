dialect "actors"

type Actor = {
    onStart() -> Done
    onMessage(msg : Object) -> Done
    onFinish() -> Done
}

type TimeoutActor = Actor & type {
    onTimeout() -> Done
}

class runActor.with(actor' : Actor) {
    inherits runner.with(actor')

    method waitForMsg() -> Done is override, confidential {
        receive { x -> match(x)
            case { _ : Signal -> super.signal(x) }
            case { _ -> super.actor.onMessage(x) }
        }
    }
}

class runTimeoutActor.with(timeout : Number, actor' : TimeoutActor) {
    inherits runner.with(actor')

    method waitForMsg() -> Done is override, confidential {
        receive { x -> match(x)
            case { _ : Signal -> super.signal(x) }
            case { _ -> super.actor.onMessage(x) }
        } after (timeout) do {
            super.actor.onTimeout()
        }
    }
}

class runner.with(actor') is confidential {
    def actor : Actor = actor'
    var running : Boolean := false

    method start() -> Done {
        running := true
        actor.onStart()

        while {running} do {
            self.waitForMsg()
        }

        actor.onFinish()
    }

    method signal(sig : Signal) -> Done is confidential {
        match(sig.getSignal())
            case { "STOP" | "KILL" -> running := false }
    }
}

method spawnActor(actor : Actor) {
    spawn({ _ -> runActor.with(actor).start() })
}

method spawnTimeoutActor(timeout : Number, actor : TimeoutActor) {
    spawn({ _ -> runTimeoutActor.with(timeout, actor).start() })
}
