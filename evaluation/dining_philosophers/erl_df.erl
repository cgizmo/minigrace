-module(erl_df).
-export([start/0, table/1, philosopher/3]).

start() ->
    DefaultTableState = {available, available, available, available, available},
    Philosophers = [{"Marx", [1, 2]}, {"Descartes", [2, 3]}, {"Nietzsche", [3, 4]},
                    {"Aristotle", [4, 5]}, {"Kant", [1, 5]}],
    Table = spawn(erl_df, table, [DefaultTableState]),
    lists:map(fun({Name, Forks}) ->
                      spawn(fun() ->
                                    random:seed(now()),
                                    philosopher(Table, Name, Forks)
                            end)
              end, Philosophers).

table(State) ->
    receive
        {Pid, acquire, Fork} when element(Fork, State) == available ->
            Pid ! ok,
            table(setelement(Fork, State, Pid));
        {Pid, acquire, _} ->
            Pid ! denied,
            table(State);
        {_, replace, Fork} ->
            table(setelement(Fork, State, available))
    end.

philosopher(Table, Name, Forks) ->
    think(Name),
    hungry_philosopher(Table, Name, [], Forks).

hungry_philosopher(Table, Name, MyForks, []) ->
    eat(Name),
    lists:map(fun(Fork) -> replace_fork(Table, Name, Fork) end, MyForks),
    philosopher(Table, Name, lists:reverse(MyForks));

hungry_philosopher(Table, Name, MyForks, [RequestedFork|TailForks]) ->
    case acquire_fork(Table, Name, RequestedFork) of
        ok -> hungry_philosopher(Table, Name, [RequestedFork|MyForks], TailForks);
        denied -> hungry_philosopher(Table, Name, MyForks, [RequestedFork|TailForks])
    end.

wait() ->
    SleepMs = random:uniform(3000),
    timer:sleep(SleepMs).

eat(Name) ->
    io:format("~s is eating...~n", [Name]),
    wait().

think(Name) ->
    io:format("~s is thinking...~n", [Name]),
    wait().

acquire_fork(Table, Name, Fork) ->
    io:format("~s is requesting fork ~p.~n", [Name, Fork]),
    wait(),
    Table ! {self(), acquire, Fork},
    receive
        ok -> io:format("~s has acquired fork ~p.~n", [Name, Fork]), ok;
        denied -> io:format("~s has been denied fork ~p.~n", [Name, Fork]), denied
    end.

replace_fork(Table, Name, Fork) ->
    io:format("~s is replacing fork ~p.~n", [Name, Fork]),
    wait(),
    Table ! {self(), replace, Fork}.

