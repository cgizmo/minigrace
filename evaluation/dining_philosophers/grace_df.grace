import "actors_lib" as actors_lib
import "math" as math

dialect "actors"

method wait {
   def sleepMs = (math.random * 3000).truncate
   receive { _ -> done } after (sleepMs) do { done }
}

class philosopher.new(tableAID, name, forks) {
    var acquiredForks := []
    var requiredForks := forks

    method think is confidential {
        print("{name} is thinking...")
        wait
    }

    method eat is confidential {
        print("{name} is eating...")
        wait
    }

    method acquireAllForks is confidential {
        while { requiredForks.size > 0 } do {
            def fork = requiredForks.pop

            tableAID ! ["acquire", me, fork]
            receive { x -> match(x)
                case { "ok" -> print("{name} has acquired fork {fork}.");
                               acquiredForks.push(fork) }
                case { "denied" -> requiredForks.push(fork) }
            }
        }
    }

    method returnAllForks is confidential {
        while { acquiredForks.size > 0 } do {
            def fork = acquiredForks.pop
            print("{name} is returning fork {fork}.")

            tableAID ! ["return", fork]
            requiredForks.push(fork)
        }
    }

    method philosophise {
        while { true } do {
            think
            acquireAllForks
            eat
            returnAllForks
        }
    }
}

class table.new(state') {
    var state := state'

    method onStart { }
    method onFinish { }
    
    method onMessage(msg) {
        match (msg[1])
            case { "acquire" ->
                    def sender = msg[2]
                    def fork = msg[3]

                    if (state[fork]) then {
                        state[fork] := false
                        sender ! "ok"
                    } else {
                        sender ! "denied"
                    }
            }
            case { "return" -> state[msg[2]] := true }
    }
}

def defaultTableState = [true, true , true, true, true]
def philosophers = [["Marx", [1, 2]], ["Descartes", [2, 3]], ["Nietzsche", [3, 4]],
                    ["Aristotle", [4, 5]], ["Kant", [1, 5]]]
def tableAID = actors_lib.spawnActor(table.new(defaultTableState))

for (philosophers) do { p ->
    // Create the fork stack. The first fork to be popped must be the fork at
    // p[2][1], so it is pushed last.
    var forks := []
    forks.push(p[2][2])
    forks.push(p[2][1])

    spawn { _ -> philosopher.new(tableAID, p[1], forks).philosophise }
}
