# Branches

Notable branches in Mata.

## Main branches

### [`master`](https://github.com/VeriFIT/mata/tree/master)

The production release branch of Mata.

### [`devel`](https://github.com/VeriFIT/mata/tree/devel)

The developement branch of Mata, providing development releases, used to quickly iterate on new features in Mata without
breaking the userland during development.
`devel` is always kept in-sync with `master`.

## Backups

Backed-up branches with experiments.

### [`simulation-samo538.bak`](https://github.com/VeriFIT/mata/tree/simulation-samo538.bak)

- Author: [Samuel Lupták (samo538)](https://github.com/samo538)
- As part of: [Project Practice 1, BIT](https://www.fit.vut.cz/study/course/IP1/.en)
- From: <https://github.com/VeriFIT/mata/pull/434>

Backup of a NFA reduction algorithm using maximum direct simulation.
Pseudo-algorithm is taken from <https://github.com/ondrik/iny-fix/blob/master/main.pdf>.
The experimental results show the algorithm is not outperforming the already existing RT algorithm for simulations.

### [`simulation-reduction-rules-samo538.bak`](https://github.com/VeriFIT/mata/tree/simulation-reduction-rules-samo538.bak)

- Author: [Samuel Lupták (samo538)](https://github.com/samo538)
- As part of: [Project Practice 2, BIT](https://www.fit.vut.cz/study/course/IP2/.en)
- From: <https://github.com/VeriFIT/mata/pull/487>

Backup of a NFA reduction algorithm enhancement using maximum direct simulation with support for reduction rules.

### [`boost-determinization-inclusion-lacko.bak`](https://github.com/VeriFIT/mata/tree/boost-determinization-inclusion-lacko.bak)

- Author: [Igor Lacko (Igor-Lacko)](https://github.com/Igor-Lacko)
- As part of: [Project Practice 1, BIT](https://www.fit.vut.cz/study/course/IP1/.en)
- From: <https://github.com/VeriFIT/mata/pull/478>

Backup of a variant of determinization and inclusion algorithms using the Boost data structures (bit vectors).
Requires Boost as an external dependency.
The Boost implmentation of bit vectors can be replaced with a manual implementation of
