%Section AFA Bits
%InitialFormula qC0_0 & qC1_0
%FinalFormula !qC0_3 & !qC0_1 & !qC0_2 & !qC0_0 & !qC1_3
qC1_2 a15 | a14 | a13 | a12 | a11 | a10 | a9 | a8 | a7 | a6 | !a5 | !a4 | a3 | a2 | a1 | (a0 & !a0) | qC1_3
qC0_2 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & (!a0 | a0) & qC0_3
qC0_1 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & (!a0 | a0) & qC0_1
qC1_1 a15 | a14 | a13 | a12 | a11 | a10 | a9 | a8 | a7 | a6 | !a5 | !a4 | a3 | a2 | a1 | (a0 & !a0) | qC1_1
qC0_3 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & (!a0 | a0) & qC0_4
qC0_4 false
qC1_3 true
qC1_0 a15 | a14 | a13 | a12 | a11 | a10 | a9 | a8 | a7 | a6 | !a5 | !a4 | a3 | a2 | a1 | a0 | qC1_2
qC0_0 !a15 & !a14 & !a13 & !a12 & !a11 & !a10 & !a9 & !a8 & !a7 & !a6 & a5 & a4 & !a3 & !a2 & !a1 & (!a0 | a0) & qC0_1
