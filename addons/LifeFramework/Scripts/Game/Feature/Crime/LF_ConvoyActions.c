//------------------------------------------------------------------------------------------------
//! Starts the hideout convoy. A member who is seated is watched; leaving that
//! vehicle fails the run.
//------------------------------------------------------------------------------------------------
class LF_ConvoyStartAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Contraband_Start";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string characterId = LF_Hideout.CharacterIdOf(pUserEntity);
		if (characterId.IsEmpty())
		{
			EL_Debug.Error("Contraband", "convoy start refused: no character");
			return;
		}

		// The offer cannot see organizations on the client, so the refusal has to reach the
		// player from here, or they press the crate and nothing happens (#157).
		string refusal = LF_Contraband.StartRefusalKey(characterId);
		if (!LF_Contraband.Start(characterId, pUserEntity) && !refusal.IsEmpty())
			EL_Utils.NotifyEntity(pUserEntity, refusal, "#LF-Contraband_Title", 5.0);
	}
}

//------------------------------------------------------------------------------------------------
//! Pays the configured cash once when the member still holds the contraband.
//------------------------------------------------------------------------------------------------
class LF_ConvoyDeliverAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Contraband_Deliver";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string characterId = LF_Hideout.CharacterIdOf(pUserEntity);
		if (characterId.IsEmpty())
		{
			EL_Debug.Error("Contraband", "delivery refused: no character");
			return;
		}

		int carried = LF_Contraband.CountCarried(pUserEntity);
		int paid = LF_Contraband.Deliver(characterId, carried, pUserEntity);
		if (paid > 0)
			EL_Debug.Log("Contraband", string.Format("drop paid %1", paid));
	}
}
