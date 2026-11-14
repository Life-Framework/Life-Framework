// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_BankRobKit` with the second-grant expectation set to 1 failed `- a second grant adds nothing (expected 1, got 0)`. Restored; green.
// tier: WORLD
//! The bank robbery kit (#605): after one grant the character holds every item the vault tool
//! check accepts (drill, keycard, fence charge), a vault that wants any one of them passes the
//! offer check, a second grant adds nothing, and the placed stand sits in the playtest town.
class LF_Test_BankRobKit : LF_Test
{
	override string GetName()
	{
		return "bank/rob-kit";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> items = LF_BankRobKit.Items();
		ctx.True(items.Find(LF_Bank.DRILL_PREFAB) != -1, "the kit holds the drill");
		ctx.True(items.Find(LF_Bank.KEYCARD_PREFAB) != -1, "the kit holds the keycard");
		ctx.True(items.Find(LF_Breach.FENCE_CHARGE_PREFAB) != -1, "the kit holds the fence charge");

		ctx.NotNull(GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BANK_ROB_KIT), "the stand is placed in the playtest town");

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		// The fixture's jacket is too small for a drill; a field pack carries the kit.
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab("{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", robber.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the robber wears a pack");

		LF_Bank.Configure("kit-drill", 1, 100, 0, 0, 0);
		LF_Bank.ConfigureTools("kit-drill", "drill", 0);
		LF_Bank.Configure("kit-card", 1, 100, 0, 0, 0);
		LF_Bank.ConfigureTools("kit-card", "keycard", 0);
		LF_Bank.Configure("kit-charge", 1, 100, 0, 0, 0);
		LF_Bank.ConfigureTools("kit-charge", "charge", 0);
		ctx.False(LF_Bank.HasTool("kit-drill", robber), "before the kit a bare character cannot work a drill vault");

		ctx.Equal(items.Count(), LF_BankRobKit.Grant(robber), "the first grant adds every item");
		ctx.True(LF_Bank.HasTool("kit-drill", robber), "the kit opens a vault that wants a drill");
		ctx.True(LF_Bank.HasTool("kit-card", robber), "the kit opens a vault that wants a keycard");
		ctx.True(LF_Bank.HasTool("kit-charge", robber), "the kit opens a vault that wants a charge");
		ctx.Equal(0, LF_BankRobKit.Grant(robber), "a second grant adds nothing");

		LF_TestCharacterFixture.Delete(robber);
	}
}
