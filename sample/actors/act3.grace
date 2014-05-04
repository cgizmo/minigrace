import "actors" as actors

print "[parent] starting child thread"

def child = actors.spawn({parent -> 
    print "[child] polling..."
    def res = actors.poll()
    print "[child] got '{res}'"
    print "[child] sending back 'got it'"
    actors.post(parent, "got it")
})

print "[parent] sending 'hello child' to child"
actors.post(child, "hello child")
print "[parent] waiting for answer..."
def res = actors.poll()
print "[parent] got back '{res}' from child"

