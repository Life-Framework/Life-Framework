// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_LootRules` printed `RED-OK loot/rules (fails with the hook on, 36 assertions)` (the hook points the config at a missing file and zeroes the seeded table's weights); `--only` then passed=1 failed=0.
//! Loot table rules (#3341, slice of #1020): a seeded roll returns predictable rows, weights shift the
//! counts, refill waits for the cooldown, and a bad table logs and rolls nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LootRules : LF_Test
{
	protected static const ResourceName MISSING = "Configs/Loot/LF_NoSuchLoot.conf";
	protected static const ResourceName PREFAB_A = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName PREFAB_B = "{0D5DA590E223A195}Prefabs/Items/Resources/Brick.et";

	override string GetName()
	{
		return "loot/rules";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_LootRowDef Row(ResourceName prefab, int weight, int countMin, int countMax)
	{
		LF_LootRowDef row = new LF_LootRowDef();
		row.m_Prefab = prefab;
		row.m_iWeight = weight;
		row.m_iCountMin = countMin;
		row.m_iCountMax = countMax;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_LootTableDef Table(string kind, int rolls, int refillSeconds)
	{
		LF_LootTableDef table = new LF_LootTableDef();
		table.m_sKind = kind;
		table.m_iRolls = rolls;
		table.m_iRefillSeconds = refillSeconds;
		table.m_aRows = new array<ref LF_LootRowDef>();
		return table;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_LootConfig.Reload();
		if (RedProof())
			LF_LootConfig.Reload(MISSING);

		// The shipped example resolves and every table in it is rollable.
		LF_LootConfig config = LF_LootConfig.Get();
		ctx.NotNull(config, "the shipped Configs/Loot/LF_Loot.conf resolves");
		LF_LootTableDef crate = LF_LootConfig.TableOf("crate");
		ctx.NotNull(crate, "the example config has a crate table");
		if (crate)
		{
			ctx.EqualStr("", LF_LootRules.Validate(crate), "the shipped crate table is valid");
			array<ref LF_LootDrop> shipped = {};
			ctx.Equal(crate.m_iRolls, LF_LootRules.Roll(crate, 7, shipped), "a roll gives one drop per fill roll");
		}

		LF_LootConfig.Reload();

		// A seeded roll: the zero-weight row is never picked, counts stay in range, the same seed repeats.
		int rareWeight = 0;
		if (!RedProof())
			rareWeight = 1;

		LF_LootTableDef seeded = Table("seeded", 5, 600);
		seeded.m_aRows.Insert(Row(PREFAB_A, rareWeight, 2, 4));
		seeded.m_aRows.Insert(Row(PREFAB_B, 0, 1, 1));
		array<ref LF_LootDrop> first = {};
		array<ref LF_LootDrop> again = {};
		ctx.Equal(5, LF_LootRules.Roll(seeded, 12345, first), "five rolls give five drops");
		ctx.Equal(5, LF_LootRules.Roll(seeded, 12345, again), "the same seed rolls again");
		bool allExpected = true;
		bool allEqual = true;
		foreach (int i, LF_LootDrop drop : first)
		{
			if (drop.m_Prefab != PREFAB_A || drop.m_iCount < 2 || drop.m_iCount > 4)
				allExpected = false;

			if (i < again.Count() && (drop.m_Prefab != again[i].m_Prefab || drop.m_iCount != again[i].m_iCount))
				allEqual = false;
		}

		ctx.True(allExpected, "only the row with weight, and a count inside its range");
		ctx.True(allEqual, "the same seed gives the same drops");

		// A seed of 0 or below still rolls (it never stalls the generator).
		array<ref LF_LootDrop> zero = {};
		ctx.Equal(5, LF_LootRules.Roll(seeded, 0, zero), "seed 0 rolls");
		ctx.Equal(5, LF_LootRules.Roll(seeded, -99, zero), "a negative seed rolls");

		// Weights shift the counts: 3:1, one roll at a time over 1000 chained seeds.
		LF_LootTableDef weighted = Table("weighted", 1, 0);
		weighted.m_aRows.Insert(Row(PREFAB_A, 3, 1, 1));
		weighted.m_aRows.Insert(Row(PREFAB_B, 1, 1, 1));
		int heavy;
		int light;
		int seed = 2026;
		array<ref LF_LootDrop> one = {};
		for (int n = 0; n < 1000; n++)
		{
			seed = LF_LootRules.NextSeed(seed);
			LF_LootRules.Roll(weighted, seed, one);
			if (one.IsEmpty())
				continue;

			if (one[0].m_Prefab == PREFAB_A)
				heavy++;
			else
				light++;
		}

		ctx.Equal(1000, heavy + light, "every one of 1000 rolls picked a row");
		ctx.InRange(heavy, 680, 820, "the weight-3 row is picked about 750 times in 1000");
		ctx.InRange(light, 180, 320, "the weight-1 row is picked about 250 times in 1000");
		ctx.True(heavy > light, "the heavier row wins more often");

		// Refill: refused before the cooldown, allowed at it and after, allowed when never filled.
		LF_LootTableDef cool = Table("cool", 1, 600);
		cool.m_aRows.Insert(Row(PREFAB_A, 1, 1, 1));
		ctx.False(LF_LootRules.CanRefill(cool, 1000, 1000), "refused right after the fill");
		ctx.False(LF_LootRules.CanRefill(cool, 1000, 1599), "refused one second before the cooldown ends");
		ctx.True(LF_LootRules.CanRefill(cool, 1000, 1600), "allowed when the cooldown ends");
		ctx.True(LF_LootRules.CanRefill(cool, 1000, 5000), "allowed long after");
		ctx.True(LF_LootRules.CanRefill(cool, -1, 0), "a container that never filled may fill");
		ctx.Equal(100, LF_LootRules.SecondsUntilRefill(cool, 1000, 1500), "100 s left of the cooldown");
		ctx.Equal(0, LF_LootRules.SecondsUntilRefill(cool, 1000, 1700), "no wait left past the cooldown");

		// A zero refill time refills at once.
		LF_LootTableDef instant = Table("instant", 1, 0);
		instant.m_aRows.Insert(Row(PREFAB_A, 1, 1, 1));
		ctx.True(LF_LootRules.CanRefill(instant, 500, 500), "refill time 0 refills at once");

		// Bad configs log and return nothing, never crash.
		array<ref LF_LootDrop> bad = {};
		bad.Insert(new LF_LootDrop());
		ctx.Equal(0, LF_LootRules.Roll(null, 1, bad), "a null table rolls nothing");
		ctx.True(bad.IsEmpty(), "and clears the result");

		LF_LootTableDef empty = Table("empty", 3, 600);
		ctx.Equal(0, LF_LootRules.Roll(empty, 1, bad), "a table with no rows rolls nothing");
		ctx.False(LF_LootRules.CanRefill(empty, -1, 0), "a table with no rows never refills");

		LF_LootTableDef zeroWeights = Table("zeroWeights", 3, 600);
		zeroWeights.m_aRows.Insert(Row(PREFAB_A, 0, 1, 1));
		zeroWeights.m_aRows.Insert(Row(PREFAB_B, 0, 1, 1));
		ctx.Equal(0, LF_LootRules.Roll(zeroWeights, 1, bad), "a table whose weights are all 0 rolls nothing");

		LF_LootTableDef noPrefab = Table("noPrefab", 3, 600);
		noPrefab.m_aRows.Insert(Row(ResourceName.Empty, 1, 1, 1));
		ctx.Equal(0, LF_LootRules.Roll(noPrefab, 1, bad), "a row with no prefab rolls nothing");

		LF_LootTableDef turned = Table("turned", 3, 600);
		turned.m_aRows.Insert(Row(PREFAB_A, 1, 4, 2));
		ctx.Equal(0, LF_LootRules.Roll(turned, 1, bad), "a count range turned round rolls nothing");

		LF_LootTableDef noRolls = Table("noRolls", 0, 600);
		noRolls.m_aRows.Insert(Row(PREFAB_A, 1, 1, 1));
		ctx.Equal(0, LF_LootRules.Roll(noRolls, 1, bad), "a fill size of 0 rolls nothing");

		LF_LootTableDef negative = Table("negative", 1, -5);
		negative.m_aRows.Insert(Row(PREFAB_A, 1, 1, 1));
		ctx.False(LF_LootRules.Validate(negative).IsEmpty(), "a negative refill time is a bad config");

		ctx.False(LF_LootRules.Validate(zeroWeights).IsEmpty(), "Validate names why an all-zero table is bad");
	}
}
