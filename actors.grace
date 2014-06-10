import "StandardPrelude" as StandardPrelude
import "actors_prim" as actors_prim

inherits StandardPrelude.methods

type Signal = {
    getSignal() -> String
}

type AID = {
    !(msg : Copyable) -> Done
}

class baseSignal.new(signal') -> Signal & Copyable {
    def signal : String = signal'

    method getSignal() -> String {
        signal
    }

    method copy() {
        baseSignal.new(signal)
    }

    method asString() {
        "Signal<{signal}>"
    }
}

def STOP_SIGNAL : Signal = baseSignal.new("STOP")
def KILL_SIGNAL : Signal = baseSignal.new("KILL")

class aid.new(aid') -> AID & Copyable {
    def prim_aid : actors_prim.AID = aid'

    method !(msg : Copyable | Signal) -> Done {
        // Put a KILL signal on the front of the queue
        if (Signal.match(msg) && (msg == KILL_SIGNAL)) then {
            actors_prim.priority_post(prim_aid, msg.copy())
        }
        // Put any other signal/message at the back of the queue
        else {
            actors_prim.post(prim_aid, msg.copy())
        }
    }

    method copy() {
        aid.new(prim_aid)
    }

    method ==(otherAid) {
        prim_aid == otherAid.getPrimAid
    }

    method asString {
        "AID<{prim_aid}>"
    }

    method getPrimAid {
        prim_aid
    }
}

method receive(block) {
    def res = actors_prim.poll()
    block.apply(res)
}

method receive(block)
       after(secs)
       do(timeout) {
    def res = actors_prim.timed_poll(secs)

    match(res)
        case { _ : actors_prim.TimedOut -> timeout.apply() }
        case { _ -> block.apply(res) }
}

method spawn(block) {
    def parent = me()
    aid.new(actors_prim.spawn(block, parent.copy()))
}

method me() {
    aid.new(actors_prim.me())
}

