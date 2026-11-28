#!/usr/bin/env python3
"""Minimal ELF32 sparcel (SPARC v8 LE) static linker for NPU 3720."""
import argparse
import struct

R_SPARC_32 = 3
R_SPARC_WDISP30 = 7
R_SPARC_WDISP22 = 8
R_SPARC_HI22 = 9
R_SPARC_22 = 10
R_SPARC_13 = 11
R_SPARC_LO10 = 12
R_SPARC_PC10 = 16
R_SPARC_PC22 = 17
R_SPARC_UA32 = 23


def parse_elf(path):
    with open(path, 'rb') as f:
        d = f.read()
    assert d[:4] == b'\x7fELF' and d[4] == 1 and d[5] == 1, f"{path}: not ELF32 LSB"
    (e_shoff,) = struct.unpack_from('<I', d, 0x20)
    (e_shentsize, e_shnum, e_shstrndx) = struct.unpack_from('<HHH', d, 0x2E)

    shdrs = [
        struct.unpack_from('<IIIIIIIIII', d, e_shoff + i * e_shentsize)
        for i in range(e_shnum)
    ]
    names_d = d[shdrs[e_shstrndx][4]:shdrs[e_shstrndx][4] + shdrs[e_shstrndx][5]]

    def sname(i):
        s = names_d[i:]
        return s[:s.index(b'\x00')].decode()

    names = [sname(s[0]) for s in shdrs]
    symtab_i = next(i for i, s in enumerate(shdrs) if s[1] == 2)
    strtab = shdrs[shdrs[symtab_i][6]]
    sd = d[shdrs[symtab_i][4]:shdrs[symtab_i][4] + shdrs[symtab_i][5]]
    strd = d[strtab[4]:strtab[4] + strtab[5]]

    syms = []
    for i in range(shdrs[symtab_i][5] // 16):
        name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', sd, i * 16)
        nm = strd[name:strd.index(b'\x00', name)].decode()
        syms.append(dict(name=nm, value=value, size=size, info=info, shndx=shndx))

    secs = []
    for i, s in enumerate(shdrs):
        if s[1] in (1, 8):  # PROGBITS / NOBITS
            secs.append(dict(
                name=names[i],
                type=s[1],
                flags=s[2],
                orig=i,
                data=d[s[4]:s[4] + s[5]] if s[1] == 1 else None,
                align=max(s[8], 1),
                size=s[5],
            ))

    relas = []
    for i, s in enumerate(shdrs):
        if s[1] in (4, 9):  # RELA
            tname = names[s[7]]
            rd = d[s[4]:s[4] + s[5]]
            for j in range(s[5] // 12):
                roff, rinfo, radd = struct.unpack_from('<IIi', rd, j * 12)
                relas.append(dict(sec=tname, off=roff, sym=rinfo >> 8, typ=rinfo & 0xFF, add=radd))

    return secs, syms, relas


def link(inputs, output, base=0x84803000, stack_top=0x87FF0000, map_path=None):
    objs = [parse_elf(o) for o in inputs]

    globals_ = {}
    for oi, (secs, syms, relas) in enumerate(objs):
        for si, s in enumerate(syms):
            if s['name'] and (s['info'] >> 4) == 1 and s['shndx'] != 0:
                globals_[s['name']] = (oi, si)

    order = ['.text.entry', '.text', '.text.lnn.entry', '.text.lnn', '.rodata', '.data', '.bss']
    placed = {}
    addr = base

    for name in order:
        for oi, (secs, _, _) in enumerate(objs):
            for s in secs:
                if s['name'] == name:
                    a = max(s['align'], 4)
                    if name in ('.text', '.text.lnn.entry') and (oi == 0 or name == '.text.lnn.entry'):
                        a = 4096
                    addr = (addr + a - 1) & ~(a - 1)
                    placed[(oi, s['orig'])] = addr
                    addr += s['size']

    # Remaining allocatable sections (.rodata.*, etc.)
    for oi, (secs, _, _) in enumerate(objs):
        for s in secs:
            if (oi, s['orig']) not in placed and (s['flags'] & 2):
                a = max(s['align'], 4)
                addr = (addr + a - 1) & ~(a - 1)
                placed[(oi, s['orig'])] = addr
                addr += s['size']

    bss_addrs = []
    for (oi, si), a in placed.items():
        for s in objs[oi][0]:
            if s['orig'] == si and s['name'] == '.bss':
                bss_addrs.append((a, a + s['size']))
    bss_base = min(x[0] for x in bss_addrs) if bss_addrs else 0
    bss_end = max(x[1] for x in bss_addrs) if bss_addrs else 0

    def resolve(oi, si):
        s = objs[oi][1][si]
        if s['shndx'] == 0:
            if s['name'] == '__bss_start':
                return bss_base
            if s['name'] == '__bss_end':
                return bss_end
            if s['name'] == '_stack_top':
                return stack_top
            if s['name'] in globals_:
                return resolve(*globals_[s['name']])
            raise ValueError(f"undefined symbol: {s['name']}")
        if s['shndx'] == 0xFFF1:
            return s['value']
        return placed[(oi, s['shndx'])] + s['value']

    images = {}
    for (oi, si), _ in placed.items():
        sec = next(s for s in objs[oi][0] if s['orig'] == si)
        if sec['data'] is not None:
            images[(oi, si)] = bytearray(sec['data'])

    for oi, (secs, _, relas) in enumerate(objs):
        for r in relas:
            si = next(s['orig'] for s in secs if s['name'] == r['sec'])
            if (oi, si) not in images:
                continue
            img = images[(oi, si)]
            val = resolve(oi, r['sym']) + r['add']
            P = placed[(oi, si)] + r['off']
            w = struct.unpack_from('<I', img, r['off'])[0]
            t = r['typ']
            if t == R_SPARC_32:
                w = val & 0xFFFFFFFF
            elif t == R_SPARC_WDISP30:
                w = (w & 0xC0000000) | (((val - P) >> 2) & 0x3FFFFFFF)
            elif t == R_SPARC_WDISP22:
                w = (w & 0xFFC00000) | (((val - P) >> 2) & 0x3FFFFF)
            elif t == R_SPARC_HI22:
                w = (w & 0xFFC00000) | ((val >> 10) & 0x3FFFFF)
            elif t == R_SPARC_LO10:
                w = (w & 0xFFFFE000) | (val & 0x3FF)
            elif t == R_SPARC_13:
                w = (w & 0xFFFFE000) | (val & 0x1FFF)
            elif t == R_SPARC_22:
                w = (w & 0xFFC00000) | (val & 0x3FFFFF)
            elif t == R_SPARC_PC10:
                w = (w & 0xFFFFFC00) | ((val - P) & 0x3FF)
            elif t == R_SPARC_PC22:
                w = (w & 0xFFC00000) | (((val - P) >> 10) & 0x3FFFFF)
            elif t == R_SPARC_UA32:
                img[r['off']:r['off'] + 4] = struct.pack('<I', val & 0xFFFFFFFF)
                continue
            else:
                raise ValueError(f"unsupported relocation type={t} @ {r['sec']}+{r['off']:#x}")
            struct.pack_into('<I', img, r['off'], w)

    end = max(placed[k] + len(images[k]) for k in images)
    out = bytearray(end - base)
    for k, img in images.items():
        off = placed[k] - base
        out[off:off + len(img)] = img

    with open(output, 'wb') as f:
        f.write(bytes(out))

    lines = [f"{output}: {len(out)} bytes, {base:#x}..{base + len(out):#x}"]
    for k in sorted(placed, key=lambda k: placed[k]):
        oi, si = k
        sec = next(s for s in objs[oi][0] if s['orig'] == si)
        lines.append(f"  [{inputs[oi]}] {sec['name']:16s} @{placed[k]:#x} ({sec['size']}B)")
    lines.append(f"  __bss_start={bss_base:#x} __bss_end={bss_end:#x} _stack_top={stack_top:#x}")
    if map_path:
        sym_lines = ["", "Global symbols:"]
        for name in sorted(globals_):
            sym_lines.append(f"  {resolve(*globals_[name]):#010x} {name}")
        with open(map_path, 'w') as mf:
            mf.write("\n".join(lines + sym_lines) + "\n")
    print("\n".join(lines))


def main():
    p = argparse.ArgumentParser(description="ELF32 sparcel static linker for NPU 3720")
    p.add_argument("inputs", nargs="+", help="Input ELF32 sparcel .o files")
    p.add_argument("-o", "--output", default="fw.bin", help="Output flat binary")
    p.add_argument("--base", type=lambda x: int(x, 0), default=0x84803000)
    p.add_argument("--stack", type=lambda x: int(x, 0), default=0x87FF0000)
    p.add_argument("--map", default=None, help="Optional link map file")
    args = p.parse_args()
    link(args.inputs, args.output, args.base, args.stack, args.map)


if __name__ == "__main__":
    main()
