import "actors_lib" as actors_lib

dialect "actors"

class pong.new(parent') -> actors_lib.Actor {
    def parent = parent'

    method onStart() {
        print "(pong) Started !"
    }

    method onFinish() {
        print "(pong) Bye !"
    }

    method onMessage(msg) {
        match(msg)
            case { "ping" -> print("Ping !"); parent ! "pong" }
    }
}

class ping.new(pong_child') -> actor_lib.Actor {
    def pong_child = pong_child'
    var i := 0

    method onStart() {
        print "(ping) Started !"
        pong_child ! "ping"
    }

    method onFinish() {
        print "(ping) Bye !"
    }

    method onMessage(msg) {
        i := i + 1
        if (i >= 10) then {
            pong_child ! STOP_SIGNAL
            me() ! STOP_SIGNAL
        }

        match(msg)
            case { "pong" -> print("Pong !"); pong_child ! "ping" }
    }
}

def pong_child = actors_lib.spawnActor(pong.new(me()))
actors_lib.runActor.with(ping.new(pong_child)).start()
