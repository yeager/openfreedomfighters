# Private disassembly status

The supported Steam executable was fully disassembled on 2026-09-04. Disassembly and analysis databases are private clean-room research artifacts and are intentionally excluded from Git.

## Byte-complete linear disassembly

`tools/private_disassemble.py` decoded the complete raw `.text` section with Capstone in 32-bit x86 mode.

| Property | Result |
|---|---:|
| Raw `.text` bytes | 2,574,336 |
| Covered bytes | 2,574,336 (100%) |
| Decoded instructions | 856,087 |
| Skip-data records | 80 |
| Private listing size | 56,684,043 bytes |
| Private listing SHA-256 | `c5eb7ab8412ad7bd34547646731db4cac16480a0eb9d945c9f783b2afb2af0d9` |

This guarantees byte coverage but does not distinguish embedded data from reachable code.

## Ghidra analysis database

Ghidra 12.1.3 completed PE loading, recursive disassembly, symbol/import/export processing, Microsoft demangling, RTTI analysis, Windows x86 exception analysis, stack analysis, references, function-start discovery, and data-type application.

| Property | Result |
|---|---:|
| Ghidra instructions | 750,814 |
| Ghidra instruction bytes | 2,449,699 |
| Function candidates | 12,948 |
| Private instruction listing SHA-256 | `75da116fad2c09074374181f4eead4d2d3bf8ea97d916d7d4f9aa4d1ec8b1ba7` |
| Private function table SHA-256 | `c1c6c80aeace7d87bd000ea103c6496cfef7a6c272911abfdfb9299724e96303` |

The packaged Ghidra decompiler has no Linux ARM64 native component, so decompilation was not part of this run. This does not affect the requested complete disassembly: the Ghidra database supplies structured control-flow analysis and the Capstone listing supplies 100% byte coverage. Any future decompiler output remains private and cannot be copied into implementation source.

The apparent TLS callback address reported during PE loading is the callback-array address; its entries are all null. No TLS initialization function is present.

## Clean-room boundary

Private artifacts contain original instructions and bytes. They must never be committed, attached to issues, placed in releases, or shared with implementers. Public work may use only reviewed behavior/interface specifications, aggregate measurements, and independently authored code.

