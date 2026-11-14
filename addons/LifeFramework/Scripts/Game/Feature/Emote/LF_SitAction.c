// LF_SitAction.c — the F-key "Sit" / "Stand up" on a chair or bench (#982).
// Built on the base game loiter action (SCR_LoiterUserAction): the server fills the seat position, the
// action is broadcast, the sitter's owner starts the loiter and the base game sends the pose to every
// other machine (Rpc_StartLoitering_BCNO) and to late joiners (SCR_CharacterCommandHandlerComponent
// OnRplSave). Unlike the base action it needs no AI smart-action component: the seat is this action's
// own offset and facing, so a bench carries one action per place to sit. Performing it again while
// seated stands up (SCR_UserActionWithOccupancy), as do jump, sprint or raising a weapon.

class LF_SitAction : SCR_LoiterUserAction
{
	[Attribute("0 0 0.62", UIWidgets.Coords, "Where the sitter stands before the pose plays, in the seat's local space. The body stays here (no root motion) so its capsule stays clear of the seat; the pose puts the hips about 0.45 m behind it, on the seat", params: "inf inf 0 purpose=coords space=entity")]
	protected vector m_vSeatOffset;

	[Attribute("180", UIWidgets.Slider, "Turn of the target transform about the seat's up axis, in degrees. The base game chair pose plays facing away from the target's +z, so 180 seats a body facing the seat's +z (the front of the base game benches and chairs)", "-180 180 1")]
	protected float m_fSeatYaw;

	[Attribute("0", UIWidgets.CheckBox, "Let the sit animation move the body. Off for a seat: root motion walks the capsule into the seat and the body climbs onto it")]
	protected bool m_bRootMotion;

	[Attribute(desc: "The sit pose. The base game chair sit is command value 7; leave the item preset NONE so no prop chair spawns")]
	protected ref SCR_LoiterCustomAnimData m_SitAnimation;

	//------------------------------------------------------------------------------------------------
	//! The seat is taken by someone who is really sitting on it. A sitter who left another way (died,
	//! was moved) does not hold it.
	protected bool IsTakenByOther(IEntity user)
	{
		if (!m_Occupant || m_Occupant == user)
			return false;

		SCR_ChimeraCharacter occupant = SCR_ChimeraCharacter.Cast(m_Occupant);
		if (!occupant)
			return false;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(occupant.GetCharacterController());
		return controller && controller.IsLoiteringOnEntity(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! A server with Emote off offers no seat (#1075).
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_Emote.SYSTEM))
			return false;

		return super.CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Standing up always works, even if Emote went off while someone sat.
		if (IsOccupiedBy(user))
			return true;

		if (!LF_Systems.IsEnabled(LF_Emote.SYSTEM))
			return false;

		LF_ESitRefusal refusal = LF_Sit.DecideFor(user, IsTakenByOther(user));
		if (refusal != LF_ESitRefusal.NONE)
		{
			SetCannotPerformReason(LF_Sit.ReasonKey(refusal));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		IEntity local = SCR_PlayerController.GetLocalControlledEntity();
		if (local && IsOccupiedBy(local))
			outName = "#LF-Sit_StandUp";
		else
			outName = "#LF-Sit_Action";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine, from the broadcast. Only the sitter's owner starts the loiter (StartLoitering
	//! returns elsewhere); the base game then replicates it.
	override void StartAction(IEntity pUserEntity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pUserEntity);
		if (!character)
		{
			LF_Debug.Error(LF_Sit.FEATURE, "sit: the user is not a character");
			return;
		}

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (!controller)
		{
			LF_Debug.Error(LF_Sit.FEATURE, "sit: the character has no controller");
			return;
		}

		if (!m_isValid)
		{
			LF_Debug.Warn(LF_Sit.FEATURE, "sit: no seat data arrived from the server; not sitting");
			return;
		}

		m_isValid = false;

		SCR_LoiterCustomAnimData pose;
		if (m_SitAnimation)
			pose = SCR_LoiterCustomAnimData.Cast(m_SitAnimation.Clone());

		RplComponent rpl = character.GetRplComponent();
		if (rpl && !rpl.IsOwner())
			return;

		controller.StartLoitering(GetOwner(), m_eLoiteringType, m_bHolsterWeapon, m_bRootMotion, true, m_targetPosition, false, pose);
		LF_Debug.Info(LF_Sit.FEATURE, string.Format("sit: %1 sits on %2 at %3", character.GetID(), GetOwner().GetID(), m_targetPosition[3]));
	}

	//------------------------------------------------------------------------------------------------
	override void StopAction(IEntity pUserEntity)
	{
		super.StopAction(pUserEntity);

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pUserEntity);
		if (!character)
			return;

		RplComponent rpl = character.GetRplComponent();
		if (rpl && rpl.IsOwner())
			LF_Debug.Info(LF_Sit.FEATURE, string.Format("sit: %1 stands up from %2", character.GetID(), GetOwner().GetID()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write the seat. Same layout as the base action (occupant, else type, holster and the
	//! four rows of the transform) so its OnLoadActionData reads it on every machine.
	override protected bool OnSaveActionData(ScriptBitWriter writer)
	{
		IEntity owner = GetOwner();
		if (m_Occupant)
		{
			SCR_CharacterControllerComponent occupantController = SCR_CharacterControllerComponent.Cast(m_Occupant.FindComponent(SCR_CharacterControllerComponent));
			if (!occupantController || !occupantController.IsLoiteringOnEntity(owner))
				ReleaseOccupant();
		}

		if (SaveOccupant(writer))
			return true;

		vector seat[4];
		owner.GetWorldTransform(seat);
		LF_Sit.SeatTransform(seat, m_vSeatOffset, m_fSeatYaw, m_targetPosition);
		m_eLoiteringType = ELoiteringType.CUSTOM;
		m_bHolsterWeapon = true;
		m_isValid = true;

		writer.WriteInt((int)m_eLoiteringType);
		writer.WriteBool(m_bHolsterWeapon);
		writer.WriteVector(m_targetPosition[0]);
		writer.WriteVector(m_targetPosition[1]);
		writer.WriteVector(m_targetPosition[2]);
		writer.WriteVector(m_targetPosition[3]);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! For tests and proofs: the seat transform this action would send.
	void LF_GetSeatTransform(out vector result[4])
	{
		vector seat[4];
		GetOwner().GetWorldTransform(seat);
		LF_Sit.SeatTransform(seat, m_vSeatOffset, m_fSeatYaw, result);
	}

	//------------------------------------------------------------------------------------------------
	bool LF_HasPose()
	{
		return m_SitAnimation != null;
	}
}
