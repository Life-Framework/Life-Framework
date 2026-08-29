// red-proof: point a path at a nonexistent resource (e.g. swap the CashMaterial.emat
// GUID for CashMaterial_Missing.emat) and run `tools\cli test --tier all`; Resource.Load
// yields an invalid resource and the ctx.True assertion fails.
//
// The CashTexture_*.edds files are deliberately not Resource.Load'ed here: the dedicated
// server registers no texture loader, so a direct .edds load always fails headless. The
// CashMaterial.emat load below proves the material exists and its texture references
// resolve at build time (the resource scan flags any orphan .edds.meta).

// tier: WORLD
class EL_Test_MoneyRework : EL_Test
{
	override string GetName()
	{
		return "money/rework-assets";
	}

	override void Run(EL_TestContext ctx)
	{
		array<string> assets = {
			"{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et",
			"{773BE5E227A5A9CB}Assets/Items/Currencies/MoneyStack.xob",
			"{BE94B9963D91D4FF}Assets/Items/Currencies/Data/CashMaterial.emat"
		};

		foreach (string path : assets)
		{
			Resource res = Resource.Load(path);
			ctx.True(res.IsValid(), string.Format("asset loads: %1", path));
		}
	}
};