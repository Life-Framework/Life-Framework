// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_MarketPackOverlay` printed `RED PROOF OK`; with the hook skipping the pack source it failed `patch: the pack's drop replaces the core one (expected 2, got 3)`, `remove: the pack takes the copper curve out` and `add: the pack's new good has a curve`.
//! #3018: the market config reads pack overlays like LF_PropertyConfig. The goods are a list keyed by the
//! good's GUID: a pack row for a good the core has patches it, a new good adds, `remove` takes one out, and a
//! patch that would leave a row unusable is refused. The overlay root's `m_bEnabled` switches the market on.
//! Without a pack the core file answers alone. The test overlay (Configs/Tests/LF_Test_MarketOverlay.conf)
//! is reached through a mission header built in code.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MarketPackOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{7C3B51E8A92D4F10}Configs/Tests/LF_Test_MarketOverlay.conf";
	static const ResourceName IRON = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	static const ResourceName COPPER = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName GOLD = "{ACC9A10F0F20F5C4}Prefabs/Resources/Mining/GoldBar.et";
	static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";

	override string GetName()
	{
		return "market/pack-overlay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_MarketConfig.Reload();

		// No pack: the core file alone, market off, three goods.
		LF_MarketConfig core = LF_MarketConfig.Get();
		ctx.True(core != null, "the core market config loads");
		if (!core)
			return;

		ctx.False(core.m_bEnabled, "without a pack the market is off");
		ctx.Equal(3, core.m_aGoods.Count(), "without a pack the core lists three goods");
		ctx.True(core.FindGood(COPPER) != null, "without a pack the core copper curve stands");
		ctx.True(core.FindGood(CABBAGE) == null, "without a pack there is no cabbage curve");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_MarketConfig.Reload();
		LF_MarketConfig merged = LF_MarketConfig.Get();
		ctx.True(merged != null, "the merged market config loads");
		if (!merged)
		{
			LF_ConfigService.Reset();
			return;
		}

		ctx.True(merged.m_bEnabled, "root: the pack's m_bEnabled switches the market on");

		LF_MarketGoodDef iron = merged.FindGood(IRON);
		ctx.True(iron != null, "patch: iron still has a curve");
		if (iron)
		{
			ctx.Equal(40, iron.m_iCeiling, "patch: the pack's ceiling replaces the core one");
			ctx.Equal(2, iron.m_iDropPerUnitPct, "patch: the pack's drop replaces the core one");
			ctx.Equal(30, iron.m_iBase, "patch: a field the pack leaves alone stays core");
			ctx.Equal(15, iron.m_iFloor, "patch: the floor stays core");
		}

		ctx.True(merged.FindGood(COPPER) == null, "remove: the pack takes the copper curve out");

		LF_MarketGoodDef gold = merged.FindGood(GOLD);
		ctx.True(gold != null, "refused patch: gold keeps its curve");
		if (gold)
			ctx.Equal(120, gold.m_iFloor, "refused patch: a floor above the ceiling leaves the shipped row");

		LF_MarketGoodDef cabbage = merged.FindGood(CABBAGE);
		ctx.True(cabbage != null, "add: the pack's new good has a curve");
		if (cabbage)
		{
			ctx.Equal(4, cabbage.m_iBase, "add: the new good has its base");
			ctx.Equal(12, cabbage.m_iRecoverPctPerHour, "add: the new good has its recovery");
		}

		ctx.Equal(3, merged.m_aGoods.Count(), "iron patched, copper removed, cabbage added, gold kept");

		LF_ConfigService.Reset();
		LF_MarketConfig.Reload();
	}
}
