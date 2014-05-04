import "actors" as actors

print "Inside parent thread, before call."

actors.spawn({parent_id -> 
    print "Inside new thread."
})

print "Inside parent thread, after call."

