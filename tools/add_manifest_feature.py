"""Add a <uses-feature> to a binary AndroidManifest.xml.

Usage: add_manifest_feature.py IN_AXML OUT_AXML FEATURE [FEATURE ...]

Decodes with androguard (it accepts the string pool that MBF writes),
adds the features, and encodes again with pyaxml. Then it decodes the
result again to check that only the new elements differ.
"""
import struct
import sys

from androguard.core.axml import AXMLPrinter
from lxml import etree
from pyaxml import axml

ANDROID_NS = "http://schemas.android.com/apk/res/android"


def decode(data: bytes) -> etree._Element:
    return AXMLPrinter(data).get_xml_obj()


def canon(root) -> list:
    """Elements in document order, attributes as a sorted list."""
    return [(e.tag, sorted(e.attrib.items()), (e.text or "").strip()) for e in root.iter()]


def features(root) -> list[str]:
    return [e.get(f"{{{ANDROID_NS}}}name") for e in root.findall("uses-feature")]


def sort_attributes(data: bytes) -> bytes:
    """Sort the attributes of every start tag like aapt2 does: attributes
    with a resource ID first, ascending by ID, then the rest in order.

    Android's RetrieveAttributes walks attributes in resource-ID order. pyaxml
    keeps source order, so on the device later attributes (such as versionCode
    after compileSdkVersion) are silently skipped."""
    buf = bytearray(data)
    off, resmap = 8, []
    while off < len(buf):
        typ, _, size = struct.unpack_from("<HHI", buf, off)
        if typ == 0x0180:
            resmap = list(struct.unpack_from(f"<{(size - 8) // 4}I", buf, off + 8))
        elif typ == 0x0102:
            start, asize, count, id_i, cls_i, sty_i = struct.unpack_from("<HHHHHH", buf, off + 24)
            base = off + 16 + start
            recs = [bytes(buf[base + i * asize:base + (i + 1) * asize]) for i in range(count)]

            def key(i):
                name = struct.unpack_from("<i", recs[i], 4)[0]
                rid = resmap[name] if 0 <= name < len(resmap) else 0
                return (0, rid, i) if rid else (1, 0, i)

            order = sorted(range(count), key=key)
            for j, i in enumerate(order):
                buf[base + j * asize:base + (j + 1) * asize] = recs[i]
            remap = lambda x: order.index(x - 1) + 1 if x else 0
            struct.pack_into("<HHH", buf, off + 30, remap(id_i), remap(cls_i), remap(sty_i))
        off += size
    return bytes(buf)


def main(src: str, dst: str, wanted: list[str]) -> None:
    root = decode(open(src, "rb").read())
    before = canon(root)
    have = features(root)
    added = [f for f in wanted if f not in have]
    anchor = root.findall("uses-feature")
    index = list(root).index(anchor[-1]) + 1 if anchor else 0
    for name in added:
        elt = etree.Element("uses-feature")
        elt.set(f"{{{ANDROID_NS}}}name", name)
        elt.set(f"{{{ANDROID_NS}}}required", "true")
        root.insert(index, elt)
        index += 1

    out = axml.AXML()
    out.from_xml(root)
    data = sort_attributes(out.pack())

    check = decode(data)
    for name in wanted:
        assert name in features(check), f"{name} missing after encode"
    for elt in check.findall("uses-feature"):
        if elt.get(f"{{{ANDROID_NS}}}name") in added:
            elt.getparent().remove(elt)
    assert canon(check) == before, "encode changed other elements"

    open(dst, "wb").write(data)
    print(f"added={added} bytes={len(data)}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
