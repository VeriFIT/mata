@TT-explicit
# Token passing plus token creation at every ordinary leaf.
%Initial q0
%Final q0 q2
n -> n : q0
n -> t : q0
t -> n : q1
tt(q0,q0) -> nn : q1
nn(q1,q0) -> tt : q2
nn(q0,q1) -> tt : q2
nn(q0,q0) -> nn : q0
nn(q2,q0) -> nn : q2
nn(q0,q2) -> nn : q2
