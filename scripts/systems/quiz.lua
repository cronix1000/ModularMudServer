function OnQuizCorrect(npcId, playerId)
    print("Correct answer! Giving reward...")
    
    -- We can call C++ functions exposed to Lua
    World.GiveExperience(playerId, 100)
    World.MessageLog("The NPC smiles: 'Correct! Take this gold.'")
    World.SpawnItemAtEntity("gold_coin", npcId)
end

function OnQuizWrong(npcId, playerId)
    World.MessageLog("The NPC frowns: 'Study harder, traveler.'")
end