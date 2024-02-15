begin_version
3
end_version
begin_metric
0
end_metric
7
begin_variable
var0
-1
3
Atom at(bob, gate)
Atom at(bob, location1)
Atom at(bob, shed)
end_variable
begin_variable
var1
-1
2
Atom at(spanner2, location1)
Atom carrying(bob, spanner2)
end_variable
begin_variable
var2
-1
2
Atom at(spanner1, location1)
Atom carrying(bob, spanner1)
end_variable
begin_variable
var3
-1
2
Atom useable(spanner1)
NegatedAtom useable(spanner1)
end_variable
begin_variable
var4
-1
2
Atom useable(spanner2)
NegatedAtom useable(spanner2)
end_variable
begin_variable
var5
-1
2
Atom loose(nut1)
Atom tightened(nut1)
end_variable
begin_variable
var6
-1
2
Atom loose(nut2)
Atom tightened(nut2)
end_variable
0
begin_state
2
0
0
0
0
0
0
end_state
begin_goal
2
5 1
6 1
end_goal
8
begin_operator
pickup_spanner location1 spanner1 bob
1
0 1
1
0 2 0 1
1
end_operator
begin_operator
pickup_spanner location1 spanner2 bob
1
0 1
1
0 1 0 1
1
end_operator
begin_operator
tighten_nut gate spanner1 bob nut1
2
0 0
2 1
2
0 5 0 1
0 3 0 1
1
end_operator
begin_operator
tighten_nut gate spanner1 bob nut2
2
0 0
2 1
2
0 6 0 1
0 3 0 1
1
end_operator
begin_operator
tighten_nut gate spanner2 bob nut1
2
0 0
1 1
2
0 5 0 1
0 4 0 1
1
end_operator
begin_operator
tighten_nut gate spanner2 bob nut2
2
0 0
1 1
2
0 6 0 1
0 4 0 1
1
end_operator
begin_operator
walk location1 gate bob
0
1
0 0 1 0
1
end_operator
begin_operator
walk shed location1 bob
0
1
0 0 2 1
1
end_operator
0
