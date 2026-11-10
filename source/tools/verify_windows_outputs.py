#!/usr/bin/env python3
"""Verify exact host/runtime architectures, ABI, dependencies and release version."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import sys
import xml.etree.ElementTree as ET
# Windows embedded Python uses python312._pth and omits the script directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pe32_image import Image
from prepare_console_sdk import VERSION as WEBVIEW2_VERSION, validate_bytes as validate_sdk_bytes

ROOT = Path(__file__).resolve().parent.parent


def read_release_version(header=ROOT / 'shared/source/release_version.h'):
    text = header.read_text('utf-8')
    version = re.search(r'^#define\s+RA2_RELEASE_VERSION\s+"(\d+\.\d+\.\d+\.\d+)"[ \t]*$', text, re.M)
    label = re.search(r'^#define\s+RA2_RELEASE_VERSION_W\s+L"([^"\r\n]+)"[ \t]*$', text, re.M)
    parts = re.search(r'^#define\s+RA2_RELEASE_VERSION_PARTS\s+(\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+)[ \t]*$', text, re.M)
    if not version or not label or not parts:
        raise ValueError('release_version.h must define numeric, display and four-part versions')
    numeric = tuple(int(part) for part in parts.group(1).split(','))
    if any(part > 65535 for part in numeric) or numeric != tuple(map(int, version.group(1).split('.'))):
        raise ValueError('release_version.h numeric version and VERSION_PARTS differ')
    return version.group(1), label.group(1)


RELEASE_FILE_VERSION, RELEASE_LABEL = read_release_version()
NAMES = ('RA2Toolbox.exe','GameRuntime.dll')
BROKER = 'GameRuntimeHost32.exe'
NEURAL = 'RA2NeuralRenderer.exe'
HOOKS = {'GameRuntime.dll':('GameRuntimeHook','GameRuntimePostHook','GameRuntimeInfo','AssistProtocolInfo','AssistMappingPrefix','ManageGameEntry','GameEntryManagerInfo')}
SYSTEM = {'kernel32.dll', 'user32.dll', 'gdi32.dll', 'winmm.dll', 'gdiplus.dll',
          'comctl32.dll', 'shell32.dll', 'ole32.dll', 'comdlg32.dll', 'advapi32.dll',
          'msvcrt.dll', 'ucrtbase.dll', 'ntdll.dll', 'version.dll', 'ws2_32.dll',
          'shlwapi.dll', 'oleaut32.dll', 'imm32.dll'}
NEURAL_SYSTEM = SYSTEM | {'d3d12.dll', 'dxgi.dll', 'wintrust.dll', 'crypt32.dll'}
ASSIST_PROTOCOL = 106
ASSIST_SHARED_BYTES = 23162672
ASSIST_MAPPING = f'Local\\RA2Assist_v{ASSIST_PROTOCOL}_'
ENTRY_PROTOCOL = 3
ENTRY_REQUEST_BYTES = 108
RUNTIME_PROTOCOL = 4
RUNTIME_CONTROL_BYTES = 176
CURSOR_PROTOCOL = 155
CURSOR_MAPPING = 'Local\\RA2CursorBridge_v155_%lu'


def verify_cursor_mapping(pe, label):
    # Cursor has its own mapping ABI while Assist/Runtime versions remain
    # unchanged. Verify both production components, not just Assist exports.
    # Require the complete format string in mapped PE section data; appended
    # evidence or arbitrary overlay bytes cannot satisfy the check.
    marker = (CURSOR_MAPPING + '\0').encode('utf-16le')
    if not any(marker in pe.take(raw, min(size, virtual or size))
               for _, _, size, raw, virtual in pe.sections if size):
        raise ValueError(label + ': Cursor155 mapping namespace missing or mismatched')
    return CURSOR_PROTOCOL


def verify_host_manifest(pe, host_arch='x86'):
    # GetWindowSubclass is imported by name. Windows needs the v6 activation
    # dependency in CREATEPROCESS_MANIFEST_RESOURCE_ID (1), before main runs.
    # A declaration in an unused module manifest or arbitrary .rsrc text is
    # insufficient. Check every language of the final executable's resource.
    architecture = {'x86': 'x86', 'x64': 'amd64'}[host_arch]
    leaves = pe.resource_blobs(24, 1)
    if not leaves:
        raise ValueError('EXE process manifest resource RT_MANIFEST/1 missing')
    namespace = {'a': 'urn:schemas-microsoft-com:asm.v1'}
    for language, blob in leaves:
        root = ET.fromstring(blob)
        if root.tag != '{urn:schemas-microsoft-com:asm.v1}assembly':
            raise ValueError('EXE process manifest has an incorrect XML namespace')
        identity = root.find('a:assemblyIdentity', namespace)
        if identity is None or identity.get('processorArchitecture') != architecture:
            raise ValueError('EXE process manifest assembly architecture mismatch')
        dependencies = root.findall('a:dependency/a:dependentAssembly/a:assemblyIdentity', namespace)
        controls = [item for item in dependencies
                    if item.get('name') == 'Microsoft.Windows.Common-Controls']
        if len(controls) != 1:
            raise ValueError('EXE manifest lacks one Common Controls v6 dependency; GetWindowSubclass cannot load')
        item = controls[0]
        expected = {'type': 'win32', 'version': '6.0.0.0', 'processorArchitecture': architecture,
                    'publicKeyToken': '6595b64144ccf1df', 'language': '*'}
        if any(item.get(key) != value for key, value in expected.items()):
            raise ValueError('EXE Common Controls dependency identity/version/architecture mismatch')
    return {'resource_type': 24, 'resource_id': 1, 'languages': [language for language, _ in leaves],
            'name': 'Microsoft.Windows.Common-Controls', 'version': '6.0.0.0', 'architecture': architecture}


def verify_worker_manifest(pe, architecture='x86', label='x86 broker'):
    leaves = pe.resource_blobs(24, 1)
    if not leaves:
        raise ValueError(label + ' process manifest resource RT_MANIFEST/1 missing')
    namespace = {'a': 'urn:schemas-microsoft-com:asm.v1',
                 't': 'urn:schemas-microsoft-com:asm.v3'}
    for _, blob in leaves:
        root = ET.fromstring(blob)
        identity = root.find('a:assemblyIdentity', namespace)
        if root.tag != '{urn:schemas-microsoft-com:asm.v1}assembly' or identity is None or identity.get('processorArchitecture') != architecture:
            raise ValueError(label + ' manifest assembly architecture mismatch')
        # The child inherits the host's token; it must not launch a second UAC
        # prompt or opt into privileged UI messaging itself.
        levels = root.findall('.//t:requestedExecutionLevel', namespace)
        if len(levels) != 1 or levels[0].get('level') != 'asInvoker' or levels[0].get('uiAccess') != 'false':
            raise ValueError(label + ' manifest must use asInvoker/uiAccess=false')
    return {'resource_type': 24, 'resource_id': 1,
            'languages': [language for language, _ in leaves], 'architecture': architecture,
            'execution_level': 'asInvoker'}


def verify_broker_manifest(pe):
    return verify_worker_manifest(pe)


def verify_webview2_loader(root, host):
    name = 'WebView2Loader.dll'
    data = (root / name).read_bytes()
    validate_sdk_bytes('bin/' + name, data)
    loader = Image(data, allow_x64=True)
    if loader.machine != 0x8664 or not loader.characteristics & 0x2000:
        raise ValueError('WebView2 loader must be the pinned x64 DLL')
    required = {'CreateCoreWebView2EnvironmentWithOptions', 'GetAvailableCoreWebView2BrowserVersionString'}
    if not required <= set(loader.exports()):
        raise ValueError('WebView2 loader entry points missing')
    if {name.lower() for name in loader.imports()} != {'kernel32.dll'}:
        raise ValueError('Pinned WebView2 loader dependency set differs')
    marker = 'WebView2Loader.dll'.encode('utf-16le')
    if not any(marker in host.take(raw, min(size, virtual or size))
               for _, _, size, raw, virtual in host.sections if size):
        raise ValueError('Host was not compiled with the WebView2 console')
    licenses = {}
    for filename in ('LICENSE.txt', 'NOTICE.txt'):
        path = root / 'licenses' / 'WebView2' / filename
        content = path.read_bytes()
        validate_sdk_bytes('licenses/' + filename, content)
        licenses['licenses/WebView2/' + filename] = hashlib.sha256(content).hexdigest()
    return {'name': name, 'sdk_version': WEBVIEW2_VERSION,
            'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'machine': loader.architecture, 'imports': list(loader.imports()),
            'licenses': licenses,
            'runtime_requirement': 'Microsoft Edge WebView2 Evergreen Runtime',
            'runtime_installed_or_executed': False}


def verify(root, host_arch='x86', neural_helper=False, webview2=False):
    if host_arch not in ('x86', 'x64'):
        raise ValueError('host_arch must be x86 or x64')
    if neural_helper and host_arch != 'x64':
        raise ValueError('the optional neural helper requires the x64 host profile')
    if webview2 and host_arch != 'x64':
        raise ValueError('the WebView2 console requires the x64 host profile')
    legacy = json.loads((root / 'BUILD_SOURCE_MANIFEST.json').read_text('utf-8')).get('legacy_binary_sha256', {})
    recs, images, errors = {}, {}, []
    names = NAMES + ((BROKER,) if host_arch == 'x64' else ()) + ((NEURAL,) if neural_helper else ())
    for name in names:
        try:
            buf = (root / name).read_bytes()
            pe = Image(buf, allow_x64=host_arch == 'x64')
            expected_machine = 0x8664 if host_arch == 'x64' and name in ('RA2Toolbox.exe', NEURAL) else 0x14c
            if pe.machine != expected_machine:
                raise ValueError('wrong architecture for ' + name + ': expected ' + ('x64 host' if expected_machine == 0x8664 else 'x86 game component'))
            if bool(pe.characteristics & 0x2000) != name.endswith('.dll'):
                raise ValueError('unexpected EXE/DLL type')
            digest = hashlib.sha256(buf).hexdigest()
            if digest == legacy.get(name):
                raise ValueError('unchanged v0.4.1 binary')
            exports = pe.exports()
            if name.endswith('.dll') and ({x.split('@')[0] for x in exports}!=set(HOOKS[name]) or any('@' in x and exports[x]!=exports[x.split('@')[0]] for x in exports)):
                raise ValueError('unexpected component export bypass')
            required = HOOKS.get(name, ('AssistHostProtocolInfo',) if name == 'RA2Toolbox.exe' else ())
            if name == BROKER and exports:
                raise ValueError('unexpected x86 broker export; broker is an IPC child, not another game entry')
            if name == NEURAL and exports:
                raise ValueError('unexpected neural helper export; helper is a separate IPC renderer')
            if any(symbol not in exports for symbol in required):
                raise ValueError('required export missing: ' + ', '.join(required))
            imports = pe.imports()
            system = NEURAL_SYSTEM if name == NEURAL else SYSTEM
            allowed_bundle = {'webview2loader.dll'} if webview2 and name == 'RA2Toolbox.exe' else set()
            unknown = [x for x in imports if x.lower() not in system | allowed_bundle
                       and not x.lower().startswith(('api-ms-win-', 'ext-ms-win-'))]
            if unknown:
                raise ValueError('unbundled non-system dependency: ' + ', '.join(unknown))
            versions = pe.resource_blobs(16, 1)
            expected_version = (RELEASE_FILE_VERSION + '\0').encode('utf-16le')
            if not versions or any(expected_version not in blob for _, blob in versions):
                raise ValueError('file version resource mismatch: expected ' + RELEASE_FILE_VERSION)
            images[name] = pe
            recs[name] = {'bytes': len(buf), 'sha256': digest, 'machine': pe.architecture,
                          'sections': pe.section_count, 'entry_rva': hex(pe.entry),
                          'exports': list(exports), 'import_dlls': list(imports),
                          'imports': imports}
        except Exception as ex:
            errors.append(f'{name}: {ex}')
    if not errors:
        try:
            host, bridge = images['RA2Toolbox.exe'], images['GameRuntime.dll']
            for name in NAMES:
                recs[name]['cursor_protocol'] = verify_cursor_mapping(images[name], name)
                recs[name]['cursor_mapping'] = CURSOR_MAPPING
            recs['RA2Toolbox.exe']['common_controls_activation'] = verify_host_manifest(host, host_arch)
            if host_arch == 'x64':
                recs[BROKER]['process_manifest'] = verify_broker_manifest(images[BROKER])
            if webview2:
                recs['RA2Toolbox.exe']['webview2_loader'] = verify_webview2_loader(root, host)
                for name in ('GameRuntime.dll', BROKER):
                    if any('webview2' in item.lower() for item in recs[name]['import_dlls']):
                        raise ValueError('WebView2 dependency leaked into the x86 game component: ' + name)
            if neural_helper:
                recs[NEURAL]['process_manifest'] = verify_worker_manifest(images[NEURAL], 'amd64', 'x64 neural helper')
            a = host.at_rva(host.exports()['AssistHostProtocolInfo'], 8)
            b = bridge.at_rva(bridge.exports()['AssistProtocolInfo'], 8)
            protocol, shared_bytes = struct.unpack('<II', a)
            if a != b or protocol != ASSIST_PROTOCOL or shared_bytes != ASSIST_SHARED_BYTES:
                raise ValueError('host/bridge protocol or Shared structure size mismatch')
            marker = (ASSIST_MAPPING + '\0').encode('utf-16le')
            if 'AssistHostMappingPrefix' not in host.exports() or host.at_rva(host.exports()['AssistHostMappingPrefix'],len(marker)) != marker:
                raise ValueError('host mapping namespace mismatch')
            if bridge.at_rva(bridge.exports()['AssistMappingPrefix'], len(marker)) != marker:
                raise ValueError('incorrect exported mapping namespace')
            manager = images['GameRuntime.dll']
            if struct.unpack('<II',manager.at_rva(manager.exports()['GameEntryManagerInfo'],8)) != (ENTRY_PROTOCOL, ENTRY_REQUEST_BYTES):
                raise ValueError('entry manager API mismatch')
            if struct.unpack('<II', manager.at_rva(manager.exports()['GameRuntimeInfo'],8)) != (RUNTIME_PROTOCOL, RUNTIME_CONTROL_BYTES):
                raise ValueError('Runtime diagnostic transport ABI mismatch')
            start=manager.optional+manager.u16(manager.nt+20)
            for symbol in ('GameRuntimeHook','GameRuntimePostHook'):
                rva=manager.exports()[symbol]
                sections=[i for i,(_,base,raw,_,virtual) in enumerate(manager.sections) if base<=rva<base+min(raw,virtual)]
                if len(sections)!=1 or not manager.u32(start+40*sections[0]+36)&0x20000000:
                    raise ValueError('hook export is not executable: '+symbol)
                if manager.exports().get(symbol+'@12',rva)!=rva:
                    raise ValueError('stdcall hook alias mismatch: '+symbol)
                recs['GameRuntime.dll'][symbol+'_rva']=hex(rva)
            recs['GameRuntime.dll']['runtime_protocol']=4
            recs['GameRuntime.dll']['runtime_control_bytes']=176
            resource = host.resource_bytes()
            for text in (b'requireAdministrator', b'PerMonitorV2'):
                if text not in resource:
                    raise ValueError('EXE manifest missing: ' + text.decode())
            for name in NAMES:
                rec = recs[name]
                rec['assist_protocol'] = protocol
                rec['assist_shared_bytes'] = shared_bytes
        except Exception as ex:
            errors.append(str(ex))
    return recs, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--host-arch', choices=('x86', 'x64'), default='x86')
    parser.add_argument('--neural-helper', action='store_true', help='also require and verify the optional x64 RA2NeuralRenderer.exe; does not claim a DLSS-capable device/runtime')
    parser.add_argument('--webview2', action='store_true', help='require the x64 modern console, pinned Microsoft loader and license notices')
    args = parser.parse_args()
    if args.neural_helper and args.host_arch != 'x64':
        parser.error('--neural-helper requires --host-arch x64')
    if args.webview2 and args.host_arch != 'x64':
        parser.error('--webview2 requires --host-arch x64')
    root = args.root.resolve()
    for name in ('BUILD-VERIFIED-v130.tag', 'WINDOWS_BUILD_ARTIFACTS.json'):
        (root / name).unlink(missing_ok=True)
    recs, errors = verify(root, args.host_arch, args.neural_helper, args.webview2)
    if errors:
        print('BUILD NOT VERIFIED:\n' + '\n'.join(errors))
        return 1
    architecture_label = 'PE32/ABI' if args.host_arch == 'x86' else 'x64 host/x86 runtime/broker ABI'
    (root / 'BUILD-VERIFIED-v130.tag').write_text(
        f'v{RELEASE_LABEL} SingleRuntime: {architecture_label}/import/version checks passed\n'
        'Game runtime and multiplayer not tested\n', encoding='utf-8')
    (root / 'WINDOWS_BUILD_ARTIFACTS.json').write_text(json.dumps({
        'label': RELEASE_LABEL + ' SingleRuntime', 'file_version': RELEASE_FILE_VERSION,
        'host_architecture': args.host_arch, 'runtime_architecture': 'x86',
        'cursor_protocol': CURSOR_PROTOCOL, 'console_ui': 'webview2' if args.webview2 else 'native',
        'neural_helper_verified': args.neural_helper, 'neural_runtime_tested': False,
        'verified': recs, 'runtime_tested': False, 'multiplayer_tested': False,
        'dependency_check': {2: 'two', 3: 'three', 4: 'four'}[len(recs)] + ' project binaries use Windows system DLLs' + ('; pinned x64 Microsoft WebView2 loader bundled; Evergreen Runtime required' if args.webview2 else ' only'),
    }, ensure_ascii=False, indent=2), encoding='utf-8')
    if args.host_arch == 'x86':
        print(f'PASS: 2 current PE32 x86 outputs; hooks exported; ABI/Shared size match; version {RELEASE_FILE_VERSION}')
    else:
        print(f'PASS: {len(recs)} current outputs; x64 host/x86 runtime/broker; hooks exported; ABI/Shared size match; version {RELEASE_FILE_VERSION}')
    if args.neural_helper:
        print('PASS: optional x64 neural helper PE/manifest/system dependencies; DLSS hardware/runtime execution not tested')
    if args.webview2:
        print('PASS: pinned x64 WebView2 loader, license notices and host integration; no WebView2 dependency in the x86 game components')
    print('PASS: no unbundled third-party or compiler-runtime DLL dependencies')
    print(f'PASS: actual EXE RT_MANIFEST/1 enables {args.host_arch} Common Controls 6 for subclass helper imports')
    print('NOTE: static validation does not execute the Windows loader or the game')
    return 0


if __name__ == '__main__':
    sys.exit(main())
