import "StandardPrelude" as StandardPrelude
import "actors_prim" as actors_prim

inherits StandardPrelude.methods

// TODO : add nice string representation
class Actor.new(aid') {
    inherits false

    def aid = aid'

    method !(msg) {
        actors_prim.post(aid, msg)
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
    def parent = self()
    Actor.new(actors_prim.spawn(block, parent))
}

method self() {
    Actor.new(actors_prim.self())
}
