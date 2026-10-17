#!/usr/bin/env python3
"""Compile/run the portable regression tests with g++ (no Windows required)."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent
tests = sorted(p for module in ('assist', 'unit_monitor')
               for p in (root / module / 'tests').glob('*.cpp')
               if not p.stem.endswith('_bench'))

with tempfile.TemporaryDirectory(prefix='ra2-logic-') as temp:
    def check(source):
        binary = Path(temp) / (source.parent.parent.name + '-' + source.stem)
        build = subprocess.run(['g++', '-O2', '-std=c++17', str(source), '-o', str(binary)],
                               capture_output=True, text=True)
        run = build if build.returncode else subprocess.run([str(binary)], capture_output=True, text=True)
        return str(source.relative_to(root)), run.returncode, run.stdout + run.stderr

    results = list(ThreadPoolExecutor(max_workers=4).map(check, tests))
    source_contracts = (
        'no_interception_source_contract.py',
        'persistent_ui_source_test.py',
        'ui_geometry_test.py',
        'modern_ui_test.py',
        'autonomy_source_contract.py',
        'tech_prerequisite_audit_test.py',
        'tech_runtime_source_contract.py',
        'unified_gateway_contract.py',
        'mega_queue_contract.py',
        'object_event_contract.py',
        'constructor_separation_contract.py',
        'outgoing_source_contract.py',
        'host_link_timeline.py',
        'snapshot_publication_test.py',
        'architecture_boundaries_test.py',
        'freshness_source_contract.py',
        'extended_boundary_contract.py',
        'host_input_test.py',
        'automatic_log_test.py',
        'manifest_loader_contract.py',
        'auto_settings_ui_test.py',
        'cursor_settings_ui_test.py',
        'super_binding_ui_test.py',
    )
    for name in source_contracts:
        source_test = root / 'assist/tests' / name
        run = subprocess.run([sys.executable, str(source_test)], capture_output=True, text=True)
        results.append((str(source_test.relative_to(root)), run.returncode, run.stdout + run.stderr))
    for name in ('winmock_check.py','unified_reader_check.py'):
        mock = root/'tests'/name
        check = subprocess.run([sys.executable, str(mock)], capture_output=True, text=True)
        results.append((str(mock.relative_to(root)), check.returncode, check.stdout + check.stderr))
    for name, code, output in results:
        print(name + ': ' + ('PASS' if code == 0 else 'FAIL'))
        if output:
            print(output.rstrip())
    print(f'TOTAL: {len(tests)} native tests + {len(source_contracts)} source/UI checks + 2 mocked Win32 read-path tests')
    sys.exit(any(code for _, code, _ in results))
