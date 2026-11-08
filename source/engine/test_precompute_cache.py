"""Real portable DIS engine/cache tests; not Windows or game execution."""
import argparse
import contextlib
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import types
import unittest
from unittest import mock

import numpy as np
import cv2
import scipy
import precompute as P
import preview_builder as V


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fixture(folder, count=450):
    folder.mkdir(parents=True, exist_ok=True)
    frames = np.zeros((count, 43, 55), np.uint8)
    frames[0, 17:24, 20:27] = 1
    frames[0, 19:22, 22:25] = 2
    frames[1, 17:24, 22:29] = 1
    frames[1, 19:22, 24:27] = 2
    frames[2] = frames[1]
    header = bytearray(struct.pack('<4H', 0, 55, 43, count) + bytes(24 * count))
    data = bytearray()
    for i in range(3):
        off = len(header) + len(data)
        struct.pack_into('<4H4I', header, 8 + i * 24, 0, 0, 55, 43, 1, 0, 0, off)
        data.extend(frames[i].tobytes())
    (folder / 'original.sha').write_bytes(header + data)
    pal = np.zeros((256, 4), np.uint8)
    pal[:, 3] = 255
    pal[1] = [20, 180, 240, 255]
    pal[2] = [200, 40, 10, 255]
    (folder / 'palette.bgra').write_bytes(pal.tobytes())
    (folder / 'groups.json').write_text(json.dumps([{'start': 0, 'count': 3, 'loop': False}]))
    task = folder / '.preprocess' / 'task1'
    task.mkdir(parents=True)
    return types.SimpleNamespace(profile=str(folder), status=str(task / 'progress.ini'), cancel=str(task / 'cancel.flag'))


