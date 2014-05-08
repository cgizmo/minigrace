import "actors" as actors

print "[parent] starting child thread"

def child = actors.spawn({parent -> 
    print "[child] polling for 3 seconds maximum"
    def res = actors.timedpoll(3)

    match(res)
        case { _ : actors.TimedOut -> print "[child] timed out" }
        case { a -> print "[child] got {a}" }
})

print "[parent] not doing anything"
