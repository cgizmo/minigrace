import "actors_prim" as actors_prim

print "[parent] starting child thread"

def child = actors_prim.spawn({parent -> 
    print "[child] polling..."
    def res = actors_prim.poll()
    print "[child] got '{res}'"
    print "[child] sending back 'got it'"
    actors_prim.post(parent, "got it")
})

print "[parent] sending 'hello child' to child"
actors_prim.post(child, "hello child")
print "[parent] waiting for answer..."
def res = actors_prim.poll()
print "[parent] got back '{res}' from child"

