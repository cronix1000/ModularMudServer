-- scripts/items/example_ring.lua
-- Example item-level hook: Dragon Ring
-- Demonstrates equip/unequip/drop hooks with cancel flow.

function on_execute(ctx)
    -- ctx fields available:
    --   userID, itemID, slot, itemTemplateID, roomID, x, y

    -- The hook distinguishes equip/unequip/drop by which scripts_path key
    -- points at this file. Author conventions:
    --   1) Read the calling key from a global set in the script loader
    --      (not yet wired) OR
    --   2) Use the same function for all three and inspect ctx.slot (nil for drop).
    --
    -- This example uses approach 2. We assume the file is referenced by all three keys.

    if ctx.slot == "" or ctx.slot == nil then
        -- on_drop path
        send_to_char(ctx.userID, "The ring pulses once as it hits the ground, then goes still.")
        return {
            success = true,
            cancelDrop = false
        }
    else
        -- on_equip / on_unequip path
        if string.find(ctx.slot, "Finger") == 1 then
            send_to_char(ctx.userID, "The ring warms on your finger.")
        end
        return {
            success = true,
            cancelEquip = false,
            cancelUnequip = false,
            message = "The ring settles into place.",
            roomMessage = "A faint hum emanates from the ring."
        }
    end
end
