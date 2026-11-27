@TT-explicit
# Structure-preserving Boolean-value percolation.
%Initial q0
%Final qD
oo -> oo : q0
ii -> ii : q1
o(q0,q0) -> o : q0
i(q0,q1) -> i : q1
i(q1,q0) -> i : q1
i(q1,q1) -> i : q1
u(q0,q0) -> u : qU
u(q0,q1) -> u : qU
u(q0,qU) -> u : qU
u(q1,q0) -> u : qU
u(q1,q1) -> u : qU
u(q1,qU) -> u : qU
u(qU,q0) -> u : qU
u(qU,q1) -> u : qU
u(qU,qU) -> u : qU
u(q0,qD) -> u : qD
u(q1,qD) -> u : qD
u(qU,qD) -> u : qD
u(qD,q0) -> u : qD
u(qD,q1) -> u : qD
u(qD,qU) -> u : qD
u(q0,q0) -> o : qD
u(q0,q1) -> i : qD
u(q1,q0) -> i : qD
u(q1,q1) -> i : qD
