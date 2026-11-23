// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_VaultDrillPickup` RED PROOF OK, hook on failed `- after the drill time the vault is open`, `- the haul is paid (expected 1, got 0)`, `- the drill lies at the vault`. Green on main as of 723c22a1: the server path is not the #4156 break (see the PR).
//! The robbery kit's drill end to end on the server (#4192, #4156): the kit puts a drill in the
//! robber's inventory as a hand tool, held it drills the vault to the open state and pays, the drill
//! is left loose at the door, and the robber can pick that drill up again and hold it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VaultDrillPickup : LF_Test
{
	override string GetName()
	{
		return "crime/vault-drill-pickup";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab(LF_TestVaultDrillFixture.PACK_PREFAB, robber.GetOrigin());
		if (!pack || !inventory || !inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
		{
			ctx.Fail("the robber cannot wear a pack");
			LF_TestVaultDrillFixture.Release(robber);
			return;
		}

		string uid = "vault-drill-pickup";
		string vaultId = "vault-drill-pickup";
		LF_TestVaultDrillFixture.Vault(vaultId, robber, 10);

		ctx.True(LF_BankRobKit.Grant(robber) >= 1, "the robbery kit hands something over");
		IEntity drill = LF_HandTool.Carried(robber, LF_VaultDrill.TOOL_ID);
		ctx.True(drill != null, "the kit's drill is in the inventory as a hand tool");
		if (!drill)
		{
			LF_TestVaultDrillFixture.Release(robber, vaultId);
			return;
		}

		LF_TestVaultDrillFixture.Hold(robber, drill, true);
		ctx.True(LF_HandTool.HeldTool(robber, LF_VaultDrill.TOOL_ID) == drill, "the kit's drill is held");

		LF_VaultDrill.s_fNowOverride = 500;
		ctx.True(LF_Bank.Rob(vaultId, uid, robber, 1), "held at the vault, the drilling starts");
		ctx.True(LF_VaultDrill.IsDrillingBy(vaultId, robber), "the robber is the one drilling");

		if (!RedProof())
			LF_VaultDrill.s_fNowOverride = 511;

		LF_VaultDrill.Tick();
		ctx.True(LF_Breach.IsOpen(vaultId), "after the drill time the vault is open");
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount(vaultId), "the haul is paid");

		// The drill now lies loose at the door; the robber takes it back.
		IEntity left = FindLeftDrill(robber.GetOrigin());
		ctx.True(left != null, "the drill lies at the vault");
		if (left)
		{
			ctx.True(LF_HandTool.Carried(robber, LF_VaultDrill.TOOL_ID) == null, "the left drill is not in the inventory");
			ctx.True(inventory.TryInsertItem(left), "the left drill can be picked up");
			IEntity again = LF_HandTool.Carried(robber, LF_VaultDrill.TOOL_ID);
			ctx.True(again == left, "the picked-up drill is a carried hand tool");
			if (again)
			{
				LF_TestVaultDrillFixture.Hold(robber, again, true);
				ctx.True(LF_HandTool.Holds(robber, LF_VaultDrill.TOOL_ID), "the picked-up drill can be held again");
			}
		}

		LF_VaultDrill.s_fNowOverride = -1;
		LF_TestVaultDrillFixture.Release(robber, vaultId);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity m_LeftDrill;

	//------------------------------------------------------------------------------------------------
	protected IEntity FindLeftDrill(vector around)
	{
		m_LeftDrill = null;
		GetGame().GetWorld().QueryEntitiesBySphere(around, 4, VisitDrill);
		return m_LeftDrill;
	}

	//------------------------------------------------------------------------------------------------
	protected bool VisitDrill(IEntity entity)
	{
		if (entity.GetParent() || !LF_Utils.SamePrefab(LF_Utils.GetPrefabName(entity), LF_Bank.DRILL_PREFAB))
			return true;

		m_LeftDrill = entity;
		return false;
	}
}
