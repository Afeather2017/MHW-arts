-- FSM/LMT live probe v3: weapon controller (slot0) + body controller (slot1) + HP tracking.
local LOG = 'E:/MHW-arts/nfh-analysis/probe_log.txt'
local last_key = nil
local last_body = nil
local last_hp = nil

local function read_hp(p)
    local ok, hp = pcall(function()
        local comp = GetAddress(p, { 0x7630 })
        if not comp or comp == 0 then return nil end
        return GetAddressData(comp + 0x64, 'float')
    end)
    if ok then return hp end
    return nil
end

function on_time()
    local ok, p = pcall(function() return GetAddress(0x1450139A0, { 0x50 }) end)
    if not ok or not p or p == 0 then return end

    local hp = read_hp(p)
    if hp and last_hp and math.abs(hp - last_hp) > 0.01 then
        local f = io.open(LOG, 'a')
        if f then
            f:write(string.format("HP %.1f -> %.1f (delta %.1f)\n", last_hp, hp, hp - last_hp))
            f:close()
        end
    end
    if hp then last_hp = hp end

    -- slot1 = body controller (player+0x6878), current action at +0xAC/+0xB0
    local okB, bSet, bId = pcall(function()
        return GetAddressData(p + 0x6924, 'int'), GetAddressData(p + 0x6928, 'int')
    end)
    if okB then
        local bk = tostring(bSet) .. ':' .. tostring(bId)
        if bk ~= last_body then
            last_body = bk
            local f = io.open(LOG, 'a')
            if f then
                f:write(string.format("body=%s hp=%s\n", bk, tostring(hp)))
                f:close()
            end
        end
    end

    local ok2, fsmT, fsmI, anim, lmt, frame, derS, derI = pcall(function()
        local anim = GetAddress(p, { 0x468 })
        return GetAddressData(p + 0x6274, 'int'),
            GetAddressData(p + 0x6278, 'int'),
            anim,
            (anim and anim ~= 0) and GetAddressData(anim + 0xE9C4, 'int') or -1,
            (anim and anim ~= 0) and GetAddressData(anim + 0x10C, 'float') or -1,
            GetAddressData(p + 0x6284, 'int'),
            GetAddressData(p + 0x6288, 'int')
    end)
    if not ok2 then return end
    local key = string.format('%s:%s:%s', tostring(fsmT), tostring(fsmI), tostring(lmt))
    if key ~= last_key then
        last_key = key
        local entry = (type(lmt) == 'number' and lmt >= 0) and (lmt % 4096) or lmt
        local f = io.open(LOG, 'a')
        if f then
            f:write(string.format("fsm=%s:%s lmt=%s entry=%s frame=%s derive=%s:%s hp=%s body=%s\n",
                tostring(fsmT), tostring(fsmI), tostring(lmt),
                tostring(entry), tostring(frame), tostring(derS), tostring(derI),
                tostring(hp), tostring(last_body)))
            f:close()
        end
    end
end
