-- Eden's play time for Switch games, read from Eden's own files.
-- play_time/playtime.bin: records of { u64 program id, u64 seconds }, little-endian.
-- game_list/<ID>.pv.txt: the update and add-ons Eden found for each game it scanned;
-- <ID>.appname.txt: the title, which Eden caches only for games installed to its NAND.
local RECORD = 16
local CLAIMS = { source = "rom", systems = { "switch" } }

-- Program id (16 upper-case hex digits) to seconds; nil while Eden is rewriting the file.
local function read_database()
  for _, root in ipairs(opensu.paths.read.play_time or {}) do
    local file = root .. "/playtime.bin"
    local before = opensu.fs.stat(file)
    if before then
      local bytes, err = opensu.fs.read(file)
      if not bytes then
        opensu.log.warn("cannot read " .. file .. ": " .. err)
        return nil
      end
      -- Eden truncates and rewrites in place, so a read that overlapped a write is dropped.
      local after = opensu.fs.stat(file)
      if not after or after.size ~= before.size or after.mtime ~= before.mtime
          or #bytes ~= before.size or #bytes % RECORD ~= 0 then
        return nil
      end
      local database = {}
      for at = 1, #bytes, RECORD do
        local id, seconds = string.unpack("<I8I8", bytes, at)
        if id ~= 0 then
          database[string.format("%016X", id)] = seconds
        end
      end
      return database
    end
  end
  return {}
end

-- The program id a dump names in brackets, [0100000000010000]; the last one in the path is the
-- file's own. An update's id ends in 800 where its game's ends in 000.
local function tagged_id(path)
  local found = nil
  for hex in path:gmatch("%[(%x+)%]") do
    if #hex == 16 then
      found = hex:upper()
    end
  end
  if found and found:sub(-3) == "800" then
    found = found:sub(1, -4) .. "000"
  end
  return found
end

local function read_text(file)
  local text = opensu.fs.read(file)
  if text and text ~= "" then
    return text
  end
  return nil
end

-- Every game Eden's game list cache holds, by program id.
local function list()
  local entries = {}
  local seen = {}
  for _, dir in ipairs(opensu.paths.read.game_list or {}) do
    local names = opensu.fs.list(dir) or {}
    local present = {}
    for _, name in ipairs(names) do
      present[name] = true
    end
    for _, name in ipairs(names) do
      local hex = name:match("^(%x+)%.")
      local id = hex and #hex == 16 and hex:upper()
      if id and not seen[id] then
        seen[id] = true
        local entry = { id = id, metadata = {} }
        if present[hex .. ".appname.txt"] then
          entry.title = read_text(dir .. "/" .. hex .. ".appname.txt")
        end
        if present[hex .. ".pv.txt"] then
          entry.metadata.versions = read_text(dir .. "/" .. hex .. ".pv.txt")
        end
        entries[#entries + 1] = entry
      end
    end
  end
  return entries
end

opensu.register_game_list{ claims = CLAIMS, list = list }

opensu.register_playtime_source{
  claims = CLAIMS,
  read = function(games)
    local database = read_database()
    if not database then
      return nil
    end
    local reports = {}
    local count = 0
    for _, game in ipairs(games) do
      local id = tagged_id(game.path) or (game.listed and game.listed.id)
      local seconds = id and database[id]
      if seconds and seconds > 0 then
        reports[game.id] = { seconds = seconds }
        count = count + 1
      end
    end
    opensu.log.info(string.format("play time for %d of %d Switch games", count, #games))
    return reports
  end,
}
