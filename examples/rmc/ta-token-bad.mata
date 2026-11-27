@TA-explicit
# Bad token configurations: no token or at least two tokens.
%Alphabet-auto
%Final p2
n -> p0
t -> p1
nn(p0,p0) -> p0
tt(p0,p0) -> p1
nn(p1,p0) -> p1
nn(p0,p1) -> p1
tt(p1,p0) -> p2
tt(p0,p1) -> p2
nn(p1,p1) -> p2
tt(p1,p1) -> p2
nn(p2,p0) -> p2
nn(p0,p2) -> p2
tt(p2,p0) -> p2
tt(p0,p2) -> p2
tt(p1,p2) -> p2
tt(p2,p1) -> p2
nn(p1,p2) -> p2
nn(p2,p1) -> p2
nn(p2,p2) -> p2
tt(p2,p2) -> p2
