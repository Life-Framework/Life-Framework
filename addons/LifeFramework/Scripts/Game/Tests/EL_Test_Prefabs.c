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
			"Prefabs/Characters/Core/Character_Roleplay.et",
			"Prefabs/Items/Food/Apple.et",
			"Prefabs/Items/Food/Plum.et",
			"Prefabs/Items/Drinks/WaterBottle.et",
			"Prefabs/Items/Currencies/MoneyStack.et",
			"Prefabs/Vehicles/LicensePlate/LicensePlate.et",
			"Prefabs/MP/Modes/Roleplay/GameMode_Roleplay.et"
		};

		foreach (string path : prefabs)
		{
			Resource res = Resource.Load(path);
			ctx.True(res.IsValid(), string.Format("prefab loads: %1", path));
		}

		Resource appleRes = Resource.Load("Prefabs/Items/Food/Apple.et");
		if (appleRes.IsValid())
		{
			EntitySpawnParams params();
			params.TransformMode = ETransformMode.LOCAL;

			IEntity apple = GetGame().SpawnEntityPrefabLocal(appleRes, GetGame().GetWorld(), params);
			ctx.True(apple != null, "Apple.et spawns an entity");
		}
	}
};