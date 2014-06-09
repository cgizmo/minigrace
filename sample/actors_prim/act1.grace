import "actors_prim" as actors_prim

print "Inside parent thread, before call."

actors_prim.spawn({parent_id -> 
    print "Inside new thread."
}, actors_prim.me())

print "Inside parent thread, after call."

