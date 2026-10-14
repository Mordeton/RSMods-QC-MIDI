# QC regression checks

Run `powershell -ExecutionPolicy Bypass -File tests/run-qc-regression.ps1` from the RSMods repository. Requires Visual Studio C++ tools. The executable and object file go to the system temporary directory unless `-OutputDirectory` is supplied.

The harness compiles the actual `QCAutomation.cpp` runtime with offline substitutes for game state, tuning reads, configuration, tone-source results, and MIDI delivery. It tests E-standard and lowered tunings, unavailable reads, live transpose enable transitions, device/channel/controller changes, failed preset and forced-scene sends, steady-state deduplication, manual target selection, return to automatic targets, and idle behavior.

It does not verify Windows memory access, the tone-source keyboard hook, USB delivery, or Quad Cortex preset loading latency. Verify these with the installed game and hardware. The production build does not define `QC_AUTOMATION_TEST` and uses its normal dependencies.
