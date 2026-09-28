# Stage 5 Block-B Windows Evidence - 2026-09-09

This directory retains the actual `MainWindow` workspace captures used for the local
Windows visual review. Every set contains all seven workspaces at a 1920x1080 physical
capture target.

- `native-125/`: native Windows desktop scaling, DPR 1.25.
- `simulated-150/`: process-local Qt scale multiplier, effective DPR 1.5.
- `simulated-200/`: process-local Qt scale multiplier, effective DPR 2.0.

Only the 125% set is native Windows display-scaling evidence. The 150% and 200% sets
are useful responsive-layout evidence, but they do not close the native visual gate.
No physical MIDI endpoint was selected while producing these captures.

See `RESULTS.md` for the measured window sizes and native WMS/WinMM lifecycle results.
`local-midi-ctest.txt` retains the final combined 6/6 local test summary.
