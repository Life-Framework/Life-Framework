//------------------------------------------------------------------------------------------------
//! "Book suspect" on the booking desk (#4420). The officer scrolls on the desk; the server picks the
//! cuffed suspect beside them and LF_BookingDesk.RequestBook re-checks every input.
class LF_BookingDeskBookAction : ScriptedUserAction
{
	protected static IEntity s_Nearest;
	protected static IEntity s_Seeker;
	protected static float s_fNearest;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police") || !user || !GetOwner())
			return false;

		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!FindSuspect(user))
		{
			SetCannotPerformReason("#LF-BookingDesk_NoSuspect");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The cuffed character nearest the officer within arrest reach, or null.
	static IEntity FindSuspect(IEntity officer)
	{
		if (!officer || !GetGame().GetWorld())
			return null;

		s_Nearest = null;
		s_Seeker = officer;
		s_fNearest = LF_PoliceConfig.ArrestReach();
		GetGame().GetWorld().QueryEntitiesBySphere(officer.GetOrigin(), s_fNearest, CollectSuspect, null, EQueryEntitiesFlags.DYNAMIC);
		IEntity found = s_Nearest;
		s_Nearest = null;
		s_Seeker = null;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! The replication id of a character: the id its RplComponent holds, which LF_NetworkUtils.FindEntityByRplId
	//! resolves back. `Replication.FindId(entity)` is not that id (it named no character on the island, so every
	//! "Book suspect" was refused as "no suspect" with a cuffed suspect beside the officer, #4699).
	//! \return The id, or RplId.Invalid() for no entity or one without a RplComponent.
	static RplId SuspectRplId(IEntity suspect)
	{
		if (!suspect)
			return RplId.Invalid();

		RplComponent rpl = RplComponent.Cast(suspect.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectSuspect(IEntity entity)
	{
		if (!SCR_ChimeraCharacter.Cast(entity) || entity == s_Seeker || !LF_Restraint.IsRestrained(entity))
			return true;

		float distance = vector.Distance(entity.GetOrigin(), s_Seeker.GetOrigin());
		if (distance <= s_fNearest)
		{
			s_fNearest = distance;
			s_Nearest = entity;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || LF_Systems.Refuses("Police", "LF_BookingDeskBookAction"))
			return;

		RplId suspectId = SuspectRplId(FindSuspect(pUserEntity));

		// The officer reads and signs the charge sheet (#4426); the offer is made when it comes back signed.
		string refusal = LF_ChargeSheetDesk.Open(pUserEntity, pOwnerEntity.GetOrigin(), suspectId);
		int officerId = LF_PlayerInteraction.PlayerIdOf(pUserEntity);
		if (!refusal.IsEmpty() && officerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-BookingDesk_Title", refusal, 5.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The suspect's two answers at the desk (#4420): shown only on the booked suspect's own machine,
//! which alone received the offer. The label numbers are the server's; the server decides again.
class LF_BookingDeskAnswerAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.CheckBox, desc: "On: \"Pay $X now\". Off: \"Take the time (N min)\"")]
	protected bool m_bPay;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user || user != LF_Utils.GetLocalCharacter())
			return false;

		if (m_bPay)
			return LF_BookingDeskClient.IsPayable();

		return LF_BookingDeskClient.IsOpen();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_bPay)
			outName = LF_Terms.Text("#LF-BookingDesk_Pay", LF_BookingDeskClient.Fine().ToString());
		else
			outName = LF_Terms.Text("#LF-BookingDesk_Time", LF_BookingDesk.Minutes(LF_BookingDeskClient.Seconds()).ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || LF_Systems.Refuses("Police", "LF_BookingDeskAnswerAction"))
			return;

		if (vector.Distance(pOwnerEntity.GetOrigin(), pUserEntity.GetOrigin()) > LF_BookingDesk.DESK_METERS)
		{
			LF_Debug.Warn(LF_BookingDesk.FEATURE, "desk answer refused: the suspect is not at the desk");
			return;
		}

		string refusal;
		if (m_bPay)
			refusal = LF_BookingDesk.PayNow(pUserEntity);
		else
			refusal = LF_BookingDesk.TakeTime(pUserEntity);

		int playerId = LF_PlayerInteraction.PlayerIdOf(pUserEntity);
		if (!refusal.IsEmpty() && playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-BookingDesk_Title", refusal, 5.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}
