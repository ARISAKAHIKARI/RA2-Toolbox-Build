"""Check the compiled EXE's actual process manifest, including negative PE fixtures."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools'))
from pe32_image import Image
from verify_windows_outputs import verify_host_manifest

data = (root / 'RA2Toolbox.exe').read_bytes()
image = Image(data)
activation = verify_host_manifest(image)
assert activation['resource_id'] == 1 and activation['version'] == '6.0.0.0'
assert activation['architecture'] == 'x86'
imports = next(items for name, items in image.imports().items() if name.lower() == 'comctl32.dll')
assert 'GetWindowSubclass' in imports
print('PASS final EXE imports GetWindowSubclass and its RT_MANIFEST/1 selects Common Controls 6')

# All source manifests must agree even though the unified link only embeds
# suite/source/app.manifest. This is precisely what the old source-only check missed.
ns = {'a': 'urn:schemas-microsoft-com:asm.v1'}
for module in ('suite', 'assist', 'cursor', 'unit_monitor'):
    tree = ET.fromstring((root / module / 'source/app.manifest').read_bytes())
    dependencies = tree.findall('a:dependency/a:dependentAssembly/a:assemblyIdentity', ns)
    assert any(item.get('name') == 'Microsoft.Windows.Common-Controls'
               and item.get('version') == '6.0.0.0'
               and item.get('processorArchitecture') == 'x86' for item in dependencies), module
print('PASS all four source manifests carry the same required dependency')

def rejected(payload):
    try:
        verify_host_manifest(Image(payload))
    except ValueError:
        return
    raise AssertionError('broken activation dependency escaped the build gate')

for old, new in (
    (b'Microsoft.Windows.Common-Controls', b'Microsoft.Windows.Legacy-Controls'),
    (b'version="6.0.0.0"', b'version="5.0.0.0"'),
    (b'processorArchitecture="x86"', b'processorArchitecture="x64"'),
    (b'6595b64144ccf1df', b'0000000000000000'),
    (b'urn:schemas-microsoft-com:asm.v1', b'urn:schemas-microsoft-com:asm.v0'),
):
    assert len(old) == len(new) and old in data
    rejected(data.replace(old, new))
print('PASS missing dependency, wrong version, wrong architecture, wrong token and wrong XML namespace fail closed')

# Keep valid manifest text in .rsrc, but move it from the process resource id
# to id 2. A loose byte/string scan would accept this invalid executable.
resource, _ = image.directory(2)
named, ids = image.unpack('<HH', image.offset(resource + 12, 4))
type_branch = None
for index in range(named + ids):
    key, target = image.unpack('<II', image.offset(resource + 16 + index * 8, 8))
    if key == 24:
        type_branch = resource + (target & 0x7fffffff)
        break
assert type_branch is not None
named, ids = image.unpack('<HH', image.offset(type_branch + 12, 4))
broken = bytearray(data)
for index in range(named + ids):
    at = image.offset(type_branch + 16 + index * 8, 8)
    if image.u32(at) == 1:
        broken[at:at + 4] = (2).to_bytes(4, 'little')
        break
else:
    raise AssertionError('expected numeric manifest id 1')
assert b'Microsoft.Windows.Common-Controls' in broken
rejected(bytes(broken))
print('PASS valid XML in the wrong resource id cannot satisfy the loader gate')
