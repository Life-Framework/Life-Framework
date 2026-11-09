// red-proof: 2026-11-08, LF_OreYield.ItemsFor returned breaks (ignored the yield); `tools\cli test --no-build --only LF_Test_OreYield` failed "one piece of a four-yield row drops four items (expected 4, got 1)". Restored, green.

// tier: LOGIC
//! Ore rock arithmetic: pickaxe damage piles up into pieces, the rock is spent
//! after its row's piece count, and grows back whole.
class LF_Test_OreYield : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/ore-yield";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OreYieldState state = new LF_OreYieldState();

		ctx.Equal(0, LF_OreYield.Strike(state, 20, 60, 2), "the first pickaxe hit breaks nothing off");
		ctx.Equal(0, LF_OreYield.Strike(state, 20, 60, 2), "the second hit breaks nothing off");
		ctx.Equal(1, LF_OreYield.Strike(state, 20, 60, 2), "the third hit breaks one piece off");
		ctx.Equal(1, state.m_iBreaks, "one piece is counted");
		ctx.False(state.m_bDepleted, "one of two pieces leaves the rock standing");

		ctx.Equal(0, LF_OreYield.Strike(state, 0, 60, 2), "a zero-damage hit (wrong tool) breaks nothing");
		ctx.Equal(0, LF_OreYield.Strike(state, -5, 60, 2), "negative damage breaks nothing");
		ctx.EqualFloat(0, state.m_fDamage, 0.001, "a refused hit adds no damage");

		LF_OreYield.Strike(state, 20, 60, 2);
		LF_OreYield.Strike(state, 20, 60, 2);
		ctx.Equal(1, LF_OreYield.Strike(state, 20, 60, 2), "the sixth hit breaks the last piece");
		ctx.True(state.m_bDepleted, "the rock is spent after its last piece");
		ctx.Equal(0, LF_OreYield.Strike(state, 200, 60, 2), "a spent rock yields nothing");

		LF_OreYield.Refill(state);
		ctx.False(state.m_bDepleted, "the rock grows back");
		ctx.Equal(0, state.m_iBreaks, "a grown rock starts again at zero pieces");
		ctx.EqualFloat(0, state.m_fDamage, 0.001, "a grown rock carries no old damage");

		LF_OreYieldState big = new LF_OreYieldState();
		ctx.Equal(2, LF_OreYield.Strike(big, 120, 60, 0), "one heavy hit can break two pieces");
		ctx.Equal(1, LF_OreYield.Strike(big, 60, 60, 3), "a heavy hit stops at the rock's last piece");
		ctx.True(big.m_bDepleted, "stopping at the last piece spends the rock");

		LF_OreYieldState endless = new LF_OreYieldState();
		for (int i = 0; i < 10; i++)
			LF_OreYield.Strike(endless, 60, 60, 0);
		ctx.False(endless.m_bDepleted, "a row with no piece limit never runs out");

		ctx.Equal(0, LF_OreYield.Strike(new LF_OreYieldState(), 20, 0, 2), "a rock with no damage per piece yields nothing");
		ctx.Equal(4, LF_OreYield.ItemsFor(1, 4), "one piece of a four-yield row drops four items");
		ctx.Equal(0, LF_OreYield.ItemsFor(0, 4), "no piece drops nothing");
		ctx.Equal(0, LF_OreYield.ItemsFor(2, 0), "a zero-yield row drops nothing");

		ctx.EqualFloat(240000, LF_OreYield.RespawnDelayMs(240000, 30), 0.1, "the row's cooldown sets the grow-back time");
		ctx.EqualFloat(30000, LF_OreYield.RespawnDelayMs(0, 30), 0.1, "a row with no cooldown uses the rock's seconds");
		ctx.EqualFloat(LF_OreYield.DEFAULT_RESPAWN_MS, LF_OreYield.RespawnDelayMs(0, 0), 0.1, "no cooldown anywhere means five minutes");

		ctx.True(LF_OreNodeComponent.SameResource("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", "{23341e2840f5acb4}prefabs/tools/pickaxe.et"), "the tool match ignores casing");
		ctx.False(LF_OreNodeComponent.SameResource("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et"), "the axe is not the pickaxe");
		ctx.False(LF_OreNodeComponent.SameResource("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", ""), "no tool is not the pickaxe");
	}
}
