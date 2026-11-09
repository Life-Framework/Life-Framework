class LF_FixtureServerToast : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_Utils.NotifyEntity(pUserEntity, "#LF-Fixture_Done", "#LF-Fixture_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the owning client, so a local hint is seen.
	void ShowLocal()
	{
		LF_Utils.Notify("#LF-Fixture_Done", "#LF-Fixture_Title", 3.0);
	}
}
