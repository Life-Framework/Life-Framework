//------------------------------------------------------------------------------------------------
//! Playtest kit stand beside the bank (#605): one action hands over everything the bank robbery
//! checks, so nobody hunts for the debug menu. The list is built from the prefab constants that
//! LF_Bank.PickTool and LF_Breach.HasCharge read, so a tool added there is one line here (and the
//! test fails until it is). Gated like LF_SpawnLogic.m_bDebugMoneyTopUp: on in the playtest game
//! mode, off in the flagship.
//------------------------------------------------------------------------------------------------
class LF_BankRobKit
{
	//------------------------------------------------------------------------------------------------
	//! Every item the vault tool check can accept: the drill, the keycard, the fence charge.
	static array<ResourceName> Items()
	{
		array<ResourceName> items = {};
		items.Insert(LF_Bank.DRILL_PREFAB);
		items.Insert(LF_Bank.KEYCARD_PREFAB);
		items.Insert(LF_Breach.FENCE_CHARGE_PREFAB);
		return items;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the playtest flag is on. With no spawn logic (a client before spawn) the stand
	//! shows and the server decides.
	static bool Enabled()
	{
		if (!Replication.IsServer())
			return true;

		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		return spawnLogic && spawnLogic.IsDebugMoneyTopUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Tops the character up to one of each kit item. Server only. Returns how many were added;
	//! an item already carried is not doubled, so a second press adds nothing.
	static int Grant(IEntity character)
	{
		if (!character || !Replication.IsServer())
			return 0;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory)
		{
			LF_Debug.Error("BankKit", "grant refused: character has no inventory");
			return 0;
		}

		int added;
		foreach (ResourceName prefab : Items())
		{
			bool has;
			if (LF_Utils.SamePrefab(prefab, LF_Breach.FENCE_CHARGE_PREFAB))
				has = LF_Breach.HasCharge(character);
			else
				has = LF_InventoryUtils.GetAmount(character, prefab) >= 1;

			if (has)
				continue;

			if (LF_InventoryUtils.AddItem(inventory, prefab))
				added++;
			else if (LF_Utils.SpawnEntityPrefab(prefab, character.GetOrigin()))
				LF_Debug.Warn("BankKit", string.Format("no room for %1: left at the player's feet, pick it up", prefab));
			else
				LF_Debug.Error("BankKit", string.Format("grant failed for %1", prefab));
		}

		LF_Debug.Info("BankKit", string.Format("granted %1 of %2 kit items", added, Items().Count()));
		return added;
	}
}

//------------------------------------------------------------------------------------------------
class LF_BankRobKitAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_BankRobKit.Enabled();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!LF_BankRobKit.Enabled())
		{
			LF_Debug.Warn("BankKit", "refused: the playtest flag is off");
			return;
		}

		LF_BankRobKit.Grant(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-BankKit_Take";
		return true;
	}
}
