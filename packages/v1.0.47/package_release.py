#!/usr/bin/env python3
"""Package the frozen 1.0.47-pre.3 build; never build or publish.

Use the verified 1.0.46.4 originals, themes, licenses and seven-file engine.
Only prepare_runtime.py may change, with a current dependency check.
Bind all current outputs and executed checks to one frozen source tree.
No optional Python runtime or generated cursor cache ships.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('ra2_146_product_policy',
                                             HERE.parent / 'v1.0.46/package_release.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
require, sha, encoded = POLICY.require, POLICY.sha, POLICY.encoded

VERSION, FILE_VERSION = '1.0.47-pre.3', '1.0.47.3'
ASSIST_PROTOCOL = 102
BASE_SHARED_BYTES, SHARED_BYTES = 23162528, 23162544
BASE_NAME = 'RA2Toolbox-1.0.46-x64-Full.zip'
BASE_BYTES = 4428436
BASE_SHA = 'd8d5d871e0358579b49c1a2dedbfca48c601ea22119bf897e18593ae7233533c'
BASE_MANIFEST_BYTES = 61217
BASE_MANIFEST_SHA = '1d30a70514dcba77b26a330db130150850fe24c6fd962c513212542db10dc26b'
BASE_COMMIT = 'f452d1cb95c57f465356b55a3d7b83fb7403a70a'
APPROVED_ENGINE_CHANGES = frozenset({'engine/prepare_runtime.py'})
CHECK_BINDINGS = {
    'missile_allocator': (
        {'source/assist/tests/missile_allocation_equivalence.cpp'},
        {'source/assist/source/missile_compute.h', 'source/assist/source/missile_bridge.h',
         'source/assist/source/main.cpp'}),
    'snapshot_reuse': (
        {'source/assist/tests/world_frame_reuse.cpp', 'source/assist/tests/world_frame_reuse.py'},
        {'source/shared/source/world_frame.h', 'source/shared/source/world_frame_pool.h',
         'source/shared/source/world_frame_transport.h'}),
    'tech_capture': (
        {'source/assist/tests/tech_enhancement_capture.cpp',
         'source/assist/tests/tech_enhancement_capture.py'},
        {'source/assist/source/tech_ui_breakpoint_bridge.h',
         'source/assist/source/tech_enhancement.h', 'source/assist/source/tech_parameter_lookup.h'}),
    'voxel_recovery': (
        {'source/assist/tests/voxel_part_capture.cpp', 'source/assist/tests/voxel_part_capture.py'},
        {'source/external_reader/source/camera_assets.h', 'source/external_reader/source/radar_scene.h'}),
    'idle_aircraft_display': (
        {'source/assist/tests/idle_aircraft_display.cpp'},
        {'source/external_reader/source/idle_aircraft.h',
         'source/external_reader/source/tactical_sampler.h',
         'source/assist/source/aircraft_indicator.h', 'source/assist/source/main.cpp'}),
    'terrain_reuse': (
        {'source/assist/tests/terrain_storage_pool.cpp'},
        {'source/external_reader/source/radar_map.h'}),
    'console_receipt_order': (
        {'source/suite/tests/console_receipt_order_test.mjs'},
        {'source/suite/frontend/app.js'}),
    'catalogue_recovery': (
        {'source/unit_monitor/tests/catalogue_recovery.cpp',
         'source/unit_monitor/tests/round_transition.cpp'},
        {'source/unit_monitor/source/sampler.h', 'source/external_reader/source/layout_probe.h'}),
    'planner_batch_delivery': (
        {'source/assist/tests/planner_batch_delivery.cpp'},
        {'source/assist/source/external_planner.h', 'source/assist/source/planner_task.h',
         'source/assist/source/value_task.h', 'source/shared/source/latest_worker.h'}),
    'scene_extra_recovery': (
        {'source/assist/tests/scene_extra_recovery.cpp',
         'source/assist/tests/scene_extra_recovery.py', 'source/assist/tests/game_assets_host_test.py'},
        {'source/external_reader/source/scene_extra_layers.h',
         'source/external_reader/source/camera_assets.h', 'source/external_reader/source/camera_raster.h',
         'source/external_reader/source/radar_scene.h', 'source/external_reader/source/radar_decoration.h',
         'source/external_reader/source/world_scene_pipeline.h'}),
}
FUNCTIONAL_CHECKS = frozenset({'console_receipt_order', 'catalogue_recovery',
                             'planner_batch_delivery', 'scene_extra_recovery'})
FEATURE_BINDINGS = {
    'planner_defense_delivery': (
        {'source/assist/tests/planner_defense_delivery.cpp'},
        {'source/assist/source/external_planner.h', 'source/assist/source/planner_task.h',
         'source/assist/source/value_task.h', 'source/shared/source/latest_worker.h'}),
    'missile_manual_lifecycle': (
        {'source/assist/tests/missile_manual_lifecycle.py'},
        {'source/assist/source/missile_bridge.h'}),
    'missile_manual_capacity': (
        {'source/assist/tests/missile_manual_capacity.py'},
        {'source/assist/source/missile_bridge.h', 'source/assist/source/missile_compute.h',
         'source/assist/source/compute_frame.h', 'source/assist/source/compute_input_copy.h',
         'source/assist/source/compute_policy.h', 'source/assist/source/plan_freshness.h',
         'source/assist/source/planner_types.h', 'source/assist/source/object_index.h',
         'source/assist/source/unit_task_arbiter.h'}),
    'patrol_capacity': (
        {'source/assist/tests/mo_patrol_capacity.cpp'},
        {'source/assist/source/mo_orders.h', 'source/assist/source/mo_patrol_graph.h'}),
    'patrol_small_buffer': (
        {'source/assist/tests/mo_patrol_capacity.cpp'},
        {'source/assist/source/mo_orders.h', 'source/assist/source/mo_patrol_graph.h'}),
    'selection_catalogue_identity': (
        {'source/assist/tests/selection_catalogue_identity.py'},
        {'source/assist/source/main.cpp', 'source/assist/source/bridge.cpp',
         'source/assist/source/protocol.h'}),
    'toast_clock_wrap': (
        {'source/assist/tests/toast_clock_wrap.py'},
        {'source/assist/source/main.cpp'}),
    'console_composition': (
        {'source/suite/tests/console_composition_test.mjs'},
        {'source/suite/frontend/app.js'}),
    'console_hotkey_presentation': (
        {'source/suite/tests/console_hotkey_presentation_test.mjs'},
        {'source/suite/frontend/app.js'}),
    'secondary_drag_bounds': (
        {'source/unit_monitor/tests/secondary_drag_bounds.py'},
        {'source/unit_monitor/source/main.cpp'}),
    'secondary_render_retry': (
        {'source/unit_monitor/tests/secondary_render_retry.py'},
        {'source/unit_monitor/source/main.cpp'}),
    'icon_cache_reopen': (
        {'source/unit_monitor/tests/icon_cache_reopen.py'},
        {'source/unit_monitor/source/icon_cache.h', 'source/unit_monitor/source/core.h'}),
    'superweapon_result_feedback': (
        {'source/assist/tests/superweapon_result_feedback.py'},
        {'source/assist/source/main.cpp', 'source/assist/source/bridge.cpp',
         'source/assist/source/superweapon_sidebar_bridge.h'}),
    'pause_key_pairing': (
        {'source/assist/tests/pause_key_pairing.py', 'source/assist/tests/superweapon_result_feedback.py'},
        {'source/assist/source/pause_view_bridge.h', 'source/assist/source/native_message_policy.h',
         'source/game_runtime/source/runtime.cpp', 'source/assist/source/hotkey_routes.h',
         'source/shared/source/key_gesture.h', 'source/assist/source/pause_view_policy.h',
         'source/assist/source/pointer_intent.h'}),
    'native_mouse_pairing': (
        {'source/assist/tests/native_mouse_pairing.py', 'source/assist/tests/pause_key_pairing.py',
         'source/assist/tests/superweapon_result_feedback.py'},
        {'source/assist/source/native_message_policy.h', 'source/assist/source/pointer_host.h',
         'source/game_runtime/source/runtime.cpp', 'source/assist/source/pause_view_bridge.h',
         'source/assist/source/hotkey_routes.h', 'source/shared/source/key_gesture.h',
         'source/assist/source/pause_view_policy.h', 'source/assist/source/pointer_intent.h'}),
    'passenger_frame_sampling': (
        {'source/assist/tests/passenger_frame_sampling.py',
         'source/assist/tests/passenger_frame_sampling.cpp', 'source/assist/tests/world_frame_core.cpp',
         'source/assist/tests/world_frame_core_test.py', 'source/tests/winmock/windows.h'},
        {'source/assist/source/main.cpp', 'source/assist/source/passenger_draw.h',
         'source/assist/source/passenger_motion.h', 'source/assist/source/display_sample_lease.h',
         'source/assist/source/label_motion.h', 'source/external_reader/source/passenger_bubbles.h',
         'source/external_reader/source/game_view.h', 'source/external_reader/source/process_reader.h',
         'source/shared/source/world_frame.h', 'source/shared/source/world_frame_capture.h',
         'source/shared/source/world_frame_transport.h'}),
}
CHECK_BINDINGS.update(FEATURE_BINDINGS)
FEATURE_CHECKS = frozenset(FEATURE_BINDINGS)
DEPENDENCY_BINDINGS = (
    {'source/tools/test_dependency_probes.py'},
    {'source/engine/prepare_runtime.py', 'source/engine/runtime-dependencies.json'},
)
PACKAGE_INPUTS = (
    'packages/v1.0.43/build_release.py', 'packages/v1.0.46/package_release.py',
    'packages/v1.0.47/package_release.py', 'packages/v1.0.47/test_package_release.py',
    'packages/v1.0.47/README.txt', 'packages/v1.0.47/PACKAGING.md',
)


def pinned_base(archive_path, manifest_path):
    """Check the pinned public archive and manifest before using any resource."""
    archive_bytes, manifest_bytes = archive_path.read_bytes(), manifest_path.read_bytes()
    require(len(archive_bytes) == BASE_BYTES and sha(archive_bytes) == BASE_SHA,
            'Published 1.0.46.4 archive differs from its pinned digest')
    require(len(manifest_bytes) == BASE_MANIFEST_BYTES and sha(manifest_bytes) == BASE_MANIFEST_SHA,
            'Published 1.0.46.4 manifest differs from its pinned digest')
    manifest = json.loads(manifest_bytes)
    require((manifest.get('version'), manifest.get('file_version'), manifest.get('source_commit'))
            == ('1.0.46', '1.0.46.4', BASE_COMMIT), 'Published resource identity differs')
    matches = [row for row in manifest['packages'] if row['name'] == BASE_NAME]
    require(len(matches) == 1, 'Expected one published Full package')
    record = matches[0]
    require((record['bytes'], record['sha256']) == (BASE_BYTES, BASE_SHA),
            'Published archive manifest membership differs')
    resources = {}
    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
        require(archive.testzip() is None, 'Published base ZIP CRC failed')
        items = archive.infolist()
        names = [item.filename for item in items]
        require(len(names) == 53 and set(names) == set(record['files']),
                'Published base ZIP member set differs')
        POLICY.verify_product_members(names, full=True)
        for item in items:
            require(not item.is_dir() and not stat.S_ISLNK(item.external_attr >> 16),
                    'Directory or symbolic link in published base')
            data = archive.read(item)
            require(record['files'][item.filename] == {'bytes': len(data), 'sha256': sha(data)},
                    'Published base member digest differs: ' + item.filename)
            if POLICY.include_base_resource(item.filename):
                resources[item.filename] = data
    return resources, record, manifest


def functional_success_log(name, text):
    """Check the four pre.2 raw run logs, not a surrounding claimed result.

    Each frozen test has one terminal summary and an exact current check count.
    Reject concatenated runs, failure output, and receipt wrappers containing an
    unrelated passed flag. The caller separately binds exit code and all bytes.
    """
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if not lines or any(re.match(r'FAIL\b', line) for line in lines):
        return False
    if name in {'console_receipt_order', 'catalogue_recovery'}:
        try:
            result = json.loads(lines[-1])
        except (json.JSONDecodeError, TypeError):
            return False
        if not isinstance(result, dict):
            return False
        if name == 'console_receipt_order':
            return (len(lines) == 25 and len(set(lines[:-1])) == 24
                    and all(line.startswith('PASS ') for line in lines[:-1])
                    and set(result) == {'suite', 'checks', 'failures', 'passed'}
                    and result.get('suite') == name and result.get('passed') is True
                    and type(result.get('checks')) is int and result['checks'] == 24
                    and type(result.get('failures')) is int and result['failures'] == 0)
        expected = {'checks': 93, 'failures': 0, 'scenarios': 21, 'recovered_catalogues': 20}
        return (len(lines) == 1 and set(result) == set(expected)
                and all(type(result[key]) is int and result[key] == value
                        for key, value in expected.items()))
    if name == 'planner_batch_delivery':
        duplicate_turns = ((500, 1, 1), (501, 0, 1), (529, 0, 1), (530, 1, 2),
                           (559, 0, 2), (560, 1, 3), (589, 0, 3), (590, 0, 3))
        return (len(lines) == 18 and lines[-1] ==
                'PASS: 136 checks, 0 failures; bounded final-batch delivery and retry lifecycle'
                and all(re.fullmatch(r'batch turns=\d+ combat=\d+ duplicate_far=0 first_yields=\d+ '
                                     r'next_yields=\d+ attempts=\d+', line) for line in lines[:4])
                and lines[4] == 'batch turns=20 combat=32 duplicate_far=1 first_yields=16 next_yields=4 attempts=20'
                and all(line == f'duplicate_turn frame={frame} yields={yields} attempts={attempts}'
                        for line, (frame, yields, attempts) in zip(lines[5:13], duplicate_turns))
                and all(re.fullmatch(r'receipt kind=' + str(kind) + r' emitted=3 attempts=3', line)
                        for kind, line in enumerate(lines[13:16]))
                and lines[16] == 'task first_yields=16 next_yields=4 reset_yields=16')
    if name == 'scene_extra_recovery':
        return (len(lines) == 25 and len(set(lines[:9])) == 9 and len(set(lines[10:22])) == 12
                and all(re.fullmatch(r'[\w-]+ failure_ready=0 fault_hits=[1-9]\d* '
                                     r'recovery_ready=1 pixel_mismatches=0', line) for line in lines[:9])
                and lines[9] == 'layer_recoveries=9 pixel_mismatches=0'
                and all(re.fullmatch(r'scene_[\w-]+ variant=[01] failure_scene=0 '
                                     r'recovery_scene=1 pixel_mismatches=0', line) for line in lines[10:22])
                and lines[22] == 'scene_recoveries=12 pixel_mismatches=0'
                and re.fullmatch(r'healthy_digest=[0-9a-f]{16} checks=109 failures=0', lines[23]) is not None
                and lines[24] == 'PASS transactional frozen body/animation palette and depth caches, '
                                  'same-cache recovery, legal absent/wide/flat paths and actual SceneRenderer output')
    return False


def feature_success_log(name, text):
    """Recognize one complete pre.3 raw run, never a claimed-pass wrapper.

    Exact current counts and scenario output are bound to the test and production
    sources by validate_evidence. Build logs and failure controls cannot satisfy
    these gates; the patrol ASan buffer run is a separate execution record.
    """
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    exact = {
        'planner_defense_delivery': [
            'omitted first_defense=0 next_defense=1',
            'returned drops=15 miners=20 duplicate=0 first=1 early=0 due=1',
            'returned drops=0 miners=0 duplicate=0 first=1 early=0 due=1',
            'returned drops=0 miners=20 duplicate=1 first=1 early=0 due=1',
            'host_task first=0 next=1',
            'PASS: 21 checks, 0 failures; real Planner final-batch defense lifecycle'],
        'missile_manual_lifecycle': [
            'dead_history count=1 new_owned=1',
            'PASS: 16 checks, 0 failures; extracted production manual-ownership helpers'],
        'missile_manual_capacity': [
            'PASS: 23 checks, 0 failures; exact production ownership/delivery/Host dispatch paths'],
        'patrol_capacity': [
            'width=2 count=4 bind=1', 'width=4 count=8 bind=1',
            'width=8 count=16 bind=1', 'width=10 count=16 bind=1',
            'width=20 count=16 bind=1', 'width=40 count=16 bind=1',
            'width=80 count=16 bind=1',
            'PASS: 31 checks, 0 failures; real patrol to route, synthetic graph only'],
        'patrol_small_buffer': [
            'buffer count=16 digest=1280',
            'PASS: 1 checks, 0 failures; real patrol to route, synthetic graph only'],
        'selection_catalogue_identity': ['21 checks, 0 failures'],
        'toast_clock_wrap': ['15 checks, 0 failures'],
        'secondary_drag_bounds': ['PASS secondary drag: 7 boundary cases + 900 range cases'],
        'secondary_render_retry': ['PASS secondary retry: 8 submission and recovery cases'],
        'icon_cache_reopen': [
            'PASS failed scope -> same-path reconnect -> cache persistence/restore; '
            'healthy reconnect keeps generation'],
        'passenger_frame_sampling': [
            'first passenger paint: bubbles=1 live_RPM=0',
            'PASS 1000 paints of one logic frame: zero live RPM, immutable contents/position',
            'PASS missing/failed passenger page: no live fallback, 120ms lease never renewed, '
            'next complete frame recovers',
            'PASS no token/unconfirmed/wrong PID/match/frame/tick/retired publication/disabled/chat: '
            'closed without RPM',
            'PASS inspect disappearance/danger-only/shared demand routing: '
            'no rate reduction or alternate live scanner',
            'PASS paused frame independent confirmation and confirmation expiry',
            'passenger frame sampling: 2158 checks passed'],
    }
    if name in exact:
        return lines == exact[name]
    if not lines:
        return False

    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate result field')
            result[key] = value
        return result

    try:
        result = json.loads(lines[-1], object_pairs_hook=unique_object)
    except (ValueError, TypeError):
        return False
    if not isinstance(result, dict):
        return False
    console = {
        'console_composition': (16, 'production listeners with controlled DOM event fixture'),
        'console_hotkey_presentation': (8, 'actual production encodeHotkey and keyName'),
    }
    if name in console:
        count, level = console[name]
        return (len(lines) == count + 1 and len(set(lines[:-1])) == count
                and all(line.startswith('PASS ') for line in lines[:-1])
                and set(result) == {'suite', 'checks', 'failures', 'level'}
                and result.get('suite') == name and result.get('level') == level
                and type(result.get('checks')) is int and result['checks'] == count
                and type(result.get('failures')) is int and result['failures'] == 0)
    counts = {'superweapon_result_feedback': 13, 'pause_key_pairing': 22, 'native_mouse_pairing': 36}
    if name in counts:
        return (len(lines) == 1 and set(result) == {'checks', 'failures', 'passed'}
                and type(result.get('checks')) is int and result['checks'] == counts[name]
                and type(result.get('failures')) is int and result['failures'] == 0
                and result.get('passed') is True)
    return False


def success_log(name, text):
    """Recognize existing test output; never append or synthesize a PASS marker."""
    if name in FUNCTIONAL_CHECKS:
        return functional_success_log(name, text)
    if name in FEATURE_CHECKS:
        return feature_success_log(name, text)
    objects, texts = [], [text]

    def visit(value):
        if isinstance(value, dict):
            objects.append(value)
            for key, child in value.items():
                if key in {'stdout', 'stderr'} and isinstance(child, str):
                    texts.append(child)
                elif isinstance(child, (dict, list)):
                    visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    for candidate in (text, *text.splitlines()):
        try:
            visit(json.loads(candidate))
        except (json.JSONDecodeError, TypeError):
            pass
    body = '\n'.join(texts)
    if re.search(r'^FAIL\b', body, re.M):
        return False
    if name == 'missile_allocator':
        return any(type(row.get('equivalent_scenarios')) is int
                   and row['equivalent_scenarios'] >= 8063
                   and row.get('same_workspace_reused') is True for row in objects)
    if name == 'snapshot_reuse':
        return any(row.get('case') == 'seqlock_interleavings' and row.get('passed') is True
                   and type(row.get('checks')) is int and row['checks'] > 0 for row in objects)
    if name == 'tech_capture':
        return any(row.get('case') == 'summary' and row.get('passed') is True
                   and type(row.get('checks')) is int and row['checks'] > 0 for row in objects)
    if name == 'voxel_recovery':
        return (re.search(r'\brecovered_scenes=[1-9]\d* pixel_mismatches=0\b', body) is not None
                and re.search(r'\bchecks=[1-9]\d* failures=0\b', body) is not None
                and not any(row.get('expected_failure') is True for row in objects))
    if name == 'idle_aircraft_display':
        return re.search(r'^PASS paused same-source aircraft display;', body, re.M) is not None
    if name == 'terrain_reuse':
        return re.search(r'^PASS actual terrain storage:', body, re.M) is not None
    if name == 'dependency_probes':
        return any(row.get('passed') is True and type(row.get('tests_run')) is int
                   and row['tests_run'] > 0 and type(row.get('failures')) is int and row['failures'] == 0
                   and type(row.get('errors')) is int and row['errors'] == 0
                   for row in objects)
    return False


def validate_evidence(validation, receipt_path, commit, source_tree, source_inputs, tracked, engine_changes=()):
    require(validation.get('schema') == 'ra2.optimization-validation.v1', 'Wrong validation schema')
    require((validation.get('version'), validation.get('file_version'), validation.get('commit'),
             validation.get('source_tree_sha')) == (VERSION, FILE_VERSION, commit, source_tree),
            'Validation belongs to another frozen source revision')
    require(validation.get('required_checks_passed') is True, 'Required checks are incomplete')
    for key in ('windows_executed', 'game_tested', 'gpu_tested', 'multiplayer_tested'):
        require(validation.get(key) is False, 'Validation overclaims platform execution: ' + key)
    require(validation.get('engine_changes') == list(engine_changes),
            'Validation engine change plan differs from current source')
    bindings_by_check = dict(CHECK_BINDINGS)
    if engine_changes:
        bindings_by_check['dependency_probes'] = DEPENDENCY_BINDINGS
    checks = validation.get('checks')
    require(isinstance(checks, list) and len(checks) == len(bindings_by_check)
            and all(isinstance(row, dict) for row in checks)
            and {row.get('name') for row in checks} == set(bindings_by_check),
            'Require exactly the current optimization checks and any required dependency check')
    logs = {}
    for check in checks:
        name = check['name']
        require(type(check.get('exit_code')) is int and check['exit_code'] == 0
                and check.get('executed_this_revision') is True,
                'Check did not execute successfully this revision: ' + name)
        command = check.get('command')
        require(isinstance(command, list) and command
                and all(isinstance(value, str) and value for value in command),
                'Missing executed command argv: ' + name)
        if name == 'patrol_small_buffer':
            compiled = check.get('compile_command')
            require(isinstance(compiled, list) and compiled
                    and all(isinstance(value, str) and value for value in compiled)
                    and '-fsanitize=address' in compiled and '-fno-omit-frame-pointer' in compiled
                    and not any(value.startswith('-fno-sanitize=') for value in compiled)
                    and type(check.get('compile_exit_code')) is int and check['compile_exit_code'] == 0
                    and '--small-buffer' in command[1:],
                    'Patrol buffer gate requires the executed ASan build and buffer run')
        raw_path = check.get('log_path')
        require(isinstance(raw_path, str) and raw_path, 'Missing execution log: ' + name)
        path = Path(raw_path)
        path = (path if path.is_absolute() else receipt_path.parent / path).resolve()
        data = path.read_bytes()
        require(data and sha(data) == check.get('log_sha256'), 'Execution log is empty or changed: ' + name)
        require(success_log(name, data.decode('utf-8', errors='replace')),
                'Execution log lacks the current check result: ' + name)
        logs[str(path)] = {'bytes': len(data), 'sha256': sha(data)}
        for key, required in zip(('test_source_hashes', 'production_source_hashes'), bindings_by_check[name]):
            bindings = check.get(key)
            require(isinstance(bindings, dict) and required <= set(bindings),
                    'Incomplete ' + key + ': ' + name)
            for path, digest in bindings.items():
                require(POLICY.safe(path).parts[0] == 'source' and path in source_inputs,
                        'Validated input is outside the completed build source: ' + path)
                require(digest == source_inputs[path]['sha256'] == sha(tracked(path)),
                        'Tested source differs from compiled source: ' + path)
    return logs


def engine_change_plan(resources, engine):
    require(set(engine) == POLICY.ENGINE, 'Engine member set differs')
    changed = sorted(name for name in engine if engine[name] != resources[name])
    require(set(changed) <= APPROVED_ENGINE_CHANGES, 'Engine change is outside the reviewed plan')
    return changed


def verify_shared_transition(base_bytes, current_bytes):
    require(type(base_bytes) is int and type(current_bytes) is int
            and (base_bytes, current_bytes) == (BASE_SHARED_BYTES, SHARED_BYTES),
            'Shared layout differs from reviewed Assist 102 tail extension')


def assemble(resources, current, engine):
    """Preserve pinned resources with only the explicit current engine change."""
    engine_change_plan(resources, engine)
    update = current | {name: data for name, data in resources.items() if name.startswith('licenses/')}
    full, update = POLICY.assemble_payload(resources, update, engine)
    require(len(full) == 53 and len(update) == 21, 'Optimization package membership differs')
    require(all(full[name] == engine.get(name, data) for name, data in resources.items()),
            'Pinned resource bytes changed')
    return full, update


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'binaries', 'provenance', 'validation', 'base-full', 'base-manifest', 'out'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--source-sync', type=Path, help='Optional already-verified source push receipt')
    args = parser.parse_args()
    repo, binaries, out = args.repo.resolve(), args.binaries.resolve(), args.out.resolve()
    source = repo / 'source'
    require(HERE == repo / 'packages/v1.0.47', 'Packager must belong to the selected checkout')
    require(re.fullmatch(r'[0-9a-f]{40}', args.expected_commit) is not None, 'Use a full pinned commit SHA')
    require(not args.out.is_symlink() and not out.exists(), 'Output directory must not already exist')

    def git(*command):
        return subprocess.check_output(['git', '-C', str(repo), *command])

    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'Wrong checkout HEAD')
    root_tree = git('rev-parse', 'HEAD^{tree}').decode().strip()
    source_tree = git('rev-parse', 'HEAD:source').decode().strip()
    tree, tracked_inputs = {}, {}
    for row in git('ls-tree', '-r', '-z', '--full-tree', 'HEAD').split(b'\0'):
        if row:
            prefix, name = row.split(b'\t', 1)
            tree[name.decode()] = tuple(prefix.decode().split())

    def tracked(name):
        POLICY.safe(name)
        require(name in tree and tree[name][0] in {'100644', '100755'} and tree[name][1] == 'blob',
                'Expected regular tracked input: ' + name)
        path = repo / name
        require(not path.is_symlink(), 'Symbolic source input: ' + name)
        data = path.read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        require(blob == tree[name][2], 'Working input differs from frozen commit: ' + name)
        tracked_inputs[name] = {'bytes': len(data), 'sha256': sha(data)}
        return data

    for name in PACKAGE_INPUTS:
        tracked(name)
    readme = tracked('packages/v1.0.47/README.txt')
    require(VERSION.encode() in readme and FILE_VERSION.encode() in readme, 'README version differs')
    provenance_bytes, validation_bytes = args.provenance.read_bytes(), args.validation.read_bytes()
    provenance, validation = json.loads(provenance_bytes), json.loads(validation_bytes)
    require(provenance.get('commit') == args.expected_commit
            and type(provenance.get('build_returncode')) is int and provenance['build_returncode'] == 0
            and type(provenance.get('static_pe_verification_returncode')) is int
            and provenance['static_pe_verification_returncode'] == 0
            and provenance.get('source_inputs_unchanged_after_build') is True
            and provenance.get('git_head_verified_at_finish') is True, 'Incomplete build provenance')
    require(provenance.get('platform') == 'linux' and provenance.get('zig_version') == '0.13.0'
            and provenance.get('webview2_sdk') == '1.0.4314.35', 'Wrong pinned build toolchain')
    for key in ('controlled_windows_checks', 'runtime_tested', 'game_tested', 'multiplayer_tested'):
        require(provenance.get(key) is False, 'Cross-build provenance overclaims execution')
    built = POLICY.verified_build_artifacts(provenance, binaries)
    rows = provenance.get('source_inputs', [])
    source_inputs = {row['path']: row for row in rows}
    require(len(rows) == len(source_inputs)
            and set(source_inputs) == {name for name in tree if name.startswith('source/')},
            'Build provenance must bind every source blob exactly once')
    for name, row in source_inputs.items():
        data = tracked(name)
        require(row['git_blob_sha'] == tree[name][2]
                and row['bytes'] == len(data) and row['sha256'] == sha(data), 'Compiled source changed: ' + name)
    require(source_tree == provenance.get('source_tree_sha'), 'Compiled source tree differs')
    resources, base_record, base_manifest = pinned_base(args.base_full, args.base_manifest)
    POLICY.source_engine_members(source / 'engine')
    engine = {name: tracked('source/' + name) for name in POLICY.ENGINE}
    changed_engine = engine_change_plan(resources, engine)
    log_bindings = validate_evidence(validation, args.validation.resolve(), args.expected_commit,
                                     source_tree, source_inputs, tracked, changed_engine)

    sync, sync_bytes = None, None
    public_commit = args.expected_commit
    if args.source_sync:
        sync_bytes = args.source_sync.read_bytes()
        sync = json.loads(sync_bytes)
        require(sync.get('local_commit') == args.expected_commit
                and sync.get('root_tree_sha') == root_tree and sync.get('remote_root_tree_sha') == root_tree
                and sync.get('remote_tree_verified') is True and sync.get('blob_digests_verified') is True,
                'Source sync receipt differs from the frozen complete tree')
        public_commit = sync.get('remote_commit', '')
        require(re.fullmatch(r'[0-9a-f]{40}', public_commit) is not None, 'Missing synced source commit')

    sys.path.insert(0, str(source / 'tools'))
    from verify_windows_outputs import verify, read_release_version
    file_version, version = read_release_version(source / 'shared/source/release_version.h')
    require((version, file_version) == (VERSION, FILE_VERSION), 'Wrong prerelease version')
    identity_bytes = tracked('source/BUILD_SOURCE_MANIFEST.json')
    identity = json.loads(identity_bytes)
    require(identity.get('version') == VERSION and identity.get('build_protocol') == ASSIST_PROTOCOL,
            'Source identity or Assist protocol differs')
    require((binaries / 'BUILD_SOURCE_MANIFEST.json').read_bytes() == identity_bytes, 'Built source identity differs')
    target_bytes = (binaries / 'BUILD_TARGET.json').read_bytes()
    target = json.loads(target_bytes)
    require(tuple(target.get(key) for key in ('host', 'runtime', 'broker', 'assist_protocol',
                                             'cursor_protocol', 'runtime_protocol', 'console_ui'))
            == ('x64', 'x86', 'x86', ASSIST_PROTOCOL, 155, 4, 'webview2'), 'Wrong completed build profile')
    actual, errors = verify(binaries, host_arch='x64', webview2=True)
    require(not errors, 'PE verification failed: ' + '; '.join(errors))
    verified = dict(actual)
    verified['WebView2Loader.dll'] = actual['RA2Toolbox.exe']['webview2_loader']
    current = {name: (binaries / name).read_bytes() for name in POLICY.BINARIES}
    for name, data in current.items():
        POLICY.verify_artifact_binding(name, built[name], verified[name], data)

    verify_shared_transition(base_manifest['assist_shared_bytes'],
                             actual['GameRuntime.dll']['assist_shared_bytes'])
    for name in ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe'):
        require(sha(current[name]) != base_record['files'][name]['sha256'], 'Previous product program reused: ' + name)
    dependency_manifest = POLICY.verify_dependency_manifest(engine['engine/runtime-dependencies.json'])
    for filename in ('LICENSE.txt', 'NOTICE.txt'):
        name = 'licenses/WebView2/' + filename
        data = (binaries / name).read_bytes()
        require(data == resources[name] and sha(data) == verified['WebView2Loader.dll']['licenses'][name],
                'Verified loader license differs from pinned base: ' + name)
    for name, path in {'licenses/LICENSE-GPL.txt': 'LICENSE-GPL',
                       'licenses/THIRD_PARTY_NOTICES.md': 'THIRD_PARTY_NOTICES.md',
                       'licenses/xbrz-License.txt': 'source/third_party/xbrz/License.txt'}.items():
        require(tracked(path) == resources[name], 'License changed from pinned base: ' + name)
    current['README.txt'] = readme
    current['Start.cmd'] = b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nstart "" "%~dp0RA2Toolbox.exe"\r\n'
    full_contents, update_contents = assemble(resources, current, engine)
    require(set(full_contents) == set(base_record['files']), 'Full package path set changed')

    # Stage locally; only expose a complete package set after all final checks.
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ra2-147-package-', dir=out.parent) as temporary:
        stage = Path(temporary) / 'complete'
        stage.mkdir()
        update = POLICY.write_zip(stage / f'RA2Toolbox-{VERSION}-x64-Update.zip', update_contents, full=False)
        full = POLICY.write_zip(stage / f'RA2Toolbox-{VERSION}-x64-Full.zip', full_contents, full=True)
        require(all(full['files'][name] == update['files'][name] for name in update_contents),
                'Full and Update paired members differ')
        report = {
            'schema': 'ra2.optimization-release.v1', 'version': VERSION, 'file_version': FILE_VERSION,
            'source_commit': public_commit, 'local_build_commit': args.expected_commit,
            'source_tree_sha': source_tree, 'complete_git_tree_sha': root_tree,
            'source_published': sync is not None, 'source_identity': sync,
            'source_sync_receipt_sha256': sha(sync_bytes) if sync_bytes is not None else None,
            'build_method': 'Zig 0.13.0 Windows-target cross compilation on Linux',
            'build_provenance_sha256': sha(provenance_bytes), 'source_files_verified': len(source_inputs),
            'packaging_inputs': {name: tracked_inputs[name] for name in PACKAGE_INPUTS},
            'assist_protocol': ASSIST_PROTOCOL, 'cursor_protocol': 155, 'runtime_protocol': 4,
            'assist_shared_bytes': actual['GameRuntime.dll']['assist_shared_bytes'],
            'actual_pe_verified': actual, 'validation': validation,
            'validation_evidence_sha256': sha(validation_bytes), 'verified_validation_logs': log_bindings,
            'controlled_windows_checks': False, 'current_release_binaries_executed_on_windows': False,
            'full_game_tested': False, 'physical_gpu_tested': False, 'multiplayer_tested': False,
            'prerelease': True, 'make_latest': False,
            'deferred_work': ['cursor_mixed_canvas_fix'] + ([] if changed_engine else ['dependency_probe_optimization']),
            'resources': {'source_release': 'v1.0.46', 'source_file_version': '1.0.46.4',
                          'source_commit': BASE_COMMIT, 'archive': BASE_NAME, 'bytes': BASE_BYTES,
                          'sha256': BASE_SHA, 'manifest_sha256': BASE_MANIFEST_SHA,
                          'verified_members': 53,
                          'unchanged_members': sorted(set(resources) - set(changed_engine)),
                          'old_product_programs_reused': False, 'generated_cursor_resources_bundled': False},
            'optional_runtime': {'bundled': False, 'native_startup_requires_download': False,
                                 'existing_runtime_and_cache_preserved_by_update': True,
                                 'engine_unchanged_from_base': not changed_engine,
                                 'engine_changed_members': changed_engine, 'complete_engine_in_update': True,
                                 'engine_source': 'current_frozen_source_with_explicit_change_plan',
                                 'manifest_path': 'engine/runtime-dependencies.json',
                                 'manifest_sha256': sha(resources['engine/runtime-dependencies.json']),
                                 'dependencies': dependency_manifest},
            'packages': [full, update],
        }
        (stage / 'release-manifest.json').write_bytes(encoded(report))
        sums = [stage / full['name'], stage / update['name'], stage / 'release-manifest.json']
        (stage / 'SHA256SUMS.txt').write_text(''.join(sha(path.read_bytes()) + '  ' + path.name + '\n'
                                                   for path in sums), encoding='ascii')
        require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'HEAD changed during packaging')
        for name, record in list(tracked_inputs.items()):
            require(record == {'bytes': len(data := tracked(name)), 'sha256': sha(data)},
                    'Source changed during packaging: ' + name)
        for name in POLICY.BINARIES:
            require(built[name] == {'bytes': len(data := (binaries / name).read_bytes()), 'sha256': sha(data)},
                    'Built product changed during packaging: ' + name)
        for path, record in log_bindings.items():
            require(record == {'bytes': len(data := Path(path).read_bytes()), 'sha256': sha(data)},
                    'Validation log changed during packaging: ' + path)
        receipts = [(args.provenance, provenance_bytes), (args.validation, validation_bytes),
                    (binaries / 'BUILD_TARGET.json', target_bytes),
                    (binaries / 'BUILD_SOURCE_MANIFEST.json', identity_bytes)]
        if args.source_sync:
            receipts.append((args.source_sync, sync_bytes))
        for path, before in receipts:
            require(path.read_bytes() == before, 'Evidence changed during packaging: ' + str(path))
        require({path.name for path in stage.iterdir()} == {path.name for path in sums} | {'SHA256SUMS.txt'},
                'Distribution member set differs')
        require(not out.exists(), 'Output appeared during packaging')
        os.rename(stage, out)
    print(json.dumps({'version': VERSION, 'file_version': FILE_VERSION, 'commit': args.expected_commit,
                      'source_published': sync is not None, 'checks': [row['name'] for row in validation['checks']],
                      'windows_executed': False, 'gpu_tested': False,
                      'packages': [{key: value for key, value in row.items() if key != 'files'}
                                   for row in (full, update)], 'output': str(out)}, indent=2))


if __name__ == '__main__':
    main()
