//------------------------------------------------------------------------------------------------
//! Save section "welcomeguide": one LF_WelcomeGuideRow per character whose welcome guide still opens
//! by itself (first spawned, "Don't show again" not pressed yet), keyed by the character's persistence id.
//------------------------------------------------------------------------------------------------
class LF_WelcomeGuideSavePayload : LF_SavePayload
{
	static const string NAME = "welcomeguide";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_WelcomeGuideRow> rows = LF_WelcomeGuide.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 welcome guide row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_WelcomeGuideRow> rows = new array<ref LF_WelcomeGuideRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_WelcomeGuide.FEATURE, "welcome guide rows failed to read, the pending set stays as it was");
			return false;
		}

		int kept = LF_WelcomeGuide.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 welcome guide row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the guide: every character in it has already played, so none gets it by itself.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_WelcomeGuide.FEATURE, "save has no welcome guide rows; only characters made from now on see it by themselves");
	}
}
