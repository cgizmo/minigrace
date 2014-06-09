import "actors_prim" as actors_prim

print "[parent] starting child thread"

def child = actors_prim.spawn({parent -> 
    print "[child] polling for 3 seconds maximum"
    def res = actors_prim.timed_poll(3)

    match(res)
        case { _ : actors_prim.TimedOut -> print "[child] timed out" }
        case { a -> print "[child] got {a}" }
}, actors_prim.me())

print "[parent] not doing anything"
