// LF_ResidentCrimeReaction.c - a resident who sees a crime remembers it (#2860, epic #1012 beat 3).
// Server side. LF_Witness.RecordCrime fills the scene (a live resident is listed as "resident:<id>")
// and calls OnScene, which applies the `crime_seen` memory for each resident witness about the suspect.
// The bus event LF_OnCrimeCommitted is published before the scene exists and carries no report number,
// so the scene is the seam, not the event.

//------------------------------------------------------------------------------------------------
class LF_ResidentCrimeReaction
{
	static const string REASON = "crime_seen";

	//------------------------------------------------------------------------------------------------
	//! The resident id a witness key names, or empty for a player or an ordinary NPC.
	static string ResidentOf(string witnessKey)
	{
		string prefix = LF_Witness.RESIDENT_PREFIX;
		if (!witnessKey.StartsWith(prefix))
			return string.Empty;

		return witnessKey.Substring(prefix.Length(), witnessKey.Length() - prefix.Length());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every resident witness of `scene` remembers the suspect.
	//! \return How many residents were moved.
	static int OnScene(notnull LF_WitnessScene scene)
	{
		if (scene.m_sSuspectUid.IsEmpty())
			return 0;

		int moved;
		foreach (LF_WitnessEntry entry : scene.m_aWitnesses)
		{
			string residentId = ResidentOf(entry.m_sKey);
			if (residentId.IsEmpty())
				continue;

			if (LF_ResidentMemory.Apply(residentId, scene.m_sSuspectUid, REASON, System.GetUnixTime()))
			{
				moved++;
				LF_Debug.Info(LF_ResidentMemory.FEATURE, string.Format("resident '%1' saw report #%2 (%3)", residentId, scene.m_iReport, scene.m_sSuspectUid));
			}
		}

		return moved;
	}
}
