//Code by Ben Grabham - Imperial College 2014

import core.thread;
import std.concurrency;
import std.random;
import std.stdio;

alias ForkID = ulong;
enum numForks = 5;

enum Request {
  Ok,
  Denied
}

struct Person {
  string name;
  ForkID[] forks;
}

void wait() {
  Thread.sleep(uniform(1, 3).seconds);
}

void table() {
  bool[numForks] forksAvailable = true;
  while(true) {
    receive((Tid person, ForkID fork) {
          if(forksAvailable[fork - 1]) {
            forksAvailable[fork - 1] = false;
            person.send(Request.Ok);
          } else {
            person.send(Request.Denied);
          }
        }, (ForkID fork) {
          forksAvailable[fork - 1] = true;
        });
  }
}

void philosopher(Tid tableTid, immutable Person person) {
  while(true) {
    stdout.writefln("[%s] I'm thinking", person.name);
    wait();
    foreach(ref i, ForkID fork; person.forks) {
      stdout.writefln("[%s] I'm requesting fork %u", person.name, fork);
      wait();
      tableTid.send(thisTid, fork);
      if(receiveOnly!Request == Request.Ok) {
        stdout.writefln("[%s] Fork %u acquired", person.name, fork);
      } else {
        stdout.writefln("[%s] Fork %u denied", person.name, fork);
        --i;
      }
    }

    stdout.writefln("[%s] I'm eating", person.name);
    wait();

    foreach(i, ForkID fork; person.forks) {
      stdout.writefln("[%s] Returning fork %u", person.name, fork);
      wait();
      tableTid.send(fork);
    }
  }
}

void main() {
  immutable Person[] people = [
      Person("Marx", [1, 2]),
      Person("Descartes", [2, 3]),
      Person("Nietzsche", [3, 4]),
      Person("Aristotle", [4, 5]),
      Person("Kant", [1, 5])
    ];

  Tid tableTid = spawn(&table);

  foreach(person; people) {
    spawn(&philosopher, tableTid, person);
  }
}
