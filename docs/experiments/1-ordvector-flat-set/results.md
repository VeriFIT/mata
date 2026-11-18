# EXP-1 results

Summary tables of the measurements, produced by the scripts in [`tools/`](tools/); see the [record](README.md) for the setup and how to reproduce them.

## End-to-end: TACAS'24 Mata experiments

Ratios against `devel` (lower is faster), over the instances where every build finished every repeat and the `devel` time is at least 1 ms: the geometric mean, and in parentheses (`tot`) the ratio of the total times of that same population. `devel-aa` is a byte copy of the `devel` binary: its spread is the noise floor. Output of [`tools/analyse.py`](tools/analyse.py).

### op: ratio vs devel, instances with devel op >= 1 ms: geomean (tot = ratio of totals)

| experiment           | n    | devel total s | devel-aa          | flatset-merge     | backmerge         | append-fastpath   | branchless-lb     | delta-pool        | stateset-pool     |
| -------------------- | ---- | ------------- | ----------------- | ----------------- | ----------------- | ----------------- | ----------------- | ----------------- | ----------------- |
| armc-incl            | 101  | 8.61          | 0.991 (tot 0.990) | 0.993 (tot 0.987) | 0.995 (tot 0.986) | 1.003 (tot 0.982) | 0.996 (tot 0.982) | 0.960 (tot 0.983) | 0.963 (tot 0.975) |
| b-smt                | 8    | 0.02          | 0.941 (tot 0.962) | 0.955 (tot 0.952) | 0.965 (tot 0.981) | 0.899 (tot 0.917) | 0.924 (tot 0.958) | 0.889 (tot 0.911) | 0.857 (tot 0.896) |
| email-filter         | 199  | 0.73          | 0.989 (tot 1.001) | 0.986 (tot 1.000) | 0.988 (tot 0.988) | 0.981 (tot 1.001) | 0.984 (tot 0.999) | 0.908 (tot 0.901) | 0.915 (tot 0.919) |
| lia-explicit         | 126  | 1.05          | 0.995 (tot 0.998) | 0.998 (tot 1.016) | 1.001 (tot 0.992) | 1.004 (tot 1.001) | 1.004 (tot 0.999) | 1.004 (tot 0.943) | 1.006 (tot 0.943) |
| lia-symbolic         | 111  | 0.93          | 0.886 (tot 0.928) | 0.883 (tot 1.027) | 0.869 (tot 0.903) | 0.869 (tot 0.972) | 0.869 (tot 0.929) | 0.848 (tot 0.880) | 0.829 (tot 0.905) |
| noodler-compl        | 3    | 0.00          | 0.914 (tot 0.910) | 0.919 (tot 0.933) | 0.900 (tot 0.901) | 0.905 (tot 0.906) | 0.893 (tot 0.927) | 1.039 (tot 1.042) | 0.995 (tot 0.997) |
| noodler-compl-big    | 173  | 3.40          | 0.987 (tot 1.000) | 0.991 (tot 0.980) | 0.971 (tot 1.009) | 0.982 (tot 0.992) | 1.008 (tot 0.984) | 1.013 (tot 0.976) | 0.987 (tot 0.971) |
| noodler-concat       | 300  | 10.83         | 0.989 (tot 1.002) | 0.993 (tot 0.999) | 0.990 (tot 1.000) | 0.988 (tot 1.006) | 0.991 (tot 1.003) | 0.822 (tot 0.959) | 0.824 (tot 0.954) |
| noodler-inter        | 1304 | 6.51          | 0.965 (tot 0.999) | 0.968 (tot 1.004) | 0.966 (tot 0.995) | 0.977 (tot 0.986) | 0.973 (tot 0.985) | 1.047 (tot 0.993) | 1.043 (tot 1.012) |
| param-inter          | 76   | 14.79         | 1.013 (tot 1.015) | 1.000 (tot 0.991) | 1.000 (tot 1.001) | 1.001 (tot 0.998) | 1.012 (tot 1.004) | 0.810 (tot 0.831) | 0.807 (tot 0.827) |
| param-inter-variadic | 25   | 36.85         | 0.999 (tot 0.972) | 1.010 (tot 0.945) | 1.016 (tot 0.974) | 0.998 (tot 0.983) | 1.001 (tot 0.963) | 0.756 (tot 0.830) | 0.751 (tot 0.829) |
| param-union          | 56   | 0.16          | 0.989 (tot 0.990) | 0.994 (tot 0.994) | 0.990 (tot 0.992) | 0.991 (tot 0.992) | 0.992 (tot 0.992) | 0.893 (tot 0.909) | 0.888 (tot 0.905) |
| param-union-variadic | 155  | 15.33         | 0.995 (tot 0.997) | 0.996 (tot 1.000) | 0.994 (tot 0.990) | 0.998 (tot 1.001) | 1.005 (tot 0.995) | 0.999 (tot 1.004) | 0.999 (tot 0.991) |
| regexps-det-min      | 19   | 17.90         | 1.022 (tot 1.025) | 1.005 (tot 1.015) | 1.005 (tot 0.976) | 0.997 (tot 1.042) | 1.008 (tot 0.976) | 0.998 (tot 0.988) | 1.008 (tot 0.994) |
| **all**              | 2656 |               | 0.974             | 0.976             | 0.973             | 0.979             | 0.979             | 0.975             | 0.971             |

