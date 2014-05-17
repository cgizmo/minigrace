import "StandardPrelude" as StandardPrelude
import "actors_prim" as actors_prim

inherits StandardPrelude.methods

class Actor.new(aid') is confidential {
    inherits false

    def aid = aid'

    method !(msg) {
        actors_prim.post(aid, msg)
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
    def res = actors_prim.timedpoll(secs)

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
