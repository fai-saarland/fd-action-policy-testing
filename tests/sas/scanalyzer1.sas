begin_version
3
end_version
begin_metric
1
end_metric
4
begin_variable
var0
-1
2
Atom on(car-in-1, seg-in-1)
Atom on(car-in-1, seg-out-1)
end_variable
begin_variable
var1
-1
2
Atom on(car-out-1, seg-in-1)
Atom on(car-out-1, seg-out-1)
end_variable
begin_variable
var2
-1
2
Atom analyzed(car-out-1)
NegatedAtom analyzed(car-out-1)
end_variable
begin_variable
var3
-1
2
Atom analyzed(car-in-1)
NegatedAtom analyzed(car-in-1)
end_variable
2
begin_mutex_group
2
0 0
1 0
end_mutex_group
begin_mutex_group
2
0 1
1 1
end_mutex_group
begin_state
0
1
1
1
end_state
begin_goal
4
0 0
1 1
2 0
3 0
end_goal
4
begin_operator
analyze-2 seg-in-1 seg-out-1 car-in-1 car-out-1
0
3
0 3 -1 0
0 0 0 1
0 1 1 0
3
end_operator
begin_operator
analyze-2 seg-in-1 seg-out-1 car-out-1 car-in-1
0
3
0 2 -1 0
0 0 1 0
0 1 0 1
3
end_operator
begin_operator
rotate-2 seg-in-1 seg-out-1 car-in-1 car-out-1
0
2
0 0 0 1
0 1 1 0
1
end_operator
begin_operator
rotate-2 seg-in-1 seg-out-1 car-out-1 car-in-1
0
2
0 0 1 0
0 1 0 1
1
end_operator
0
