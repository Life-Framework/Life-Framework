//------------------------------------------------------------------------------------------------
//! Save section "medicranks" (#2708, a section since #2841): one LF_MedicRankRow per character with a
//! medic rank or medic duty seconds, keyed by the character's record key. Sections apply after every
//! legacy payload, so the "characters" payload has already set the final record keys.
//------------------------------------------------------------------------------------------------
class LF_MedicRanksSavePayload : LF_SavePayload
{
	static const string NAME = "medicranks";

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
		// Rank 0 with no seconds is not written.
		array<ref LF_MedicRankRow> rows = LF_MedicRanks.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 medic rank row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_MedicRankRow> rows = new array<ref LF_MedicRankRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_MedicRanks.FEATURE, "medic ranks failed to read, medic ranks stay as they were");
			return false;
		}

		int kept = LF_MedicRanks.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 medic rank row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before medic ranks: nothing to seed. A boot starts every standing at rank 0, and a
	//! re-apply to a live session keeps the live ranks (the save holds none to replace them with).
	override void OnAbsent()
	{
		LF_Debug.Info(LF_MedicRanks.FEATURE, "save has no medic ranks; every medic stays at the rank the session holds (rank 0 on a boot)");
	}
}
