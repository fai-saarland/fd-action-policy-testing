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
Atom robot-has(robot1, black)
Atom robot-has(robot1, white)
end_variable
begin_variable
var1
-1
4
Atom clear(tile_0-1)
Atom painted(tile_0-1, black)
Atom painted(tile_0-1, white)
Atom robot-at(robot1, tile_0-1)
end_variable
begin_variable
var2
-1
4
Atom clear(tile_2-1)
Atom painted(tile_2-1, black)
Atom painted(tile_2-1, white)
Atom robot-at(robot1, tile_2-1)
end_variable
begin_variable
var3
-1
4
Atom clear(tile_1-1)
Atom painted(tile_1-1, black)
Atom painted(tile_1-1, white)
Atom robot-at(robot1, tile_1-1)
end_variable
1
begin_mutex_group
3
1 3
3 3
2 3
end_mutex_group
begin_state
1
0
3
0
end_state
begin_goal
2
2 1
3 2
end_goal
14
begin_operator
change-color robot1 black white
0
1
0 0 0 1
5
end_operator
begin_operator
change-color robot1 white black
0
1
0 0 1 0
5
end_operator
begin_operator
down robot1 tile_1-1 tile_0-1
0
2
0 1 0 3
0 3 3 0
1
end_operator
begin_operator
down robot1 tile_2-1 tile_1-1
0
2
0 3 0 3
0 2 3 0
1
end_operator
begin_operator
paint-down robot1 tile_0-1 tile_1-1 black
2
3 3
0 0
1
0 1 0 1
2
end_operator
begin_operator
paint-down robot1 tile_0-1 tile_1-1 white
2
3 3
0 1
1
0 1 0 2
2
end_operator
begin_operator
paint-down robot1 tile_1-1 tile_2-1 black
2
2 3
0 0
1
0 3 0 1
2
end_operator
begin_operator
paint-down robot1 tile_1-1 tile_2-1 white
2
2 3
0 1
1
0 3 0 2
2
end_operator
begin_operator
paint-up robot1 tile_1-1 tile_0-1 black
2
1 3
0 0
1
0 3 0 1
2
end_operator
begin_operator
paint-up robot1 tile_1-1 tile_0-1 white
2
1 3
0 1
1
0 3 0 2
2
end_operator
begin_operator
paint-up robot1 tile_2-1 tile_1-1 black
2
3 3
0 0
1
0 2 0 1
2
end_operator
begin_operator
paint-up robot1 tile_2-1 tile_1-1 white
2
3 3
0 1
1
0 2 0 2
2
end_operator
begin_operator
up robot1 tile_0-1 tile_1-1
0
2
0 1 3 0
0 3 0 3
3
end_operator
begin_operator
up robot1 tile_1-1 tile_2-1
0
2
0 3 3 0
0 2 0 3
3
end_operator
0
