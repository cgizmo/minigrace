dialect "actors"

method pong(parent) {
    var alive := true

    while { alive } do {
        receive { x -> match(x)
            case { "ping" -> parent ! "pong" }
            case { _ -> alive := false }
        }
    }
}

def pong_child = spawn({ parent -> pong(parent) })
print "{me()} made child {pong_child}"

var i := 0

while { i < 10 } do {
    print "Ping !"
    pong_child ! "ping"
    receive { "pong" -> print "Pong !" }
    i := i + 1
}

print "Bye !"
pong_child ! "die"
