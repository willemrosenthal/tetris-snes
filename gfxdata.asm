.include "hdr.asm"

.section ".rodata_gfx" superfree

; 16x16 block master tile: 4 8x8 tiles (TL,TR,BL,BR), 4bpp/16-color.
; Recolored at runtime via 5 palettes. Source: tetris-blocks.png (green block).
blocktiles:
.incbin "gfx/block_master.pic"
blocktiles_end:

; Play-area purple grid: 9 tiles (9-slice-ish), 4 colors. From play-area.png.
gridtiles:
.incbin "gfx/grid.pic"
gridtiles_end:

; Play-area frame nine-patch: 10 tiles (TL,T,TR,L,C,R,BL,B,BR,blank), 2bpp.
; Derived from play-area-frame.png colors (field/blue/cyan).
frametiles:
.incbin "gfx/frame9.pic"
frametiles_end:

.ends
