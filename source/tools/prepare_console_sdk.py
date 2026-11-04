#!/usr/bin/env python3
"""Prepare the pinned official x64 WebView2 SDK without vendoring it in Git."""
from pathlib import Path
import argparse
import base64
import hashlib
import json
import os
import struct
import sys
import tempfile
import urllib.request
import zipfile

VERSION = '1.0.4314.35'
PACKAGE_URL = ('https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/'
               + VERSION + '/microsoft.web.webview2.' + VERSION + '.nupkg')
# Verified against the official NuGet catalog entry for this exact version.
PACKAGE_SHA512 = 'cOaiVqP3BmQHMnxGpZzANM22tUXgycrisyJHjIGNzIaMsm8iXj7Kx+ceYg+RbYwJtzT009qfcFTocvVLY+ekMw=='
PACKAGE_SHA256 = '90778deb1b77e36292828b4b5833c9255c27533f40e0ddd55891ae718944edc4'
CATALOG_URL = 'https://api.nuget.org/v3/catalog0/data/2026.10.23.06.50.10/microsoft.web.webview2.1.0.4314.35.json'
FILES = {
    'include/WebView2.h': ('build/native/include/WebView2.h', 2951155, '4e47763725f507235e8a12f35b9f41d7e348586153414c39fc1e9108875c181f'),
    'include/WebView2EnvironmentOptions.h': ('build/native/include/WebView2EnvironmentOptions.h', 19500, '25a2de86c15fdc2fbb0e5d9cb01f17cadb8accc7ed4a5a41e875a5e7b866c991'),
    'bin/WebView2Loader.dll': ('runtimes/win-x64/native/WebView2Loader.dll', 194912, 'd3fce90bdeecb08596e377dea6a316354dd8949e32ff09b89d51be00c14e21b6'),
    'licenses/LICENSE.txt': ('LICENSE.txt', 1487, '0af8f1b807512aae39c2ac1aa4d0cae65cabecb6fd554b8439a5162a0d6eca55'),
    'licenses/NOTICE.txt': ('NOTICE.txt', 3894, '106423785c5b7eba0a8e61d1837f2132e9c828e20ad530f565d981c1df60dd90'),
}
LOADER_SHA256 = FILES['bin/WebView2Loader.dll'][2]
DEFAULT_OUTPUT = Path(__file__).resolve().parents[1] / '.deps' / 'webview2' / VERSION
# The official WebView2 header expects this Windows SDK ABI type. Zig 0.13's
# MinGW headers omit EventToken.h; keep the compatibility declaration separate
# from, and never modify, the pinned Microsoft header.
EVENT_TOKEN_HEADER = ('#pragma once\n'
                      '#ifndef __EventToken_h__\n#define __EventToken_h__\n'
                      'typedef struct EventRegistrationToken { __int64 value; } EventRegistrationToken;\n'
                      '#endif\n')


def prepare_compatibility_header(output):
    path = output / 'include' / 'EventToken.h'
    data = EVENT_TOKEN_HEADER.encode('ascii')
    if not path.is_file() or path.read_bytes() != data:
        path.write_bytes(data)


def validate_bytes(name, data):
    _, size, digest = FILES[name]
    if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
        raise ValueError('Pinned WebView2 SDK file differs: ' + name)
    if name.endswith('.dll'):
        if data[:2] != b'MZ':
            raise ValueError('WebView2 loader is not a PE file')
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        if data[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', data, pe + 4)[0] != 0x8664:
            raise ValueError('WebView2 loader must be x64')


def validate_installed(output):
    manifest = json.loads((output / 'sdk_manifest.json').read_text('utf-8-sig'))
    if manifest.get('version') != VERSION or manifest.get('package_sha512_base64') != PACKAGE_SHA512:
        raise ValueError('WebView2 SDK manifest is not the pinned package')
    for name in FILES:
        validate_bytes(name, (output / name).read_bytes())
    return output


def prepare(output, package=None, offline=False):
    output = Path(output).resolve()
    try:
        validate_installed(output)
        prepare_compatibility_header(output)
        return output
    except (OSError, ValueError, KeyError):
        if offline and not package:
            raise ValueError('Pinned SDK cache unavailable; supply --package or prepare it with network access')
    if package:
        data = Path(package).read_bytes()
    else:
        request = urllib.request.Request(PACKAGE_URL, headers={'User-Agent': 'RA2-Toolbox-SDK-Preparation'})
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
    digest = base64.b64encode(hashlib.sha512(data).digest()).decode('ascii')
    if digest != PACKAGE_SHA512 or hashlib.sha256(data).hexdigest() != PACKAGE_SHA256:
        raise ValueError('Microsoft.Web.WebView2 package hash does not match the pinned official package')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='webview2-', dir=output.parent) as temporary:
        archive = Path(temporary) / 'sdk.nupkg'
        archive.write_bytes(data)
        metadata = {}
        with zipfile.ZipFile(archive) as package_zip:
            if package_zip.testzip() is not None:
                raise ValueError('Corrupt WebView2 NuGet package')
            for name, (source, _, _) in FILES.items():
                content = package_zip.read(source)
                validate_bytes(name, content)
                destination = output / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                pending = Path(temporary) / Path(name).name
                pending.write_bytes(content)
                os.replace(pending, destination)
                metadata[name] = {'source': source, 'bytes': len(content), 'sha256': hashlib.sha256(content).hexdigest()}
        info = {'package': 'Microsoft.Web.WebView2', 'version': VERSION,
                'package_url': PACKAGE_URL, 'package_sha512_base64': PACKAGE_SHA512,
                'package_sha256': PACKAGE_SHA256, 'official_catalog': CATALOG_URL,
                'host_architecture': 'x64', 'files': metadata}
        pending = Path(temporary) / 'sdk_manifest.json'
        pending.write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
        os.replace(pending, output / 'sdk_manifest.json')
    validate_installed(output)
    prepare_compatibility_header(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path(os.environ.get('RA2_WEBVIEW2_SDK', DEFAULT_OUTPUT)))
    parser.add_argument('--package', type=Path, default=os.environ.get('RA2_WEBVIEW2_PACKAGE'))
    parser.add_argument('--offline', action='store_true', help='use a verified existing SDK cache or a supplied package only')
    parser.add_argument('--print-path', action='store_true')
    args = parser.parse_args()
    try:
        output = prepare(args.output, args.package, args.offline)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print('WebView2 SDK preparation failed: ' + str(error), file=sys.stderr)
        return 1
    print(str(output) if args.print_path else 'WebView2 SDK ' + VERSION + ' verified: ' + str(output))
    return 0


if __name__ == '__main__':
    sys.exit(main())
