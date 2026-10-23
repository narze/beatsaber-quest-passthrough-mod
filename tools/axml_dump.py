"""Low-level dump of a binary AndroidManifest.xml (string pool flags,
resource map, and the attributes of the first N start tags)."""
import struct
import sys


def strings(data, off):
    _, hsize, size, count, scount, flags, sstart, _ = struct.unpack_from("<HHIIIIII", data, off)
    utf8 = bool(flags & 0x100)
    offs = struct.unpack_from(f"<{count}I", data, off + hsize)
    out = []
    for o in offs:
        p = off + sstart + o
        if utf8:
            n = data[p]; p += 2 if n & 0x80 else 1
            n = data[p]; hi = n & 0x80
            ln = ((n & 0x7F) << 8 | data[p + 1]) if hi else n
            p += 2 if hi else 1
            out.append(data[p:p + ln].decode("utf-8", "replace"))
        else:
            ln = struct.unpack_from("<H", data, p)[0]
            out.append(data[p + 2:p + 2 + ln * 2].decode("utf-16le", "replace"))
    return out, flags, size


def main(path, tags=2):
    data = open(path, "rb").read()
    off = 8
    pool, resmap = None, []
    seen = 0
    while off < len(data) and seen < tags:
        typ, hsize, size = struct.unpack_from("<HHI", data, off)
        if typ == 0x0001:
            pool, flags, _ = strings(data, off)
            print(f"stringpool count={len(pool)} flags=0x{flags:x}")
        elif typ == 0x0180:
            resmap = list(struct.unpack_from(f"<{(size - 8) // 4}I", data, off + 8))
            print(f"resmap n={len(resmap)}")
        elif typ == 0x0102:
            ns, name, astart, asize, acount = struct.unpack_from("<iiHHH", data, off + 16)
            print(f"<{pool[name]}> attrStart={astart} attrSize={asize} count={acount}")
            for i in range(acount):
                a = off + 16 + astart + i * asize
                ans, aname, raw, vsize, _, vtype, vdata = struct.unpack_from("<iiiHBBI", data, a)
                rid = resmap[aname] if 0 <= aname < len(resmap) else None
                print(f"  ns={ans} name[{aname}]={pool[aname]!r} rid={hex(rid) if rid else None} "
                      f"raw={raw} type=0x{vtype:x} data={vdata}")
            seen += 1
        off += size


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 2)
