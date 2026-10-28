# Passive native-feedback audit

Requires Python with `pefile` and `unicorn`. Supply an authorized engine image;
no executable, raw dump, proprietary sprite or original code bytes are included.

```sh
python actions.py --game /path/to/authorized-engine.exe --output /tmp/actions.json
python placement.py --game /path/to/authorized-engine.exe --output /tmp/placement.json
```

`actions.py` executes the original Tactical selection/option gate and Foot
action-line timer, target, last-navigation and palette-color path. FLH,
target-coordinate and endpoint virtuals are explicitly controlled boundary
doubles. It proves these gates and argument selection, not native geometry or
planning-token output.

`placement.py` executes original placement frame/anchor selection, the Centered
and frame-crop coordinate slices, and native foundation mark/unmark. Eligibility,
shroud, type kind, terrain height and final draw are controlled boundaries. It
does not certify actual placement legality. A pre-existing active-draw jump to
an all-zero offline continuation is reported BLOCKED; original instructions are
never copied from another executable to make that path pass.

Public JSON records only supported sample hashes, addresses, synthetic inputs,
outputs, case results and blocking facts. It is not a live game, driver or
multiplayer test.
