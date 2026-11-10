// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_SearchCooldown` with MaySearch returning true failed `- inside the cooldown: refused`, `- a repeat a second later is refused`, `- still refused just inside the cooldown`. Restored; green.
// tier: LOGIC
//! Police search cooldown (#472): the same target cannot be searched again inside the configured
//! window, so repeating a search cannot beat the find chance. Another target is unaffected.
class LF_Test_SearchCooldown : LF_Test
{
	override string GetName()
	{
		return "chains/search-cooldown";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_ChainSearch.MaySearch(-1, 1000, 120000), "never searched: allowed");
		ctx.False(LF_ChainSearch.MaySearch(1000, 60000, 120000), "inside the cooldown: refused");
		ctx.True(LF_ChainSearch.MaySearch(1000, 121000, 120000), "at the cooldown: allowed");
		ctx.True(LF_ChainSearch.MaySearch(1000, 1001, 0), "cooldown 0 disables it");

		int cooldownMs = LF_PoliceConfig.SearchCooldownSeconds() * 1000;
		ctx.Equal(120000, cooldownMs, "the configured cooldown is 120 s");

		LF_ChainSearch.ResetSearchCooldowns();
		ctx.True(LF_ChainSearch.TryBeginSearch("p:a", 5000), "first search of a target is allowed");
		ctx.False(LF_ChainSearch.TryBeginSearch("p:a", 6000), "a repeat a second later is refused");
		ctx.True(LF_ChainSearch.TryBeginSearch("p:b", 6000), "another target is not affected");
		ctx.False(LF_ChainSearch.TryBeginSearch("p:a", 5000 + cooldownMs - 1), "still refused just inside the cooldown");
		ctx.True(LF_ChainSearch.TryBeginSearch("p:a", 5000 + cooldownMs + 1000), "allowed once the cooldown has passed");
		LF_ChainSearch.ResetSearchCooldowns();
	}
}
