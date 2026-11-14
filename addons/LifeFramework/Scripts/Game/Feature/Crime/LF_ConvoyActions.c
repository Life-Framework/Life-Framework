//------------------------------------------------------------------------------------------------
//! Starts the hideout convoy at the barn and hands the member a crate of spirits.
//! The vehicle the member is in, or the first one they get into, is watched;
//! leaving it away from the drop fails the run.
//------------------------------------------------------------------------------------------------
class LF_ConvoyStartAction : ScriptedUserAction
{
	//! The Crime system is off (#1076): no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled("Crime");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Contraband_Start";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_ConvoyStartAction"))
			return;

		if (!Replication.IsServer())
			return;

		string characterId = LF_Hideout.CharacterIdOf(pUserEntity);
		if (characterId.IsEmpty())
		{
			LF_Debug.Error("Contraband", "convoy start refused: no character");
			return;
		}

		// The offer cannot see organizations on the client, so the refusal has to reach the
		// player from here, or they press the crate and nothing happens (#157).
		string refusal = LF_Contraband.StartRefusalKey(characterId);
		if (!LF_Contraband.Start(characterId, pUserEntity))
		{
			if (!refusal.IsEmpty())
				LF_Utils.NotifyEntity(pUserEntity, refusal, "#LF-Contraband_Title", 5.0);
			return;
		}

		LF_Contraband.GiveCrate(pUserEntity);
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
	//! Only the cage the game mode names as the drop takes crates; the other one stays inert.
	protected bool IsActiveDrop(IEntity owner)
	{
		LF_ConvoyMarkerComponent marker = LF_Component<LF_ConvoyMarkerComponent>.Find(owner);
		if (!marker)
			return true;

		return marker.IsNearDrop() == LF_Contraband.IsNearDropActive();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Crime"))
			return false;

		return IsActiveDrop(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return IsActiveDrop(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_ConvoyDeliverAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!IsActiveDrop(pOwnerEntity))
		{
			LF_Debug.Warn("Contraband", "delivery refused: this is not the active drop");
			return;
		}

		string characterId = LF_Hideout.CharacterIdOf(pUserEntity);
		if (characterId.IsEmpty())
		{
			LF_Debug.Error("Contraband", "delivery refused: no character");
			return;
		}

		int carried = LF_Contraband.CountCarried(pUserEntity);
		int paid = LF_Contraband.Deliver(characterId, carried, pUserEntity);
		if (paid > 0)
			LF_Debug.Log("Contraband", string.Format("drop paid %1", paid));
	}
}
