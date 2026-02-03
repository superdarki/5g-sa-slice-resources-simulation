# 5g-sa-slice-resources-simulation
Resource block allocation simulation for 5G sliced networks (URLLC vs eMBB).

Most of the work went into `UR3.c`, which implements the Markov-chain model.
`sim.c` is a proof of concept for a "real" simulation that avoids mathematical
assimilation and uses direct simulation logic instead; it has not been tested 
nor used.

`UR3.c` simulates a continuous-time Markov chain of slice occupancy with
arrivals (URLLC/eMBB), service rates, and a guard-channel threshold. It sweeps
eMBB arrival rates, estimates blocking/loss and queueing metrics, and writes a
CSV report.

Usage (UR3.c):
```bash
cc -O2 UR3.c -o UR3 -lm
./UR3 <S>
```
`S` is the total number of resource blocks. The run generates `S(<S>).csv`
containing per-load metrics and a timing footer. If you want to change the
arrival/service parameters or sweep range, edit the constants at the top of
`UR3.c`.
