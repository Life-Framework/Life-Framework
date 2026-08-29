// red-proof: restore the fork's early batch abort (a draw of 0 returns from
// SpawnOres before any bin is checked) and drop the clamp in
// ComputeSpawnCount; run `tools\cli test --tier fast` on the main serial
// thread and the "draw 0 maps to the first bin" boundary assertions plus the
// "(10,12) clamps to 0" assertion fail. Deferred to the serial suite here
// because this workstream may not boot the test server (port collision gate).

// tier: LOGIC
class EL_Test_MiningAreaWeightedSpawns : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/area-weighted-spawns";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		ref array<ref EL_WeightedSpawn> empty = new array<ref EL_WeightedSpawn>();
		ctx.Equal(0, EL_MiningArea.GetTotalWeight(empty), "empty weight list sums to zero");
		ctx.Equal(-1, EL_MiningArea.PickWeightedIndex(empty, 0), "empty weight list picks nothing");
		ref array<ref EL_WeightedSpawn> nullList = null;
		ctx.Equal(-1, EL_MiningArea.PickWeightedIndex(nullList, 0), "null weight list picks nothing");

		ref array<ref EL_WeightedSpawn> zeroWeights = new array<ref EL_WeightedSpawn>();
		zeroWeights.Insert(new EL_WeightedSpawn());
		zeroWeights.Insert(new EL_WeightedSpawn());
		ctx.Equal(0, EL_MiningArea.GetTotalWeight(zeroWeights), "all-zero chances sum to zero");
		ctx.Equal(-1, EL_MiningArea.PickWeightedIndex(zeroWeights, 0), "zero total weight picks nothing");

		ref array<ref EL_WeightedSpawn> weights = new array<ref EL_WeightedSpawn>();
		EL_WeightedSpawn copper = new EL_WeightedSpawn();
		copper.m_iChance = 50;
		weights.Insert(copper);
		EL_WeightedSpawn iron = new EL_WeightedSpawn();
		iron.m_iChance = 30;
		weights.Insert(iron);
		EL_WeightedSpawn gold = new EL_WeightedSpawn();
		gold.m_iChance = 20;
		weights.Insert(gold);

		ctx.Equal(100, EL_MiningArea.GetTotalWeight(weights), "chances accumulate to the total");

		ctx.Equal(0, EL_MiningArea.PickWeightedIndex(weights, 0), "draw 0 maps to the first bin");
		ctx.Equal(0, EL_MiningArea.PickWeightedIndex(weights, 49), "draw 49 maps to the first bin");
		ctx.Equal(1, EL_MiningArea.PickWeightedIndex(weights, 50), "draw 50 maps to the second bin");
		ctx.Equal(1, EL_MiningArea.PickWeightedIndex(weights, 79), "draw 79 maps to the second bin");
		ctx.Equal(2, EL_MiningArea.PickWeightedIndex(weights, 80), "draw 80 maps to the third bin");
		ctx.Equal(2, EL_MiningArea.PickWeightedIndex(weights, 99), "draw 99 maps to the third bin");
		ctx.Equal(-1, EL_MiningArea.PickWeightedIndex(weights, 100), "draw outside the total picks nothing");

		ctx.Equal(0, EL_MiningArea.ComputeSpawnCount(0, 0), "zero-amount area spawns nothing");
		ctx.Equal(7, EL_MiningArea.ComputeSpawnCount(10, 3), "spawns the shortfall to the amount");
		ctx.Equal(0, EL_MiningArea.ComputeSpawnCount(10, 10), "full area spawns nothing");
		ctx.Equal(0, EL_MiningArea.ComputeSpawnCount(10, 12), "over-full area clamps to zero");
	}
}