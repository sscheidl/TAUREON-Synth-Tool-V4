# Stage 5 Block-B Results - 2026-09-09

## Environment

- Windows display: 1920x1200, native 125% scaling, DPR 1.25.
- Qt: 6.10.3, Windows platform plugin.
- Windows MIDI Services Console: 1.0.17-rc.4.25.
- Build: fresh VS 2022 x64 Debug tree with local MIDI integration tests enabled.
- MIDI path: temporary, uniquely named WMS loopback endpoints only; no physical device.

## Product-host lifecycle

The test constructs the production `QApplication`, `MainWindow`, `ConnectionWorker`,
and native transport factory. It executes 20 complete create/connect/disconnect/destroy
cycles per backend, then closes once during active receive and once during a paced,
partially completed 256-frame Raw Send.

```json
{"event":"stage5_product_host","backend":"wms","cycles":20,"tx":1,"rx_callbacks":1,"dropped":0,"late":0,"queue_high_water":1,"max_shutdown_ms":58,"steady_handle_min":461,"steady_handle_max":463,"steady_handle_slope":-0.193939,"sustained_handle_growth":false}
{"event":"stage5_product_host","backend":"winmm","cycles":20,"tx":1,"rx_callbacks":1,"dropped":0,"late":0,"queue_high_water":2,"max_shutdown_ms":23,"steady_handle_min":503,"steady_handle_max":503,"steady_handle_slope":0,"sustained_handle_growth":false}
```

The temporary endpoint pair was removed in the wrapper's `finally` path and confirmed
absent through both WMS and WinMM enumeration.

The final combined local suite passed 6/6 in 341.55 seconds. The final complete
non-local suite passed 28/28 in 9.53 seconds.

## Visual sizing

Before the responsive-layout correction, the actual Windows host reported a
`MainWindow::minimumSizeHint()` of 1170x903 logical pixels. The native 125% capture was
therefore 1920x1129 instead of the requested 1920x1080. After the correction, the
minimum is 758x419 and all three capture sets are exactly 1920x1080.

| Run | Evidence class | DPR | Requested logical | Actual logical | Capture |
|---|---|---:|---:|---:|---:|
| Windows 125% | native | 1.25 | 1536x864 | 1536x864 | 1920x1080 |
| Qt effective 150% | simulated | 1.5 | 1280x720 | 1280x720 | 1920x1080 |
| Qt effective 200% | simulated | 2.0 | 960x540 | 960x540 | 1920x1080 |

All seven workspaces were inspected in each set. Settings and SysEx Transfer use
scrollable workspaces at constrained heights; transfer and manager actions use wrapped
button grids. No status-text overlap remains. Native Windows inspection at 150% and
200% is still required before the visual gate can close.
