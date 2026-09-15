--[[ Mesen test harness for the Pompom Tetris SNES port.

  Runs under: Mesen --testrunner <rom> harness.lua
  Reads a command script from /tmp/mesen_harness/commands.txt, drives the
  emulated controller, saves screenshots, and calls emu.stop() when done.

  Command file format (one per line, '#' = comment):
    press <btns> <frames>   hold comma-separated buttons for N frames, then release
                            btns: a b x y l r up down left right select start
    wait  <frames>          idle for N frames
    shot  <name>            save a screenshot to /tmp/mesen_harness/<name>.png
    stop  [code]            stop the emulator (also implicit at end of file)

  TIMING: use generous holds (>= 8-10 frames) and put a `wait 10` before each
  `shot`. Short holds can miss the game's per-frame pad read (setInput lands via
  the inputPolled event, which can be a frame off from the game's read), and a
  shot taken immediately after a press may capture before the effect renders.
]]

local OUT = "/tmp/mesen_harness/"

local cmds = {}
for line in io.lines(OUT .. "commands.txt") do
  line = line:gsub("%s+$", "")
  if line ~= "" and line:sub(1, 1) ~= "#" then
    cmds[#cmds + 1] = line
  end
end

local BUTTONS = { "a", "b", "x", "y", "l", "r",
                  "up", "down", "left", "right", "select", "start" }
local held = {}
local function releaseAll()
  for _, b in ipairs(BUTTONS) do held[b] = false end
end
releaseAll()

-- Feed the current button state to the emulated pad every time it's polled.
emu.addEventCallback(function() emu.setInput(held, 0) end, emu.eventType.inputPolled)

local function tokens(s)
  local t = {}
  for w in s:gmatch("%S+") do t[#t + 1] = w end
  return t
end

local step = 1
local holdLeft = 0

-- Drive the command sequence once per frame.
emu.addEventCallback(function()
  if holdLeft > 0 then
    holdLeft = holdLeft - 1
    if holdLeft == 0 then releaseAll() end -- 1-frame release gap between commands
    return
  end

  while step <= #cmds do
    local t = tokens(cmds[step]); step = step + 1
    local op = t[1]
    if op == "press" then
      releaseAll()
      for b in t[2]:gmatch("[^,]+") do held[b] = true end
      holdLeft = tonumber(t[3]) or 10
      return
    elseif op == "wait" then
      releaseAll()
      holdLeft = tonumber(t[2]) or 4
      return
    elseif op == "shot" then
      local png = emu.takeScreenshot()
      local f = io.open(OUT .. t[2] .. ".png", "wb")
      if f then f:write(png); f:close() end
      emu.log("SHOT " .. t[2])
    elseif op == "stop" then
      emu.stop(tonumber(t[2]) or 0)
      return
    end
  end
  emu.stop(0)
end, emu.eventType.endFrame)

emu.log("harness loaded: " .. #cmds .. " commands")