### overall: ratio vs devel, instances with devel overall >= 1 ms: geomean (tot = ratio of totals)

| experiment           | n    | devel total s | devel-aa          | flatset-merge     | backmerge         | append-fastpath   | branchless-lb     | delta-pool        | stateset-pool     |
| -------------------- | ---- | ------------- | ----------------- | ----------------- | ----------------- | ----------------- | ----------------- | ----------------- | ----------------- |
| armc-incl            | 136  | 13.38         | 0.997 (tot 0.993) | 1.000 (tot 0.989) | 1.003 (tot 0.991) | 1.012 (tot 0.992) | 1.003 (tot 0.986) | 0.996 (tot 0.990) | 0.998 (tot 0.984) |
| b-smt                | 384  | 0.78          | 0.991 (tot 0.992) | 0.989 (tot 0.989) | 0.985 (tot 0.986) | 0.997 (tot 0.997) | 0.988 (tot 0.988) | 0.965 (tot 0.965) | 0.956 (tot 0.955) |
| email-filter         | 500  | 4.75          | 0.998 (tot 1.000) | 0.998 (tot 1.003) | 0.997 (tot 0.996) | 0.993 (tot 0.995) | 1.001 (tot 0.998) | 0.990 (tot 0.982) | 0.994 (tot 0.992) |
| lia-explicit         | 145  | 3.63          | 0.995 (tot 0.991) | 0.999 (tot 1.000) | 0.999 (tot 0.987) | 1.003 (tot 0.991) | 1.000 (tot 0.987) | 1.001 (tot 0.969) | 1.002 (tot 0.982) |
| lia-symbolic         | 169  | 11.46         | 0.998 (tot 0.957) | 1.003 (tot 0.953) | 0.995 (tot 1.006) | 0.988 (tot 0.987) | 0.988 (tot 0.953) | 0.962 (tot 0.968) | 0.985 (tot 0.949) |
| noodler-compl        | 22   | 0.04          | 0.980 (tot 0.999) | 0.986 (tot 0.997) | 0.973 (tot 0.984) | 0.986 (tot 0.994) | 0.968 (tot 0.984) | 0.992 (tot 1.003) | 1.015 (tot 1.023) |
| noodler-compl-big    | 236  | 14.79         | 0.970 (tot 0.995) | 0.958 (tot 0.968) | 0.973 (tot 1.023) | 0.985 (tot 0.999) | 0.963 (tot 0.960) | 0.949 (tot 0.971) | 0.971 (tot 0.954) |
| noodler-concat       | 301  | 10.90         | 0.990 (tot 1.002) | 0.993 (tot 0.999) | 0.990 (tot 1.000) | 0.987 (tot 1.006) | 0.991 (tot 1.004) | 0.824 (tot 0.959) | 0.827 (tot 0.955) |
| noodler-inter        | 3467 | 29.67         | 0.987 (tot 0.997) | 0.990 (tot 1.000) | 0.992 (tot 1.010) | 0.991 (tot 0.992) | 0.991 (tot 0.995) | 1.014 (tot 1.002) | 1.021 (tot 1.003) |
| param-inter          | 80   | 16.02         | 1.011 (tot 1.014) | 1.001 (tot 0.992) | 0.999 (tot 1.001) | 1.000 (tot 0.999) | 1.010 (tot 1.004) | 0.845 (tot 0.843) | 0.844 (tot 0.841) |
| param-inter-variadic | 32   | 36.96         | 1.010 (tot 0.972) | 1.023 (tot 0.945) | 1.024 (tot 0.974) | 1.006 (tot 0.983) | 1.001 (tot 0.963) | 0.832 (tot 0.830) | 0.829 (tot 0.830) |
| param-union          | 80   | 1.39          | 1.003 (tot 1.003) | 1.005 (tot 1.004) | 0.998 (tot 0.995) | 1.006 (tot 1.004) | 1.005 (tot 1.002) | 0.996 (tot 0.997) | 0.992 (tot 0.987) |
| param-union-variadic | 187  | 23.39         | 0.999 (tot 0.999) | 0.999 (tot 1.000) | 0.997 (tot 0.994) | 0.997 (tot 1.002) | 0.999 (tot 0.996) | 0.998 (tot 1.003) | 0.997 (tot 0.993) |
| regexps-det-min      | 19   | 18.43         | 1.025 (tot 1.025) | 1.007 (tot 1.015) | 1.005 (tot 0.978) | 0.995 (tot 1.041) | 1.011 (tot 0.979) | 0.997 (tot 0.989) | 1.010 (tot 0.995) |
| **all**              | 5758 |               | 0.989             | 0.991             | 0.992             | 0.992             | 0.992             | 0.988             | 0.994             |

