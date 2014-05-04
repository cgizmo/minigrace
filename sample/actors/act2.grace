import "actors" as actors

def xs = [1,2,3,4,5,6,7,8,9,10]

print "Inside parent thread, before call."

for (xs) do { x ->
    actors.spawn({parent_id -> 
        print "({x})"
    })
}

print "Inside parent thread, after call."

