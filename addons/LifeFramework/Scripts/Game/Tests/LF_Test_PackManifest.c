// red-proof: expecting 1 overlay for an out-of-range pack failed `- out of range: no overlay is handed out (expected 1, got 0)` and `SUMMARY tier=only passed=0 failed=1` (observed 2026-11-14); restored, green.
// tier: LOGIC
//! Pack manifest (#963): a pack whose core range holds the running core loads its overlays; one
//! whose range excludes it is refused and its overlays are not handed out.
class LF_Test_PackManifest : LF_Test
{
	override string GetName()
	{
		return "packs/manifest";
	}

	protected static LF_PackManifest Make(string min, string below)
	{
		LF_PackManifest manifest = new LF_PackManifest();
		manifest.m_sPackId = "LF_Test_Pack";
		manifest.m_sCoreMin = min;
		manifest.m_sCoreBelow = below;
		return manifest;
	}

	override void Run(LF_TestContext ctx)
	{
		// The range: min inclusive, below exclusive, empty bounds open.
		ctx.True(LF_PackManifest.InRange("0.1.0", "0.1.0", "0.2.0"), "the min is inclusive");
		ctx.True(LF_PackManifest.InRange("0.1.9", "0.1.0", "0.2.0"), "a patch inside the range");
		ctx.False(LF_PackManifest.InRange("0.2.0", "0.1.0", "0.2.0"), "the below bound is exclusive");
		ctx.False(LF_PackManifest.InRange("0.0.9", "0.1.0", "0.2.0"), "under the min is out");
		ctx.True(LF_PackManifest.InRange("1.10.0", "1.9.0", ""), "no upper bound, and 10 is above 9");
		ctx.True(LF_PackManifest.InRange("0.1.0", "", "0.2.0"), "no lower bound");
		ctx.False(LF_PackManifest.InRange("0.1.0", "0.x", ""), "a bound that is not x.y.z refuses");
		ctx.False(LF_PackManifest.InRange("dev", "", ""), "a core that is not x.y.z is out");

		// The source: in range loads, out of range is refused with no overlays.
		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert("{7A2C94E10D5B38F6}Configs/Tests/LF_Test_ShopPricesOverlay2.conf");

		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		array<ResourceName> overlays = {};

		source.SetManifest(Make("0.1.0", "0.2.0"), "0.1.5");
		source.GetOverlays("{000000000000000A}Configs/Core.conf", overlays);
		ctx.Equal(1, overlays.Count(), "in range: the pack's overlay is handed out");

		overlays.Clear();
		LF_PackManifest tooNew = Make("0.1.0", "0.2.0");
		source.SetManifest(tooNew, "0.2.0");
		source.GetOverlays("{000000000000000A}Configs/Core.conf", overlays);
		ctx.Equal(0, overlays.Count(), "out of range: no overlay is handed out");
		ctx.False(source.IsPackAccepted(header), "out of range: the pack is not accepted");
		ctx.EqualStr("pack 'LF_Test_Pack' refused: it needs core >= 0.1.0, < 0.2.0, this core is 0.2.0; its config overlays are not loaded", tooNew.RefusalLine("0.2.0"), "the refusal line names the pack and both versions");

		// No manifest: nothing to check, the mission loads as before.
		source.SetManifest(null);
		source.GetOverlays("{000000000000000A}Configs/Core.conf", overlays);
		ctx.Equal(1, overlays.Count(), "no manifest: loads as before");

		// The running core accepts the range a pack would ship today.
		ctx.True(Make("0.1.0", "0.2.0").Supports(LF_PackManifest.CORE_VERSION), "the current core is in the skeleton's range");
	}
}
