// red-proof: observed with the unfixed label path (LF_MapAreas.LabelOf returning the bare WidgetManager.Translate of the key): the lawless label read "No {police}" and `no placeholder is left in the lawless label` failed. LabelOf now runs LF_Terms.Shown; green.

//------------------------------------------------------------------------------------------------
//! map/area-labels: the words the map prints inside a zone hold no `{term}` placeholder. The lawless
//! style's label key reads "No {police}"; the map's draw path shows it through LF_MapAreas.LabelOf,
//! which runs the terms expander, so it reads "No police" (or the pack's or the owner's word).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapAreaLabels : LF_Test
{
	override string GetName()
	{
		return "map/area-labels";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Terms.Reload();

		LF_MapArea lawless = new LF_MapArea();
		lawless.m_sName = "ml-lawless";
		lawless.m_eStyle = LF_MapAreaStyle.LAWLESS;
		string shown = LF_MapAreas.LabelOf(lawless);
		ctx.False(shown.Contains("{"), "no placeholder is left in the lawless label: " + shown);
		ctx.True(shown.Contains("police"), "the lawless label names the police word: " + shown);

		LF_TermBook renamed = LF_Terms.Get();
		renamed.SetOwner("police", "singular", "", "Sheriff");
		ctx.True(LF_MapAreas.LabelOf(lawless).Contains("Sheriff"), "the lawless label follows the owner's word");
		LF_Terms.Reload();

		LF_MapArea safe = new LF_MapArea();
		safe.m_sName = "ml-safe";
		safe.m_eStyle = LF_MapAreaStyle.SAFE;
		ctx.False(LF_MapAreas.LabelOf(safe).Contains("{"), "no placeholder is left in the safe label");

		LF_MapArea named = new LF_MapArea();
		named.m_sName = "ml-town";
		named.m_eStyle = LF_MapAreaStyle.TOWN;
		named.m_sLabel = "Test Town";
		ctx.EqualStr("Test Town", LF_MapAreas.LabelOf(named), "a given plain label shows as written");

		named.m_sLabel = "No {police} in {town}";
		ctx.False(LF_MapAreas.LabelOf(named).Contains("{"), "a given label with placeholders is expanded too");
	}
}
