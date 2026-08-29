// red-proof: change an expected value (e.g. AddCash return 200 -> 199, or
// GetCash 50 -> 51) and run `tools\cli test --tier all`. Red run pending:
// first boot after the EPF dependency removal lands.
class EL_Test_MoneyCash : EL_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "money/cash-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads: " + CHARACTER_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(character != null, "character spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(EL_MoneyUtils.AddCash(character, 200) == 200, "AddCash(200) reports 200 added");
		ctx.Equal(200, EL_MoneyUtils.GetCash(character), "GetCash reads 200 after AddCash");

		ctx.Equal(150, EL_MoneyUtils.TakeCash(character, 150), "TakeCash(150) reports 150 taken");
		ctx.Equal(50, EL_MoneyUtils.GetCash(character), "GetCash reads 50 after TakeCash(150)");

		ctx.Equal(50, EL_MoneyUtils.TakeCash(character, 999), "overdraw TakeCash(999) takes only the remaining 50");
		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "GetCash reads 0 after overdraw");

		ctx.Equal(0, EL_MoneyUtils.AddCash(character, -5), "negative AddCash is rejected");
		ctx.Equal(0, EL_MoneyUtils.AddCash(character, 0), "zero AddCash is rejected");
		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "GetCash unaffected by rejected AddCash");
	}
}
