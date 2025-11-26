-- Townsfolk Relocation Mod
-- Moves Wirt and Adria closer to the other townsfolk in Tristram

local events = require("devilutionx.events")
local towners = require("devilutionx.towners")
local log = require("devilutionx.log")

-- New positions for Wirt and Adria (closer to the main town center)
local NEW_WIRT_X = 40
local NEW_WIRT_Y = 75
local NEW_ADRIA_X = 75
local NEW_ADRIA_Y = 70

log.info("Townsfolk Relocation mod loaded")

events.GameStart.add(function()
  log.info("Townsfolk Relocation: GameStart event fired")

  -- Move Wirt (originally at 11, 53)
  local wirt_result = towners.wirt.setPosition(NEW_WIRT_X, NEW_WIRT_Y)
  log.info("Wirt move result: " .. wirt_result)

  -- Move Adria (originally at 80, 20)
  local adria_result = towners.adria.setPosition(NEW_ADRIA_X, NEW_ADRIA_Y)
  log.info("Adria move result: " .. adria_result)
end)
