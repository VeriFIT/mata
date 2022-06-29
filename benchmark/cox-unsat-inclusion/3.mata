%Section AFA Bits
%InitialFormula qC0_0 & qC1_0
%FinalFormula !qC0_4 & !qC0_3 & !qC0_1 & !qC0_2 & !qC0_0 & !qC1_4
qC0_5 false
qC1_2 a15 | a14 | a13 | a12 | a11 | a10 | a9 | a8 | a7 | a6 | !a5 | !a4 | a3 | a2 | a1 | (a0 & !a0) | qC1_3
qC0_2 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & (!a0 | a0) & qC0_3
qC0_1 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & a0 & qC0_2
qC1_1 a15 | a14 | a13 | a12 | a11 | a10 | a9 | a8 | a7 | a6 | !a5 | !a4 | a3 | a2 | a1 | a0 | qC1_2
qC1_4 true
qC0_3 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & (!a0 | a0) & qC0_4
qC0_4 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & (!a0 | a0) & qC0_5
qC1_3 a15 | a14 | a13 | a12 | a11 | a10 | a9 | a8 | a7 | a6 | !a5 | !a4 | a3 | a2 | a1 | (a0 & !a0) | qC1_4
qC1_0 a15 | a14 | a13 | a12 | a11 | a10 | a9 | a8 | a7 | a6 | !a5 | !a4 | a3 | a2 | a1 | (a0 & !a0) | qC1_1
qC0_0 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & a0 & qC0_2
