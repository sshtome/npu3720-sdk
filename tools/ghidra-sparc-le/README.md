# Ghidra SPARC v8 Little-Endian (`sparc:LE:32:LE`)

Intel NPU 3720 LeonRT runs SPARC v8 in **little-endian** mode, while upstream Ghidra only ships big-endian `SparcV9` SLEIGH definitions.

## Setup

1. Locate `$GHIDRA_DIR/Ghidra/Processors/Sparc/data/languages/`.
2. Copy `SparcV9.sinc` to `SparcV9_le.sinc` and change `define endian=big;` to `define endian=little;`.
3. Copy `SparcV9_32.slaspec` to `SparcV9_32_le.slaspec` and include `SparcV9_le.sinc` instead of `SparcV9.sinc`.
4. Add a `<language>` entry to `SparcV9.ldefs` with `id="sparc:LE:32:LE"`, `endian="little"`, `processor="Sparc"`, `size="32"`, `slafile="SparcV9_32_le.sla"`, `processorspec="SparcV9.pspec"`, `cspec="SparcV9_32.cspec"`.
5. Compile the `.sla` file with Ghidra's `sleigh` compiler or by opening a binary with `sparc:LE:32:LE` in Ghidra.
