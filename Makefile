ifeq ($(strip $(PVSNESLIB_HOME)),)
$(error "Please create an environment variable PVSNESLIB_HOME (see PVSnesLib install guide)")
endif

export ROMNAME := tetris
export ROMTITLE := POMPOM TETRIS

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

.PHONY: all cleanLogs

all: buildWithSummary
buildActual: $(OBJS) $(ROMNAME).sfc

clean: cleanBuildRes cleanRom cleanGfx cleanLogs

# Convenience: build then launch in MesenCE.
# NOTE: use the binary directly with the ROM path — `open -a Mesen <rom>` only
# activates the app WITHOUT loading the ROM (black screen).
run: all
	/Applications/Mesen.app/Contents/MacOS/Mesen $(CURDIR)/$(ROMNAME).sfc &
