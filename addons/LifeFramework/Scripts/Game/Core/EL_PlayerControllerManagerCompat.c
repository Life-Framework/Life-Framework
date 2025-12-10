class EL_PlayerControllerManagerCompat
{
    static ref EL_PlayerControllerManagerCompat s_Instance;

    static EL_PlayerControllerManagerCompat GetInstance()
    {
        if (!s_Instance)
            s_Instance = new EL_PlayerControllerManagerCompat();
        return s_Instance;
    }

    // Fill the provided array with player ids. Uses PlayerManager compatibility layer.
    void GetAllPlayerIds(out array<int> outIds)
    {
        outIds.Clear();
        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return;

        // Try to use a GetPlayers method (used in POPLIFE). Falls back to iterating possible ids.
        // If PlayerManager exposes GetPlayers(array<int>&) it will fill outIds.
        // Otherwise attempt to gather ids via GetPlayerCount / GetPlayerIdFromControlledEntity patterns.
        bool filled = false;
        // Try dynamic call - if method exists this will work; otherwise it will be ignored at runtime.
        pm.GetPlayers(outIds);
        if (outIds && outIds.Count() > 0)
            filled = true;

        if (!filled)
        {
            // Best-effort fallback: attempt an index-based collection using GetPlayerCount and GetPlayerController
            int maxPlayers = 64;
            for (int i = 0; i < maxPlayers; i++)
            {
                // Use PlayerManager where possible to retrieve controllers by index
                PlayerController pc = null;
                // prefer using pm.GetPlayerController if available
                pc = pm.GetPlayerController(i);
                if (!pc)
                    continue;

                outIds.Insert(i);
            }
        }
    }

    // Provide a compatibility method to fetch a PlayerController by player id.
    // Some LifeFramework code expects SCR_PlayerControllerManagerComponent.GetPlayerController(id).
    PlayerController GetPlayerController(int playerId)
    {
        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm)
            return null;

        // Try direct PlayerManager call if available
        PlayerController pc = pm.GetPlayerController(playerId);
        if (pc)
            return pc;

        // Fallback: try to query a players array and fetch by id
        array<int> ids = {};
        pm.GetPlayers(ids);
        foreach (int id : ids)
        {
            if (id == playerId)
            {
                PlayerController pcc = pm.GetPlayerController(id);
                if (pcc)
                    return pcc;
            }
        }

        return null;
    }
};
