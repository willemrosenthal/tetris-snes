.include "hdr.asm"

.section ".rodata_gfx" superfree

; 16x16 block master tile: 4 8x8 tiles (TL,TR,BL,BR), 4bpp/16-color.
; Recolored at runtime via 5 palettes. Source: tetris-blocks.png (green block).
blocktiles:
.incbin "gfx/block_master.pic"
blocktiles_end:

.ends
