// red-proof: PENDING
// tier: WORLD
//! Gazette (#792): the shipped config resolves, every event class and string key in it is real, and a
//! rendered story and the sheet read the way a player sees them (terms expanded, keys translated).
class LF_Test_GazetteWorld : LF_Test
{
	override string GetName()
	{
		return "gazette/gazette-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Gazette.ResetStatics();
		LF_GazetteConfig config = LF_GazetteConfig.Get();
		ctx.True(config != null, "the gazette config resolves");
		if (!config || !config.m_aRules)
			return;

		ctx.True(config.m_aRules.Count() >= 9, "the config counts the crime, arrest, murder, chop, heist, vault, job, fence and medic events");
		foreach (LF_GazetteRule rule : config.m_aRules)
		{
			typename eventType = rule.m_sEvent.ToType();
			ctx.True(eventType && eventType.IsInherited(LF_Event), string.Format("%1 is a bus event class", rule.m_sEvent));
			ctx.True(WidgetManager.Translate(rule.m_sTextKey) != rule.m_sTextKey, string.Format("%1 resolves in the string table", rule.m_sTextKey));
			if (!rule.m_sAnonKey.IsEmpty())
				ctx.True(WidgetManager.Translate(rule.m_sAnonKey) != rule.m_sAnonKey, string.Format("%1 resolves in the string table", rule.m_sAnonKey));
		}

		LF_GazetteRow row = new LF_GazetteRow();
		row.m_sKind = "crime";
		row.m_sTextKey = "#LF-Gazette_Crime";
		row.m_aArgs.Insert("Ann Lee");
		row.m_aArgs.Insert("#OFFENCE_Robbery");
		row.m_aArgs.Insert("0");
		string story = LF_Gazette.RenderRow(row);
		ctx.True(story.Contains("Ann Lee"), "the story prints the name");
		ctx.False(story.Contains("{") || story.Contains("#OFFENCE"), "terms are expanded and the offence key is translated: " + story);

		array<string> packed = {};
		packed.Insert(row.Pack());
		array<ref LF_UIPart> parts = LF_GazetteSheet.BuildParts("#LF-Gazette_Title", 7, packed);
		ctx.True(parts.Count() == 5, "the sheet is title, heading, rule, one story and close");
		ctx.True(parts[0].GetKind() == LF_EUIPartKind.TITLE && !parts[0].GetText().Contains("{"), "the paper's name is a title with its term expanded");
		array<ref LF_UIPart> empty = LF_GazetteSheet.BuildParts("#LF-Gazette_Title", 0, null);
		ctx.True(empty.Count() == 3, "before the first edition the sheet says so");
		LF_Gazette.ResetStatics();
	}
}
