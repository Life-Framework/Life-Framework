// red-proof: expected the title "police-gates"; `tools\cli test --no-build --only LF_Test_SignGridLabel`
// (2026-11-15) failed signs/grid-label with "the title is the slug with spaces (expected 'police-gates', got 'police gates')".

// tier: LOGIC
class LF_Test_SignGridLabel : LF_Test
{
	override string GetName()
	{
		return "signs/grid-label";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SignDef label = LF_SignCatalog.GridLabel("grid:police-gates");
		ctx.True(LF_SignCatalog.IsUsable(label), "a grid cell id makes a usable sign");
		if (label)
		{
			ctx.EqualStr("police gates", label.m_sTitle, "the title is the slug with spaces");
			ctx.EqualStr("POLICE GATES", LF_SignComponent.FaceText(label), "the plate prints it in capitals");
		}

		ctx.True(LF_SignCatalog.Find("grid:police-gates") != null, "Find answers a grid id without the config");
		ctx.True(LF_SignCatalog.GridLabel("grid:") == null, "an empty slug is no label");
		ctx.True(LF_SignCatalog.GridLabel("bank") == null, "a catalog id is not a grid label");
		ctx.True(LF_SignCatalog.GridLabel("") == null, "an empty id is no label");
	}
}
