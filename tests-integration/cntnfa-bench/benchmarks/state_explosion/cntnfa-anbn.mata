@CNTNFA-explicit
%States q0 q1 q2
%Alphabet-auto
%Initial q0
%Final q2
%Registers c0
q0 0 (+ c0 1) q0
q0 1 (> c0 1) (+ c0 -1) q1
q0 1 (= c0 1) (+ c0 -1) q2
q1 1 (= c0 1) (+ c0 -1) q2
