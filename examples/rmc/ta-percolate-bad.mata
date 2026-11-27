@TA-explicit
# Percolate bad configurations, as in ARTmc's simple_artmc_exprm.ml.
%Alphabet-auto
%Final q0 q3
oo -> q2
o(q2,q2) -> q1
i(q2,q2) -> q1
u(q2,q2) -> q1
o(q2,q1) -> q1
i(q2,q1) -> q1
u(q2,q1) -> q1
o(q1,q2) -> q1
i(q1,q2) -> q1
u(q1,q2) -> q1
o(q1,q1) -> q1
i(q1,q1) -> q1
u(q1,q1) -> q1
i(q2,q2) -> q0
i(q2,q1) -> q0
i(q1,q2) -> q0
i(q1,q1) -> q0
oo -> q5
ii -> q4
o(q5,q5) -> q5
i(q5,q5) -> q5
u(q5,q5) -> q5
o(q5,q4) -> q4
i(q5,q4) -> q4
u(q5,q4) -> q4
o(q4,q5) -> q4
i(q4,q5) -> q4
u(q4,q5) -> q4
o(q4,q4) -> q4
i(q4,q4) -> q4
u(q4,q4) -> q4
o(q5,q4) -> q3
o(q4,q5) -> q3
o(q4,q4) -> q3
