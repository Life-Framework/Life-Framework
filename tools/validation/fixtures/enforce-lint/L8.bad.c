class LF_FixtureServerToast : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_Utils.Notify("#LF-Fixture_Done", "#LF-Fixture_Title", 3.0);
	}
}
