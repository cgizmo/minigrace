import "actors_lib" as actors_lib
import "math" as math

dialect "actors"

method wait() {
    def sleepS = (math.random * 3).truncate + 1
    receive { _ -> true } after (sleepS) do { true }
}

class philosopher.new(tableAID, name, forks) {
    var acquiredForks := []
    var requiredForks := forks

    method think is confidential {
        print("{name} is thinking...")
        wait()
    }

    method eat is confidential {
        print("{name} is eating...")
        wait()
    }

    method acquireAllForks is confidential {
        while { requiredForks.size > 0 } do {
            def fork = requiredForks.pop
            print("{name} is requesting fork {fork}.")
            wait()

            tableAID ! [me(), "acquire", fork]
            receive { x -> match(x)
                case { "ok" -> print("{name} has acquired fork {fork}.");
                               acquiredForks.push(fork) }
                case { "denied" -> print("{name} has been denied fork {fork}.");
                                   requiredForks.push(fork) }
            }
        }
    }

    method replaceAllForks is confidential {
        while { acquiredForks.size > 0 } do {
            def fork = acquiredForks.pop
            print("{name} is replacing fork {fork}.")
            wait()

            tableAID ! [me(), "replace", fork]

            requiredForks.push(fork)
        }
    }

    method philosophise {
        while { true } do {
            think
            acquireAllForks
            eat
            replaceAllForks
        }
    }
}

class table.new(numForks) {
    var state := []

    method onStart() {
        state := [me(), me(), me(), me(), me()]
    }

    method onFinish() { }
    
    method onMessage(msg) {
        def sender = msg[1]
        def fork = msg[3]

        match (msg[2])
            case { "acquire" ->
                     if (state[fork] == me()) then {
                        state[fork] := sender
                        sender ! "ok"
                     } else {
                        sender ! "denied"
                     }
            }
            case { "replace" -> state[fork] := me() }
    }
}

def numForks = 5
def philosophers = [["Marx", [1, 2]],
                    ["Descartes", [2, 3]],
                    ["Nietzsche", [3, 4]],
                    ["Aristotle", [4, 5]],
                    ["Kant", [1, 5]]]
def tableAID = actors_lib.spawnActor(table.new(numForks))

for (philosophers) do { p ->
    var forks := []
    forks.push(p[2][2])
    forks.push(p[2][1])

    spawn({ _ -> philosopher.new(tableAID, p[1], forks).philosophise })
}
