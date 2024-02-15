begin_version
3
end_version
begin_metric
0
end_metric
4
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
2
Atom pointing(satellite0, planet1)
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
Atom have_image(planet1, thermograph0)
NegatedAtom have_image(planet1, thermograph0)
end_variable
0
begin_state
0
1
1
1
end_state
begin_goal
2
1 1
3 0
end_goal
6
begin_operator
calibrate satellite0 instrument0 star0
2
1 1
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
take_image satellite0 planet1 instrument0 thermograph0
3
2 0
1 0
0 1
1
0 3 -1 0
1
end_operator
begin_operator
turn_to satellite0 planet1 star0
0
1
0 1 1 0
1
end_operator
begin_operator
turn_to satellite0 star0 planet1
0
1
0 1 0 1
1
end_operator
0
