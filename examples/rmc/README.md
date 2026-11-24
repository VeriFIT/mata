# Regular model checking

[`armc.matasc`](armc.matasc) is abstract regular model checking (Bouajjani, Habermehl, Vojnar, CAV 2004) as a Mata
Script. A configuration of a parameterised system is a word with one letter per process, the transition relation a
length-preserving two-level transducer. The script over-approximates the reachable configurations with
`nfa:collapse-by-length`, checks them against the bad configurations lazily, decides with a backward search whether a
bad configuration it meets is really reachable, and refines a spurious one by a larger length.

| System                                     | Letters             | Result                      |
| ------------------------------------------ | ------------------- | --------------------------- |
| [`token-passing.mata`](token-passing.mata) | `t` token, `n` none | safe, length 1              |
| [`token-copying.mata`](token-copying.mata) | as above            | unsafe, steps: 1, length 1  |
| [`burns.mata`](burns.mata)                 | `0`–`5`, see below  | safe, length 1              |
| [`burns-broken.mata`](burns-broken.mata)   | as above            | unsafe, steps: 10, length 1 |

Run, from the repository root:

```sh
mata script run --param 'initial-regex=tn*' --param 'bad-regex=n*tn*' --param 'symbols=n t' \
  examples/rmc/armc.matasc examples/rmc/token-passing.mata
mata script run --param 'initial-regex=0*' --param 'bad-regex=[0-4]*(5[0-4]*)?' --param 'symbols=0 1 2 3 4 5' \
  examples/rmc/armc.matasc examples/rmc/burns.mata
```

## Burns' mutual exclusion

Each process runs Burns' one-bit algorithm. One process moves per step, and a scan of the other flags is atomic. The
letters are the program counters: `0` idle, `1` scan the left processes before raising the flag, `2` raise it, `3`
scan the left processes again, `4` wait for the right processes, `5` critical. The flag is up in `3`, `4` and `5`.

| From | To  | Condition                                |
| ---- | --- | ---------------------------------------- |
| `0`  | `1` | always                                   |
| `1`  | `0` | some process on the left has its flag up |
| `1`  | `2` | no process on the left has its flag up   |
| `2`  | `3` | always                                   |
| `3`  | `0` | some process on the left has its flag up |
| `3`  | `4` | no process on the left has its flag up   |
| `4`  | `5` | no process on the right has its flag up  |
| `5`  | `0` | always                                   |

Initially every process is idle (`0*`); a configuration with two processes in `5` is bad. `burns-broken.mata` drops
the condition of `4` to `5`.
