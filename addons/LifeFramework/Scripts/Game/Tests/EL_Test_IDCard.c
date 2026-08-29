// red-proof: point Resource.Load at a nonexistent path (e.g.
// "{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard_Missing.et") or use a wrong
// GUID, and run `tools\cli test --tier all`; the resource comes back invalid and
// the ctx.True assertion fails.

// tier: WORLD
class EL_Test_IDCard : EL_Test
{
	protected static const ResourceName IDCARD_PREFAB = "{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard.et";

	override string GetName()
	{
		return "idcard/prefab-load";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(IDCARD_PREFAB);
		ctx.True(res.IsValid(), "IDCard prefab loads: " + IDCARD_PREFAB);

		Resource xob = Resource.Load("{46A5E009BCE3233E}Assets/Items/Roleplay/IDCard/IdCard.xob");
		ctx.True(xob.IsValid(), "IDCard xob loads");
	}
};