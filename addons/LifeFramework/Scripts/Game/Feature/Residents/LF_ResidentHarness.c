//! Agent harness names for spawned residents (#3650). A spawned resident has no layer name, so `stand` and
//! `use` could only reach the nearest one. `Resident_<id>` (a token the harness parser accepts, like
//! `Player<id>`) names the live resident with that `LF_ResidentDef` id, for example
//! `stand Resident_night_shift` and `use LF_DialogueTalkAction Resident_night_shift`. Server side: the
//! spawner's live table exists only there, so a client's own lookup finds nothing and the verb goes on to the
//! server (`LF_AgentHarness.DoUse`).
class LF_ResidentHarness
{
	static const string NAME_PREFIX = "Resident_";

	//------------------------------------------------------------------------------------------------
	//! The resident id a harness entity name asks for, or empty when it is not a `Resident_<id>` name.
	static string IdOf(string entityName)
	{
		if (!entityName.StartsWith(NAME_PREFIX) || entityName.Length() <= NAME_PREFIX.Length())
			return string.Empty;

		return entityName.Substring(NAME_PREFIX.Length(), entityName.Length() - NAME_PREFIX.Length());
	}

	//------------------------------------------------------------------------------------------------
	//! The live resident a `Resident_<id>` name stands for, or null (not that form, not live, or on a client).
	static IEntity Named(string entityName)
	{
		string id = IdOf(entityName);
		if (id.IsEmpty())
			return null;

		IEntity live = LF_ResidentSpawner.GetLive(id);
		if (!live && Replication.IsServer())
			LF_Debug.Warn("Residents", string.Format("harness: resident '%1' is not live (not due, or no player near its spot)", id));

		return live;
	}
}
