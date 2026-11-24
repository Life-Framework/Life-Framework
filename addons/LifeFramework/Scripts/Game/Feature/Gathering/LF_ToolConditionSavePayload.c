//------------------------------------------------------------------------------------------------
//! Tool conditions of one stored snapshot (#4821), parallel to its item list (-1 = not a gathering
//! tool), keyed by the snapshot's stable owner key and slot name. Field order is the save order;
//! append only.
class LF_StorageToolConditionRecord : Managed
{
	string m_sOwnerKey;
	string m_sSlot;
	ref array<float> m_aConditions = {};
}

//------------------------------------------------------------------------------------------------
//! Save section "toolcondition" (#4821): the condition of every gathering tool carried or kept in a
//! stored snapshot, so a worn pickaxe in the pack is still worn after a restart. Tools on the ground
//! and in world storage save through LF_ToolConditionComponentSerializer instead. Sections apply after
//! every legacy payload, so the "carried" payload has already loaded the snapshots this fills in.
//------------------------------------------------------------------------------------------------
class LF_ToolConditionSavePayload : LF_SavePayload
{
	static const string NAME = "toolcondition";

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
		array<ref LF_StorageToolConditionRecord> rows = LF_StorageSnapshot.ExportToolConditions();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved tool conditions of %1 snapshot(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_StorageToolConditionRecord> rows = new array<ref LF_StorageToolConditionRecord>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_ToolWear.FEATURE, "tool conditions failed to read, carried tools load at full");
			return false;
		}

		int matched = LF_StorageSnapshot.ApplyToolConditions(rows);
		LF_Debug.Info("Persistence", string.Format("applied tool conditions to %1 of %2 snapshot(s)", matched, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before #4821. Tool condition was never saved before (the per-player store of #4300
	//! lived in memory only), so there is nothing to migrate: every carried tool loads at full, as it
	//! did after any restart before.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_ToolWear.FEATURE, "save has no tool conditions; carried tools load at full condition");
	}
}
