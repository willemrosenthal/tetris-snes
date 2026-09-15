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

# Convenience: build then launch in MesenCE
run: all
	open -a Mesen $(ROMNAME).sfc
