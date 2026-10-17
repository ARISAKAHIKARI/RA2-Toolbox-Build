"""Small, dependency-free PE32 reader for release checks (not an executable loader)."""
import struct


class Image:
    def __init__(self, data):
        self.data = data
        if self.take(0, 2) != b'MZ':
            raise ValueError('MZ header missing')
        self.nt = self.u32(0x3c)
        if self.take(self.nt, 4) != b'PE\0\0':
            raise ValueError('PE signature missing')
        self.machine, self.section_count = self.unpack('<HH', self.nt + 4)
        self.optional = self.nt + 24
        if self.machine != 0x14c or self.u16(self.optional) != 0x10b:
            raise ValueError('not x86 PE32')
        self.characteristics = self.u16(self.nt + 22)
        self.entry = self.u32(self.optional + 16)
        self.sections = []
        start = self.optional + self.u16(self.nt + 20)
        for i in range(self.section_count):
            at = start + i * 40
            label = self.take(at, 8).rstrip(b'\0').decode('ascii')
            virtual_size, rva, size, offset = self.unpack('<4I', at + 8)
            self.sections.append((label, rva, size, offset, virtual_size))
        self.offset(self.entry, 1)

    def take(self, at, size):
        if at < 0 or size < 0 or at + size > len(self.data):
            raise ValueError('truncated PE image')
        return self.data[at:at + size]

    def unpack(self, fmt, at):
        return struct.unpack(fmt, self.take(at, struct.calcsize(fmt)))

    def u16(self, at):
        return self.unpack('<H', at)[0]

    def u32(self, at):
        return self.unpack('<I', at)[0]

    def offset(self, rva, size=1):
        for _, base, raw_size, raw_at, _ in self.sections:
            delta = rva - base
            if 0 <= delta and delta + size <= raw_size:
                self.take(raw_at + delta, size)
                return raw_at + delta
        raise ValueError(f'unmapped RVA {rva:#x}')

    def at_rva(self, rva, size):
        return self.take(self.offset(rva, size), size)

    def cstring(self, rva):
        result = bytearray()
        for i in range(4096):
            b = self.at_rva(rva + i, 1)[0]
            if not b:
                return result.decode('ascii')
            result.append(b)
        raise ValueError('unterminated PE string')

    def directory(self, number):
        if self.u32(self.optional + 92) <= number:
            return 0, 0
        return self.unpack('<II', self.optional + 96 + number * 8)

    def resource_blobs(self, resource_type, resource_id):
        """Read exact type/id language leaves, not arbitrary resource strings."""
        root, size = self.directory(2)
        if not root or size < 16:
            return []

        def take(relative, count):
            if relative < 0 or relative + count > size:
                raise ValueError('resource directory outside declared bounds')
            return self.at_rva(root + relative, count)

        def entries(relative):
            named, ids = struct.unpack_from('<HH', take(relative, 16), 12)
            return [struct.unpack('<II', take(relative + 16 + i * 8, 8))
                    for i in range(named + ids)]

        def child(relative, wanted):
            found = [target for key, target in entries(relative) if key == wanted]
            if not found:
                return None
            if len(found) != 1 or not found[0] & 0x80000000:
                raise ValueError('invalid resource directory branch')
            return found[0] & 0x7fffffff

        kind = child(0, resource_type)
        if kind is None:
            return []
        name = child(kind, resource_id)
        if name is None:
            return []
        result = []
        for language, target in entries(name):
            if target & 0x80000000 or language & 0x80000000:
                raise ValueError('invalid resource language leaf')
            rva, count, _, _ = struct.unpack('<4I', take(target, 16))
            result.append((language, self.at_rva(rva, count)))
        return result

    def exports(self):
        rva, size = self.directory(0)
        if not rva:
            return {}
        at = self.offset(rva, 40)
        count, names = self.unpack('<II', at + 20)
        funcs, pointers, ordinals = self.unpack('<III', at + 28)
        if count > 65536 or names > 65536:
            raise ValueError('invalid export count')
        result = {}
        for i in range(names):
            name = self.cstring(self.unpack('<I', self.offset(pointers + i * 4, 4))[0])
            ordinal = self.unpack('<H', self.offset(ordinals + i * 2, 2))[0]
            if ordinal >= count:
                raise ValueError('invalid export ordinal')
            target = self.unpack('<I', self.offset(funcs + ordinal * 4, 4))[0]
            if rva <= target < rva + size:
                raise ValueError('forwarded project export is unsupported')
            self.offset(target, 1)
            result[name] = target
        return result

    def imports(self):
        rva, size = self.directory(1)
        result = {}
        if not rva:
            return result
        for i in range(min(size // 20, 4096)):
            row = self.unpack('<5I', self.offset(rva + i * 20, 20))
            if not any(row):
                return result
            name = self.cstring(row[3])
            functions = []
            for j in range(65536):
                value = self.unpack('<I', self.offset((row[0] or row[4]) + j * 4, 4))[0]
                if not value:
                    break
                functions.append(f'ordinal:{value & 0xffff}' if value & 0x80000000
                                 else self.cstring(value + 2))
            else:
                raise ValueError('unterminated import thunk table')
            result[name] = functions
        raise ValueError('unterminated import directory')

    def resource_bytes(self):
        for name, _, size, offset, _ in self.sections:
            if name == '.rsrc':
                return self.take(offset, size)
        return b''
