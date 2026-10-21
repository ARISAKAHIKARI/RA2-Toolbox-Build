#!/usr/bin/env python3
"""Verify PE32 architecture, ABI exports, dependency closure and release version."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import xml.etree.ElementTree as ET
# Windows embedded Python uses python312._pth and omits the script directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pe32_image import Image

ROOT = Path(__file__).resolve().parent.parent
NAMES = ('RA2Toolbox.exe','GameRuntime.dll')
HOOKS = {'GameRuntime.dll':('GameRuntimeHook','GameRuntimePostHook','GameRuntimeInfo','AssistProtocolInfo','AssistMappingPrefix','ManageGameEntry','GameEntryManagerInfo')}
SYSTEM = {'kernel32.dll', 'user32.dll', 'gdi32.dll', 'winmm.dll', 'gdiplus.dll',
          'comctl32.dll', 'shell32.dll', 'ole32.dll', 'comdlg32.dll', 'advapi32.dll',
          'msvcrt.dll', 'ucrtbase.dll', 'ntdll.dll', 'version.dll', 'ws2_32.dll',
          'shlwapi.dll', 'oleaut32.dll', 'imm32.dll'}


def verify_host_manifest(pe):
    # GetWindowSubclass is imported by name. Windows needs the v6 activation
    # dependency in CREATEPROCESS_MANIFEST_RESOURCE_ID (1), before main runs.
    # A declaration in an unused module manifest or arbitrary .rsrc text is
    # insufficient. Check every language of the final executable's resource.
    leaves = pe.resource_blobs(24, 1)
    if not leaves:
        raise ValueError('EXE process manifest resource RT_MANIFEST/1 missing')
    namespace = {'a': 'urn:schemas-microsoft-com:asm.v1'}
    for language, blob in leaves:
        root = ET.fromstring(blob)
        if root.tag != '{urn:schemas-microsoft-com:asm.v1}assembly':
            raise ValueError('EXE process manifest has an incorrect XML namespace')
        dependencies = root.findall('a:dependency/a:dependentAssembly/a:assemblyIdentity', namespace)
        controls = [item for item in dependencies
                    if item.get('name') == 'Microsoft.Windows.Common-Controls']
        if len(controls) != 1:
            raise ValueError('EXE manifest lacks one Common Controls v6 dependency; GetWindowSubclass cannot load')
        item = controls[0]
        expected = {'type': 'win32', 'version': '6.0.0.0', 'processorArchitecture': 'x86',
                    'publicKeyToken': '6595b64144ccf1df', 'language': '*'}
        if any(item.get(key) != value for key, value in expected.items()):
            raise ValueError('EXE Common Controls dependency identity/version/architecture mismatch')
    return {'resource_type': 24, 'resource_id': 1, 'languages': [language for language, _ in leaves],
            'name': 'Microsoft.Windows.Common-Controls', 'version': '6.0.0.0', 'architecture': 'x86'}


def verify(root):
    legacy = json.loads((root / 'BUILD_SOURCE_MANIFEST.json').read_text('utf-8')).get('legacy_binary_sha256', {})
    recs, images, errors = {}, {}, []
    for name in NAMES:
        try:
            buf = (root / name).read_bytes()
            pe = Image(buf)
            if bool(pe.characteristics & 0x2000) != name.endswith('.dll'):
                raise ValueError('unexpected EXE/DLL type')
            digest = hashlib.sha256(buf).hexdigest()
            if digest == legacy.get(name):
                raise ValueError('unchanged v0.4.1 binary')
            exports = pe.exports()
            if name.endswith('.dll') and ({x.split('@')[0] for x in exports}!=set(HOOKS[name]) or any('@' in x and exports[x]!=exports[x.split('@')[0]] for x in exports)):
                raise ValueError('unexpected component export bypass')
            required = HOOKS.get(name, ('AssistHostProtocolInfo',))
            if any(symbol not in exports for symbol in required):
                raise ValueError('required export missing: ' + ', '.join(required))
            imports = pe.imports()
            unknown = [x for x in imports if x.lower() not in SYSTEM
                       and not x.lower().startswith(('api-ms-win-', 'ext-ms-win-'))]
            if unknown:
                raise ValueError('unbundled non-system dependency: ' + ', '.join(unknown))
            if "1.0.30.0".encode("utf-16le") not in pe.resource_bytes():
                raise ValueError("file version resource mismatch")
            images[name] = pe
            recs[name] = {'bytes': len(buf), 'sha256': digest, 'machine': 'PE32 x86',
                          'sections': pe.section_count, 'entry_rva': hex(pe.entry),
                          'exports': list(exports), 'import_dlls': list(imports),
                          'imports': imports}
        except Exception as ex:
            errors.append(f'{name}: {ex}')
    if not errors:
        try:
            host, bridge = images['RA2Toolbox.exe'], images['GameRuntime.dll']
            recs['RA2Toolbox.exe']['common_controls_activation'] = verify_host_manifest(host)
            a = host.at_rva(host.exports()['AssistHostProtocolInfo'], 8)
            b = bridge.at_rva(bridge.exports()['AssistProtocolInfo'], 8)
            protocol, shared_bytes = struct.unpack('<II', a)
            if a != b or protocol != 55 or not shared_bytes:
                raise ValueError('host/bridge protocol or Shared structure size mismatch')
            marker = 'Local\\RA2Assist_v055_\0'.encode('utf-16le')
            if bridge.at_rva(bridge.exports()['AssistMappingPrefix'], len(marker)) != marker:
                raise ValueError('incorrect exported mapping namespace')
            manager = images['GameRuntime.dll']
            if struct.unpack('<II',manager.at_rva(manager.exports()['GameEntryManagerInfo'],8)) != (3,108):
                raise ValueError('entry manager API mismatch')
            if struct.unpack('<II', manager.at_rva(manager.exports()['GameRuntimeInfo'],8)) != (4,176):
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
            if '1.0.30.0'.encode('utf-16le') not in resource:
                raise ValueError('EXE version resource mismatch')
            for text in (b'requireAdministrator', b'PerMonitorV2', b'1.0.30.0'):
                if text not in resource:
                    raise ValueError('EXE manifest missing: ' + text.decode())
            for rec in recs.values():
                rec['assist_protocol'] = protocol
                rec['assist_shared_bytes'] = shared_bytes
        except Exception as ex:
            errors.append(str(ex))
    return recs, errors


def main():
    for name in ('BUILD-VERIFIED-v130.tag', 'WINDOWS_BUILD_ARTIFACTS.json'):
        (ROOT / name).unlink(missing_ok=True)
    recs, errors = verify(ROOT)
    if errors:
        print('BUILD NOT VERIFIED:\n' + '\n'.join(errors))
        return 1
    (ROOT / 'BUILD-VERIFIED-v130.tag').write_text(
        'v1.0.30.0 SingleRuntime: PE32/ABI/import/version checks passed\n'
        'Game runtime and multiplayer not tested\n', encoding='utf-8')
    (ROOT / 'WINDOWS_BUILD_ARTIFACTS.json').write_text(json.dumps({
        'label': '1.0.30.0 SingleRuntime', 'file_version': '1.0.30.0',
        'verified': recs, 'runtime_tested': False, 'multiplayer_tested': False,
        'dependency_check': 'two project binaries import Windows system DLLs only',
    }, ensure_ascii=False, indent=2), encoding='utf-8')
    print('PASS: 2 current PE32 x86 outputs; hooks exported; ABI/Shared size match; version 1.0.30.0')
    print('PASS: no unbundled third-party or compiler-runtime DLL dependencies')
    print('PASS: actual EXE RT_MANIFEST/1 enables x86 Common Controls 6 for subclass helper imports')
    print('NOTE: static validation does not execute the Windows loader or the game')
    return 0


if __name__ == '__main__':
    sys.exit(main())

