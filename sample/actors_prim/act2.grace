import "actors_prim" as actors_prim

def xs = [1,2,3,4,5,6,7,8,9,10]

print "Inside parent thread, before call."

for (xs) do { x ->
    actors_prim.spawn({parent_id -> 
        print "({x})"
    }, actors_prim.self())
}

print "Inside parent thread, after call."

