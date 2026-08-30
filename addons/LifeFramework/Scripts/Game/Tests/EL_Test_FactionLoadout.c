// red-proof: change an expected value below (e.g. CIVILIAN cash 500 -> 501, or
// remove one faction's EL_FactionLoadout from GameMode_Roleplay_Debug.et), then
// run `tools\cli test --tier all`; the config assertions go red.
// Observed red: expected 501 cash for CIVILIAN (config gives 500), then reverted.

// tier: WORLD
class EL_Test_FactionLoadoutConfig : EL_Test
{
	protected static const ResourceName MONEY_PREFAB = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName M9_PREFAB = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spawn/faction-loadouts";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		// The live spawn logic is the one configured on the DebugWorld game mode
		// (GameMode_Roleplay_Debug.et). If the prefab failed to deserialize, the
		// spawn logic either is missing or carries no faction loadouts.
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		ctx.True(respawnSystem != null, "respawn system is present");
		if (ctx.FailureCount() > 0)
			return;

		EL_SpawnLogic spawnLogic = EL_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());
		ctx.True(spawnLogic != null, "respawn system carries EL_SpawnLogic");
		if (ctx.FailureCount() > 0)
			return;

		EL_FactionLoadout civilian = spawnLogic.GetFactionLoadout(EL_Faction.CIVILIAN);
		EL_FactionLoadout police = spawnLogic.GetFactionLoadout(EL_Faction.POLICE);
		ctx.True(civilian != null, "CIVILIAN spawn has a configured loadout");
		ctx.True(police != null, "POLICE spawn has a configured loadout");
		if (ctx.FailureCount() > 0)
			return;

		CheckLoadoutPrefabs(ctx, civilian, "civilian");
		CheckLoadoutPrefabs(ctx, police, "police");

		ctx.True(LoadoutStoresMoney(civilian, 500), "CIVILIAN starts with 500 cash");
		ctx.True(LoadoutStoresMoney(police, 200), "POLICE starts with 200 cash");
		ctx.True(HasDirectItem(civilian, APPLE_PREFAB), "CIVILIAN spawns apples for survival testing");
		ctx.True(HasDirectItem(police, M9_PREFAB), "POLICE spawns an M9 sidearm");
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts every prefab the loadout references (loadout slots + direct items +
	//! nested stored items) resolves.
	protected void CheckLoadoutPrefabs(EL_TestContext ctx, EL_FactionLoadout loadout, string label)
	{
		foreach (EL_DefaultLoadoutItem item : loadout.m_aItems)
			CheckItemPrefabs(ctx, item, label, 0);

		foreach (ResourceName prefab : loadout.m_aDirectItems)
			ctx.True(Resource.Load(prefab).IsValid(), label + " direct item resolves: " + prefab);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckItemPrefabs(EL_TestContext ctx, EL_DefaultLoadoutItem item, string label, int depth)
	{
		if (!item || depth > 4)
			return;

		ctx.True(Resource.Load(item.m_rPrefab).IsValid(), label + " loadout item resolves: " + item.m_rPrefab);

		if (item.m_aStoredItems)
		{
			foreach (EL_DefaultLoadoutItem stored : item.m_aStoredItems)
				CheckItemPrefabs(ctx, stored, label, depth + 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the loadout's clothing carries a MoneyStack whose quantity
	//! component default matches the expected starting cash.
	protected bool LoadoutStoresMoney(EL_FactionLoadout loadout, int expectedQuantity)
	{
		foreach (EL_DefaultLoadoutItem item : loadout.m_aItems)
		{
			if (!item.m_aStoredItems)
				continue;

			foreach (EL_DefaultLoadoutItem stored : item.m_aStoredItems)
			{
				if (stored.m_rPrefab != MONEY_PREFAB)
					continue;

				if (!stored.m_aComponentDefaults)
					continue;

				foreach (EL_DefaultLoadoutItemComponent componentDefault : stored.m_aComponentDefaults)
				{
					EL_DefaultLoadoutItemQuantityComponent quantity = EL_DefaultLoadoutItemQuantityComponent.Cast(componentDefault);
					if (quantity && quantity.m_iQuantity == expectedQuantity)
						return true;
				}
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDirectItem(EL_FactionLoadout loadout, ResourceName prefab)
	{
		foreach (ResourceName direct : loadout.m_aDirectItems)
		{
			if (direct == prefab)
				return true;
		}

		return false;
	}
}