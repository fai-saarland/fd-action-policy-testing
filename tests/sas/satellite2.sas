begin_version
3
end_version
begin_metric
0
end_metric
5
begin_variable
var0
-1
2
Atom power_avail(satellite0)
Atom power_on(instrument0)
end_variable
begin_variable
var1
-1
4
Atom pointing(satellite0, groundstation1)
Atom pointing(satellite0, phenomenon2)
Atom pointing(satellite0, phenomenon3)
Atom pointing(satellite0, star0)
end_variable
begin_variable
var2
-1
2
Atom calibrated(instrument0)
NegatedAtom calibrated(instrument0)
end_variable
begin_variable
var3
-1
2
Atom have_image(phenomenon3, thermograph0)
NegatedAtom have_image(phenomenon3, thermograph0)
end_variable
begin_variable
var4
-1
2
Atom have_image(phenomenon2, thermograph0)
NegatedAtom have_image(phenomenon2, thermograph0)
end_variable
0
begin_state
0
1
1
1
1
end_state
begin_goal
3
1 3
3 0
4 0
end_goal
17
begin_operator
calibrate satellite0 instrument0 star0
2
1 3
0 1
1
0 2 -1 0
1
end_operator
begin_operator
switch_off instrument0 satellite0
0
1
0 0 1 0
1
end_operator
begin_operator
switch_on instrument0 satellite0
0
2
0 2 -1 1
0 0 0 1
1
end_operator
begin_operator
take_image satellite0 phenomenon2 instrument0 thermograph0
3
2 0
1 1
0 1
1
0 4 -1 0
1
end_operator
begin_operator
take_image satellite0 phenomenon3 instrument0 thermograph0
3
2 0
1 2
0 1
1
0 3 -1 0
1
end_operator
begin_operator
turn_to satellite0 groundstation1 phenomenon2
0
1
0 1 1 0
1
end_operator
begin_operator
turn_to satellite0 groundstation1 phenomenon3
0
1
0 1 2 0
1
end_operator
begin_operator
turn_to satellite0 groundstation1 star0
0
1
0 1 3 0
1
end_operator
begin_operator
turn_to satellite0 phenomenon2 groundstation1
0
1
0 1 0 1
1
end_operator
begin_operator
turn_to satellite0 phenomenon2 phenomenon3
0
1
0 1 2 1
1
end_operator
begin_operator
turn_to satellite0 phenomenon2 star0
0
1
0 1 3 1
1
end_operator
begin_operator
turn_to satellite0 phenomenon3 groundstation1
0
1
0 1 0 2
1
end_operator
begin_operator
turn_to satellite0 phenomenon3 phenomenon2
0
1
0 1 1 2
1
end_operator
begin_operator
turn_to satellite0 phenomenon3 star0
0
1
0 1 3 2
1
end_operator
begin_operator
turn_to satellite0 star0 groundstation1
0
1
0 1 0 3
1
end_operator
begin_operator
turn_to satellite0 star0 phenomenon2
0
1
0 1 1 3
1
end_operator
begin_operator
turn_to satellite0 star0 phenomenon3
0
1
0 1 2 3
1
end_operator
0
