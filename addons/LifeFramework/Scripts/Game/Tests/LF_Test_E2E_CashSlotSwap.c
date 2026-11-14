// red-proof: e2e/cash-slot-swap, observed 2026-11-14 with `tools\cli e2e --cash-swap`: skipping the legacy prefabs in LF_MoneyUtils failed boot 2 with `- e2e cash swap: the cash saved before the swap is still counted (expected 237, got 0)` and `- e2e cash swap: both count (expected 187, got 50)`. Restored; both boots green.

//------------------------------------------------------------------------------------------------
//! A pack that lists the cash slot overlay (Configs/Tests/LF_E2E_CashSlotSwap.conf). The reload
//! boot installs it as the pack source before the slot resolves.
class LF_E2E_CashSwapPackSource : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{266F551C39D78C7D}Configs/Tests/LF_E2E_CashSlotSwap.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_PrefabSlotsConfig.CONFIG_PATH)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "e2e-cash-slot-swap";
	}
}

//------------------------------------------------------------------------------------------------
//! The restart proof of #1130, two boots (`tools\cli wt e2e <slug> --cash-swap`):
//! 1. Save: a carried inventory with 237 cash under the core MoneyStack is captured and saved.
//! 2. Restart with a pack that swaps the cash slot to the iron nugget: the saved cash loads,
//!    is applied to a character, and still counts; spending and earning behave.
//! Runs only under LF_E2E_CASH_SWAP; a normal suite run passes it untouched.
// tier: PERSISTENCE
class LF_Test_E2E_CashSlotSwap : LF_Test
{
	static const string OWNER = "e2e-cash-swap-character";
	static const ResourceName CORE_CASH = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	static const ResourceName PACK_CASH = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	static const string MARKER_SEEDED = "e2e cash swap seeded";
	static const string MARKER_COUNTED = "e2e cash swap counted";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/cash-slot-swap";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_CASH_SWAP
#ifdef LF_E2E_LOAD_BOOT
		VerifySwapped(ctx);
#else
		Seed(ctx);
#endif
#else
		ctx.Pass("runs only under tools\\cli wt e2e <slug> --cash-swap");
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Boot 1: 237 cash under the core prefab, saved.
	protected void Seed(LF_TestContext ctx)
	{
		ctx.EqualStr(CORE_CASH, LF_MoneyUtils.CashPrefab(), "e2e cash swap: the first boot has the core cash");
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		ctx.Equal(237, LF_MoneyUtils.AddCash(character, 237), "e2e cash swap: 237 cash added");
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(OWNER, LF_StorageSnapshot.SLOT_CARRIED, character);
		LF_TestCharacterFixture.Delete(character);
		ctx.NotNull(record, "e2e cash swap: the character is captured");
		if (!record)
			return;

		LF_StorageSnapshot.Set(record);
		ctx.Equal(237, record.CountPrefab(CORE_CASH), "e2e cash swap: the save holds 237 core cash");
		PrintFormat("[LF_Tests] %1: %2 records under %3", MARKER_SEEDED, record.m_aItems.Count(), OWNER);
	}

	//------------------------------------------------------------------------------------------------
	//! Boot 2: the slot is swapped; the saved stack is still the core prefab and still counts.
	protected void VerifySwapped(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.SetPackSource(new LF_E2E_CashSwapPackSource());
		LF_PrefabSlots.ResetStatics();
		ctx.EqualStr(PACK_CASH, LF_MoneyUtils.CashPrefab(), "e2e cash swap: the pack swapped the cash slot");

		LF_StorageSnapshotRecord loaded = LF_StorageSnapshot.Get(OWNER, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(loaded, "e2e cash swap: the carried inventory loaded after the restart");
		if (!loaded)
			return;

		ctx.Equal(237, loaded.CountPrefab(CORE_CASH), "e2e cash swap: the save still holds the 237 core cash");
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_StorageSnapshot.Apply(character, loaded);
		int cash = LF_MoneyUtils.GetCash(character);
		ctx.Equal(237, cash, "e2e cash swap: the cash saved before the swap is still counted");
		ctx.Equal(100, LF_MoneyUtils.RemoveCash(character, 100), "e2e cash swap: 100 is spent from it");
		ctx.Equal(137, LF_MoneyUtils.GetCash(character), "e2e cash swap: 137 left");
		ctx.Equal(50, LF_MoneyUtils.AddCash(character, 50), "e2e cash swap: 50 earned");
		ctx.Equal(50, LF_InventoryUtils.GetAmount(character, PACK_CASH), "e2e cash swap: earned cash is the pack prefab");
		ctx.Equal(187, LF_MoneyUtils.GetCash(character), "e2e cash swap: both count");
		LF_TestCharacterFixture.Delete(character);
		LF_ConfigService.Reset();
		LF_PrefabSlots.ResetStatics();
		if (ctx.FailureCount() == 0)
			PrintFormat("[LF_Tests] %1: %2 cash counted after the swap", MARKER_COUNTED, cash);
	}
}
