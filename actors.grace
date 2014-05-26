import "StandardPrelude" as StandardPrelude
import "actors_prim" as actors_prim

inherits StandardPrelude.methods

class Actor.new(aid') {
    inherits false

    def aid = aid'

    method !(msg : Copyable) {
        actors_prim.post(aid, msg.copy())
    }

    method asString {
        "Actor<{aid}>"
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
    Actor.new(actors_prim.spawn(block, parent))
}

method me() {
    Actor.new(actors_prim.me())
}
