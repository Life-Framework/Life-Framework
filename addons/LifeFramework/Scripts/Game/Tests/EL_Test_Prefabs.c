// red-proof: append a nonexistent path (e.g. "Prefabs/Items/Food/Missing.et")
// to the prefab list and run `tools\cli test --tier all`. Red run pending:
// first boot after the EPF dependency removal lands.
class EL_Test_Prefabs : EL_Test
{
	override string GetName()
	{
		return "prefabs-load-and-spawn";
	}

	override void Run(EL_TestContext ctx)
	{
		array<string> prefabs = {
			"{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et",
			"{C9D661E5B0714711}Prefabs/Items/Food/Apple.et",
			"{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et",
			"{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et",
			"{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et",
			"{E95486C43308F36B}Prefabs/Vehicles/LicensePlate/LicensePlate.et",
			"{03913608C59017A3}Prefabs/MP/Modes/Roleplay/GameMode_Roleplay.et"
		};

		foreach (string path : prefabs)
		{
			Resource res = Resource.Load(path);
			ctx.True(res.IsValid(), string.Format("prefab loads: %1", path));
		}

		Resource appleRes = Resource.Load("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et");
		if (appleRes.IsValid())
		{
			EntitySpawnParams params();
			params.TransformMode = ETransformMode.LOCAL;

			IEntity apple = GetGame().SpawnEntityPrefabLocal(appleRes, GetGame().GetWorld(), params);
			ctx.True(apple != null, "Apple.et spawns an entity");
		}
	}
};