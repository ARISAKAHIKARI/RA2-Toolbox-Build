# Loaded passenger admission and Host exception boundaries (pre.8)

This is an audit of toolbox source behavior, not new engine reverse engineering.
All game field offsets and mission/virtual-call conventions remain unchanged.
No supported game executable hash or new engine ABI is asserted by these tests.

## Ctrl+D admitted without a passenger continuation

The supplied pre.7 log contains six command 17 submissions and six successful
acknowledgements with count=1. The user reports passengers leave the vehicle but
do not enter a building. Those acknowledgements establish carrier admission,
not a successful passenger follow-up or a successful infiltration.

`own()` in `bridge.cpp` includes `live()`: positive health, active, not limbo,
and on-map. Both `buildUnloadAdmissionRoster` and `smartUnloadNow` used it on
loaded passengers. A loaded/limbo passenger could be excluded while Deploy was
still sent, so no `UnloadPassenger` job existed when the passenger landed.
Native-first delivery can instead admit that passenger from the existing short
history after it becomes live. This explains a source-level timing dependency;
the log alone does not identify which exact passenger was excluded.

Admission now checks indexed identity, owner, positive health, active state and
infantry kind separately. Map presence is required after leaving the carrier.
Carrier identity, passenger token, filters, capacity and event delivery remain
checked. There are no mission/target memory writes or new sampling traversals.

`unload_loaded_admission.py` extracts actual production live/own, roster and
admission bodies. Current code retains a loaded passenger before Deploy;
frozen pre.7 fails `loaded passenger lost before Deploy`. Other cases cover
native-first history, owner/life/filter/carrier/kind/identity/manual cancellation
and duplicate roster. Synthetic object reads/native delivery do not demonstrate
actual engine execution, infiltration success or netplay behavior.

## Host exceptions

The supplied logs do not contain the reported toolbox crash. A separate actual
source defect exists: a throwing timing sink/format allocation can escape an
implicitly noexcept timing destructor and terminate the EXE, including while an
original exception is unwinding. `pipeline_timing_failure.py` executes current
code and frozen pre.7 (terminate exit 86); current code preserves the original
exception, retains aggregates, and bounds failed reports to one per five seconds.

The EXE-owned compositor catches allocation/other C++ exceptions per iteration,
clears partial output, and retries after an interruptible 100ms deadline. It
retains immutable producer layers and records bounded diagnostics. It does not
catch SEH access violations or claim corrupted driver state can be recovered.
`overlay_exception_windows.py` runs the actual HWND/thread code with injected
allocation, diagnostics and upload failures, and a frozen pre.7 termination
control. This is Windows execution, not game/DWM performance evidence.

`host_crash.h` is installed only by the two EXE console entry variants before
workers start. It prepares file handles/paths and system dbghelp during startup;
fatal recording avoids the normal logger/locks and uses a small Host-only dump.
`host_crash_windows.py` checks native access violation and unhandled C++ worker
child processes, PID/thread/code/module recording and MDMP thread streams.
No game dump, suspended game thread, automatic upload or fault-recovery claim.

The console embeds the existing canonical `app-icon.png`; Windows Edge checks
desktop/narrow rendering. No application artwork regeneration is performed.
