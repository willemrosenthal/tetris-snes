# Pompom Tetris — SNES Port

SNES port of the TetrisMinigame from the Unity game **Pompom**, built with
[PVSnesLib](https://github.com/alekmaul/pvsneslib) (C) and tested in
[MesenCE](https://github.com/nesdev-org/MesenCE).

This lives outside the `Pompom Ports` repo because PVSnesLib cannot build from
paths containing spaces. The plan, mechanics writeup, and Unity reference source
are in the `Pompom Ports` repo (`.claude/PLAN.md`).

## Build & run

```sh
export PVSNESLIB_HOME="$HOME/snesdev/pvsneslib"
export PATH="/opt/homebrew/opt/gnu-sed/libexec/gnubin:$PATH"

make        # -> tetris.sfc
make run    # build + launch in MesenCE
make clean
```

## Status

- **Phase 1** (in progress): project skeleton — boots and draws the 9x12 grid
  + HUD labels via the text console.

See `.claude/PLAN.md` in the Pompom Ports repo for the full 7-phase plan.
