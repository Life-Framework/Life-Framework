// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: appending
// "{0000000000000002}Prefabs/Items/Food/Missing.et" to the prefab list failed it with
// `[LFTEST] FAIL prefabs-load-and-spawn`,
// `- prefab loads: {0000000000000002}Prefabs/Items/Food/Missing.et`. Restored; green.
// tier: WORLD
class LF_Test_Prefabs : LF_Test
{
	override string GetName()
	{
		return "prefabs-load-and-spawn";
	}

	override void Run(LF_TestContext ctx)
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
			ctx.True(res != null && res.IsValid(), string.Format("prefab loads: %1", path));
		}

		Resource appleRes = Resource.Load("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et");
		if (appleRes != null && appleRes.IsValid())
		{
			EntitySpawnParams params();
			params.TransformMode = ETransformMode.LOCAL;

			IEntity apple = GetGame().SpawnEntityPrefabLocal(appleRes, GetGame().GetWorld(), params);
			ctx.True(apple != null, "Apple.et spawns an entity");
		}
	}
};