class CacheTests(unittest.TestCase):
    root = None
    quality_digest = None

    @classmethod
    def setUpClass(cls):
        cls.root = Path(OUTPUT) / 'profile'
        cls.args = fixture(cls.root)
        cls.request = Path(cls.args.status).with_name('request.json')
        cls.canonical = '0 1\n1 2\n'
        cls.source_hashes = {n: sha(cls.root / n) for n in ('original.sha', 'palette.bgra', 'groups.json')}
        cls.frames = P.decode(cls.root / 'original.sha')
        cls.pal = P.palette(cls.root / 'palette.bgra')
        cls.first = P.bgra(cls.pal[cls.frames[0]])
        cls.last = P.bgra(cls.pal[cls.frames[1]])

    def setUp(self):
        Path(self.args.cancel).unlink(missing_ok=True)

    def request_mode(self, mode, pairs=None):
        value = {'schema': 1, 'mode': mode}
        if pairs is not None:
            value['pairs'] = pairs
        self.request.write_text(json.dumps(value))

    def no_engine(self):
        stack = contextlib.ExitStack()
        for name in ('configure', 'setup', 'render'):
            stack.enter_context(mock.patch.object(P.engine, name, side_effect=AssertionError('cached data reached engine.' + name)))
        return stack

    def test_01_request_schema(self):
        cases = [None, [], {}, {'schema': True, 'mode': 'all'}, {'schema': 2, 'mode': 'all'},
                 {'schema': 1, 'mode': True}, {'schema': 1, 'mode': 'unknown'},
                 {'schema': 1, 'mode': 'all', 'extra': 1}, {'schema': 1, 'mode': 'preview', 'pairs': []}]
        for pair in (None, [], [[0]], [[0, 0]], [[False, 1]], [[0.0, 1]], [['0', 1]], [[-1, 1]], [[0, 450]], [[0, 1]] * 513):
            cases.append({'schema': 1, 'mode': 'pairs', 'pairs': pair})
        for value in cases:
            with self.subTest(value=str(value)[:100]):
                self.request.write_text(json.dumps(value))
                with self.assertRaises(ValueError):
                    P.request_pairs(self.request, [(0, 1), (1, 2)], 450)
        self.request.write_text('{"schema":1,"schema":1,"mode":"all"}')
        with self.assertRaises(ValueError):
            P.request_pairs(self.request, [], 450)
        self.request_mode('pairs', [[449, 448], [0, 1], [449, 448]])
        self.assertEqual(P.request_pairs(self.request, [(0, 1)], 450), ([(449, 448), (0, 1)], False))
        self.request_mode('pairs', [[516, 0]])
        self.assertEqual(P.request_pairs(self.request, [], 517), ([(516, 0)], False))
        with self.assertRaises(ValueError):
            P.request_pairs(self.request, [], 450)
        self.request.unlink()
        self.assertEqual(P.request_pairs(self.request, [(0, 1)], 450), ([(0, 1)], True))

    def test_02_real_pair_quality_and_subset(self):
        self.request_mode('pairs', [[0, 1], [0, 1]])
        with mock.patch.object(P.engine, 'setup', wraps=P.engine.setup) as setup, mock.patch.object(P.engine, 'render', wraps=P.engine.render) as render:
            P.run(self.args)
        self.assertEqual(setup.call_count, 1)
        self.assertEqual(render.call_count, 1001)
        path = self.root / 'timelines/0-1.dis'
        self.assertTrue(V.valid_pack(path, self.first, self.last))
        self.assertEqual(len(list(V.pixels(path, self.first, self.last))), 1001)
        self.assertFalse((self.root / 'timelines/1-2.dis').exists())
        self.assertFalse((self.root / 'preview.html').exists())
        self.assertEqual((self.root / 'pairs.txt').read_text(), self.canonical)
        # Render the unchanged quality algorithm at every k/1000 independently.
        P.engine.configure(self.frames, self.pal)
        data = P.engine.setup(0, 1)
        reference = Path(OUTPUT) / 'quality-reference.dis'
        P.write_pack(reference, (P.bgra(P.engine.render(data, k / 1000)) for k in range(1001)), self.first, self.last)
        self.assertEqual(path.read_bytes(), reference.read_bytes())
        type(self).quality_digest = sha(path)
        self.assertEqual({n: sha(self.root / n) for n in self.source_hashes}, self.source_hashes)

    def test_03_valid_cache_never_renders(self):
        path = self.root / 'timelines/0-1.dis'
        before = (sha(path), path.stat().st_mtime_ns)
        self.request_mode('pairs', [[0, 1]])
        with self.no_engine():
            P.run(self.args)
        self.assertEqual(before, (sha(path), path.stat().st_mtime_ns))
        self.assertIn('State=done', Path(self.args.status).read_text())

    def test_04_full_parser_and_corrupt_repair(self):
        original = (self.root / 'timelines/0-1.dis').read_bytes()
        constant = Path(OUTPUT) / 'constant.dis'
        P.write_pack(constant, (self.first for _ in range(1001)), self.first, self.first)
        fixed = constant.read_bytes()
        at = 28 + 55 * 43 * 4
        bad = [b'', original[:27], b'X' + original[1:], original + b'junk', original[:-1]]
        for offset in (8, 12, 16, 20, 28):
            changed = bytearray(original)
            changed[offset] ^= 1
            bad.append(bytes(changed))
        for delta in (b'\xff\xff\xff\xff\x1f', b'\0\0', P.var(2366) + b'\0', b'\0' + P.var(2366), b'\0\1\x01\x02\x03'):
            bad.append(fixed[:at] + delta)
        path = Path(OUTPUT) / 'bad.dis'
        for number, data in enumerate(bad):
            with self.subTest(corruption=number):
                path.write_bytes(data)
                self.assertFalse(V.valid_pack(path, self.first, self.last))
                # Delta-corruption cases use matching endpoints/digest too.
                if number >= 10:
                    self.assertFalse(V.valid_pack(path, self.first, self.first))
        target = self.root / 'timelines/0-1.dis'
        target.write_bytes(original[:-1])
        self.request_mode('pairs', [[0, 1]])
        with mock.patch.object(P.engine, 'setup', wraps=P.engine.setup) as setup, mock.patch.object(P.engine, 'render', wraps=P.engine.render) as render:
            P.run(self.args)
        self.assertEqual((setup.call_count, render.call_count), (1, 1001))
        self.assertEqual(target.read_bytes(), original)
        Path(self.args.cancel).touch()
        with self.assertRaises(InterruptedError):
            V.valid_pack(target, self.first, self.last, self.args.cancel)

    def test_05_cancel_keeps_finished_segments(self):
        target = self.root / 'timelines/0-1.dis'
        before = sha(target)
        self.request_mode('pairs', [[0, 1], [449, 448]])
        real_write = P.write_pack
        def interrupted(path, images, first, last):
            def stop():
                for i, image in enumerate(images):
                    if i == 20:
                        Path(self.args.cancel).touch()
                    yield image
            return real_write(path, stop(), first, last)
        with mock.patch.object(P, 'write_pack', side_effect=interrupted), self.no_engine():
            with self.assertRaises(InterruptedError):
                P.run(self.args)
        self.assertEqual(sha(target), before)
        self.assertFalse((self.root / 'timelines/449-448.dis').exists())
        self.assertFalse(list((self.root / 'timelines').glob('*.tmp')))
        self.assertEqual((self.root / 'pairs.txt').read_text(), self.canonical)
        Path(self.args.cancel).unlink()
        with self.no_engine():
            P.run(self.args)
        self.assertTrue((self.root / 'timelines/449-448.dis').exists())

    def test_06_all_preview_and_legacy(self):
        self.request_mode('all')
        with self.no_engine():
            P.run(self.args)  # Remaining 1->2 endpoints are identical.
        self.assertTrue((self.root / 'timelines/1-2.dis').exists())
        self.assertFalse((self.root / 'preview.html').exists())
        self.request_mode('preview')
        with self.no_engine():
            P.run(self.args)
        preview = self.root / 'preview.html'
        self.assertIn('data:image', preview.read_text())
        self.assertNotIn('__DATA__', preview.read_text())
        before = preview.read_bytes()
        real_png = V.png
        def cancel_png(image):
            out = real_png(image)
            Path(self.args.cancel).touch()
            return out
        with mock.patch.object(V, 'png', side_effect=cancel_png), self.no_engine():
            with self.assertRaises(InterruptedError):
                P.run(self.args)
        self.assertEqual(preview.read_bytes(), before)
        self.assertFalse(preview.with_name(preview.name + '.tmp').exists())
        Path(self.args.cancel).unlink()
        self.request.unlink()
        with self.no_engine():
            P.run(self.args)
        self.assertEqual(preview.read_bytes(), before)
        self.assertEqual((self.root / 'pairs.txt').read_text(), self.canonical)

    def test_07_atomic_cancel_and_517(self):
        path = Path(OUTPUT) / 'atomic.txt'
        path.write_text('retained')
        Path(self.args.cancel).touch()
        with self.assertRaises(InterruptedError):
            V.atomic_text(path, 'replacement', self.args.cancel)
        self.assertEqual(path.read_text(), 'retained')
        self.assertFalse(path.with_name(path.name + '.tmp').exists())
        args = fixture(Path(OUTPUT) / 'profile517', 517)
        Path(args.status).with_name('request.json').write_text(json.dumps({'schema': 1, 'mode': 'pairs', 'pairs': [[516, 515]]}))
        with self.no_engine():
            P.run(args)
        self.assertTrue((Path(args.profile) / 'timelines/516-515.dis').exists())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    OUTPUT = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix='precompute-cache-'))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(CacheTests))
    report = {'pass': result.wasSuccessful(), 'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
              'scope': __doc__, 'windows_executed': False, 'game_executed': False, 'real_opencv_engine_executed': True,
              'dependencies': {'numpy': np.__version__, 'opencv': cv2.__version__, 'scipy': scipy.__version__},
              'quality_reference_sha256': CacheTests.quality_digest,
              'source_sha256': {p.name: sha(p) for p in (Path(__file__), Path(P.__file__), Path(V.__file__), Path(P.engine.__file__))}}
    (OUTPUT / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)
