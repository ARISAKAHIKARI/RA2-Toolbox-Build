# Controlled ordinary CnCNet rendering probes

Dependencies: Python 3 with `pefile` and `unicorn`; the repository fixture imports its other normal Python dependencies. These scripts import `source/assist/tests/entry_runtime_regression.py` from `--repo` and execute actual x86 instructions with synthetic world/Win32 inputs. No real graphics device, Syringe startup scheduling, driver, two-player network or gameplay is exercised.

Both scripts require the official ordinary CnCNetYR v0.0.0.17 `CnCNet-Spawner.dll` SHA256 `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d`. This requirement is enforced because the clamp target and logging-counter RVA are tied to that binary. Supplied game and Runtime binary hashes are recorded in each output.

Actual released GameRuntime.dll used for the audit: SHA256 `eba0ce9f13376aa193942a4765b07fc28e2f63d7a577bed9450325d8e951f4f7`.

Run each game separately:

```sh
python smallmap_ordinary_runtime.py --repo /repo/RA2-Toolbox-Build --game /authorized/game.exe --runtime /release/GameRuntime.dll --spawner /official/CnCNet-Spawner.dll --output /results/smallmap.json
python fp_ordinary_runtime.py --repo /repo/RA2-Toolbox-Build --game /authorized/game.exe --runtime /release/GameRuntime.dll --spawner /official/CnCNet-Spawner.dll --output /results/fp.json
```

Authorized game sample hashes checked during the audit:

| Sample | SHA256 |
| --- | --- |
| LBW sample (3) | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| CnCNet sample (4) | `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |

The clamp probe maps the real ordinary DLL at relocated base `0x18000000`, applies its public CRT-resolved `0x6D8640` jump, calls the actual clamp through that game address, checks the point, bool in AL and `ret 4` ABI, then verifies Runtime ticks and stop do not write the owned hook. It tests initial large/small maps and repeated geometry changes. The ordinary constructor was independently executed to resolve this generated patch; this probe itself does not run DLL startup constructors.

The FP probe calls both real ordinary FP repair exports before and after Runtime callbacks across six MXCSR inputs. The logger's rate-limit counter is initialized to its natural cap of 10 to avoid an uninitialized ordinary CRT logger. It verifies scoped Runtime normalization, caller restoration, chained-hook state and preservation of ordinary repair. It does not prove arbitrary nested dispatch or GPU-driver behavior.
