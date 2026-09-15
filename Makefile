ifeq ($(strip $(PVSNESLIB_HOME)),)
$(error "Please create an environment variable PVSNESLIB_HOME (see PVSnesLib install guide)")
endif

export ROMNAME := tetris
export ROMTITLE := POMPOM TETRIS

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

.PHONY: all cleanLogs bitmaps

# Block art is pre-converted and committed (gfx/block_master.pic). To regenerate
# from the source PNG, run `make gfx`.
gfx:
	cd gfx && $(GFXCONV) -i block_master.png -s 8 -u 16 -p -m -t png

all: bitmaps buildWithSummary
bitmaps: gfx/block_master.pic
buildActual: $(OBJS) $(ROMNAME).sfc

clean: cleanBuildRes cleanRom cleanGfx cleanLogs

# Convenience: build then launch in MesenCE.
# NOTE: use the binary directly with the ROM path — `open -a Mesen <rom>` only
# activates the app WITHOUT loading the ROM (black screen).
run: all
	/Applications/Mesen.app/Contents/MacOS/Mesen $(CURDIR)/$(ROMNAME).sfc &
