.include "hdr.asm"

.section ".rodata_gfx" superfree

; 16x16 block master tile: 4 8x8 tiles (TL,TR,BL,BR), 4bpp/16-color.
; Recolored at runtime via 5 palettes. Source: tetris-blocks.png (green block).
blocktiles:
.incbin "gfx/block_master.pic"
blocktiles_end:

; Bomb OBJ sprite: 3 frames arranged for 16x16 hardware sprites (gfxoffset f*2).
bombtiles:
.incbin "gfx/bomb_obj.pic"
bombtiles_end:

bombpal:
.incbin "gfx/bomb.pal"
bombpal_end:

; Placement reticle: 16x16 dashed outline OBJ sprite (from ghost-blocks.png),
; laid out for a 16x16 hardware sprite (tiles 0,1,16,17).
reticletiles:
.incbin "gfx/reticle.pic"
reticletiles_end:

reticlepal:
.incbin "gfx/reticle.pal"
reticlepal_end:

; Pixel-accurate background SCENE (16-color, 4bpp): real checker (tetris-game-bg)
; + play-area grid (play-area.png, with top/left shadow) + 5px blue/white/cyan
; frame (play-area-frame.png). 23 reduced tiles + full 32x28 tilemap.
scenetiles:
.incbin "gfx/scene.pic"
scenetiles_end:

scenemap:
.incbin "gfx/scene.map"
scenemap_end:

scenepal:
.incbin "gfx/scene.pal"
scenepal_end:

; 2bpp HUD font (96 ASCII glyphs from space) for the console on BG3 (4-color).
font2tiles:
.incbin "gfx/font2.pic"
font2tiles_end:

font2pal:
.incbin "gfx/font2.pal"
font2pal_end:

.ends