### failures (exp, build, statuses): instances

### result disagreements with the first repeat of devel, over every repeat

none

### instances whose result was compared, per experiment

{'armc-incl': 136, 'b-smt': 384, 'param-inter': 80, 'param-inter-variadic': 32, 'param-union': 80, 'param-union-variadic': 187, 'email-filter': 500, 'lia-explicit': 169, 'lia-symbolic': 169, 'noodler-compl': 438, 'noodler-compl-big': 313, 'noodler-concat': 438, 'noodler-inter': 4872}

### instances whose program prints no result (not compared), per experiment

{'regexps-det-min': 25}

### skipped after the baseline probe failed (exp, status): instances

{('regexps-det-min', 'ERR255'): 18, ('regexps-det-min', 'TO'): 9, ('param-inter-variadic', 'TO'): 41, ('param-inter-variadic', 'ERR255'): 114, ('param-diff', 'ERR255'): 80}

## Microbenchmark

[`tools/micro.cc`](tools/micro.cc) against the headers of `53021908`, `g++ 16.2 -std=c++23 -O3 -DNDEBUG`, pinned to one core, minimum of 5 timed repeats. In the `build` rows the time is divided by the number of inserts actually performed.

The `contains, size 65536` row is not stable between runs of the same binary: over seven runs `OrdVector` (`std::lower_bound`) took 14 to 34 ns, `std::flat_set` 49 to 58 ns, and the branchless search 18.4 to 23 ns. The other rows varied by a few percent.

