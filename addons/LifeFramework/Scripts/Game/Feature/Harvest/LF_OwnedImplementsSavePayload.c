//------------------------------------------------------------------------------------------------
//! Save section "harvest" (#3295): one LF_OwnedImplementRow per implement a character owns, keyed by the
//! character's record key (never an entity id). Sections apply after every legacy payload, so the
//! "characters" payload has already set the final record keys. A load replaces the owned list, so a second
//! load ends in the same state. What is out in the world is session state and is not saved: an implement
//! out at a restart is back on the list and not in the world.
class LF_HarvestSavePayload : LF_SavePayload
{
	static const string NAME = "harvest";

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
		array<ref LF_OwnedImplementRow> rows = LF_OwnedImplements.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 owned implement row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_OwnedImplementRow> rows = new array<ref LF_OwnedImplementRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_OwnedImplements.FEATURE, "owned implements failed to read; the list stays as it was");
			return false;
		}

		int kept = LF_OwnedImplements.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 owned implement row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the harvester: nobody owns an implement, which is the state a boot starts in.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_OwnedImplements.FEATURE, "save has no owned implements; nobody owns a reaper yet");
	}
}