```text
ns/op (min of 5)                             OrdVector flat_set  inplmerge backmerge backshift
union 4 += 4 (overlap)                           33.32     55.82     61.78     47.98     40.35
union 16 += 16 (overlap)                         56.87     92.91     98.57    110.81     69.31
union 64 += 8 (sparse)                           95.60    150.33    157.29    137.86    101.43
union 256 += 256                               2056.71   2693.96   2740.11   4525.16   2025.41
union 1024 += 16                               1274.68   1794.40   1963.61   1626.72   1337.46
union 16 += 1024                               1180.19   1515.59   1743.62   5578.67   2612.01
union 1000 += subset                            785.21   1242.26   1676.49    535.33    812.93
build 16 random (3rd col: append fast path)      10.13     10.25     10.75      0.00      0.00
build 256 random (3rd col: append fast path)     15.53     15.99     16.63      0.00      0.00
build 4096 random (3rd col: append fast path)    141.66    140.91    140.93      0.00      0.00
build 256 ascending (3rd col: append fast path)      7.05      6.95      1.97      0.00      0.00
build 4096 ascending (3rd col: append fast path)      9.27      9.46      1.56      0.00      0.00
contains, size 8 (3rd col: branchless)            3.95      3.90      4.38      0.00      0.00
contains, size 64 (3rd col: branchless)           5.88      5.99      6.12      0.00      0.00
contains, size 1024 (3rd col: branchless)         8.98     10.80      9.40      0.00      0.00
contains, size 65536 (3rd col: branchless)       14.33     56.32     18.53      0.00      0.00
```

## Profile of `devel`

Share of samples (%) whose innermost (inlined) frame is in the given source file; `perf record` at 2 kHz over 40 sampled instances per experiment, `perf report --sort srcfile` (see [`tools/shares.sh`](tools/shares.sh)).

| experiment      | ord-vector.hh | small-ord-vector.hh | stl_algo.h | stl_algobase.h | vector.tcc | stl_vector.h | synchronized-iterator.hh | sparse-set.hh | hashtable.h |
| --------------- | ------------- | ------------------- | ---------- | -------------- | ---------- | ------------ | ------------------------ | ------------- | ----------- |
| armc-incl       | 0.04          | 0.57                | 2.78       | 1.02           | 7.55       | 4.10         | 13.31                    | 0.00          | 4.51        |
| b-smt           | 0.00          | 1.07                | 0.23       | 0.00           | 0.86       | 3.73         | 0.00                     | 0.19          | 1.08        |
| email-filter    | 0.00          | 0.43                | 0.76       | 0.20           | 8.08       | 1.86         | 0.00                     | 0.00          | 2.02        |
| lia-explicit    | 0.00          | 0.76                | 2.15       | 0.64           | 6.70       | 4.55         | 0.00                     | 0.07          | 2.15        |
| lia-symbolic    | 0.00          | 0.00                | 0.02       | 0.33           | 17.50      | 0.99         | 0.00                     | 0.00          | 0.41        |
| noodler-compl   | 0.00          | 0.00                | 0.00       | 0.03           | 0.73       | 0.00         | 0.00                     | 0.00          | 0.70        |
| noodler-concat  | 0.06          | 6.99                | 0.00       | 1.68           | 0.26       | 4.68         | 0.00                     | 0.56          | 0.00        |
| noodler-inter   | 0.00          | 0.58                | 0.18       | 0.28           | 2.60       | 1.20         | 0.00                     | 0.00          | 0.77        |
| param-diff      | 0.01          | 0.17                | 1.16       | 0.41           | 6.50       | 13.90        | 0.00                     | 0.00          | 0.00        |
| param-inter     | 0.00          | 5.06                | 0.03       | 0.11           | 2.87       | 6.34         | 0.00                     | 2.67          | 0.11        |
| param-union     | 0.00          | 0.13                | 0.07       | 0.14           | 13.02      | 2.14         | 0.00                     | 0.00          | 2.22        |
| regexps-det-min | 0.00          | 3.66                | 0.20       | 0.73           | 6.36       | 15.09        | 0.00                     | 0.05          | 0.62        |
