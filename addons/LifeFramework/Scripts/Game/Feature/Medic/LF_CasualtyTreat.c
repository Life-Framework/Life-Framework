//------------------------------------------------------------------------------------------------
//! "Treat the patient" on a casualty (#4828, 0.3.0 plan, medic work). A casualty is a hurt resident the
//! server spawned for a medic call (LF_CasualtyCalls, #4827). An on-duty medic beside one holds the same
//! hold as a revive (the casualty's character carries LF_ReviveAction), but on a casualty it is labelled
//! "Treat the patient", takes twice as long and ends here instead of in LF_Revive:
//! - the server checks duty, range, the medic's state and the first-aid item (LF_Revive.Check and the
//!   revive item of LF_MedicConfig), never the client;
//! - the patient wakes (LF_Downed.Clear), stands and leaves a little later;
//! - the call closes: the casualty leaves LF_CasualtyCalls.Open() and LF_OnServiceCallClosed takes its
//!   dispatch mark off the map;
//! - the medic is paid by publishing LF_OnPlayerRevived with the casualty's caller uid, so the existing
//!   LF_MedicDutyBonus subscriber books the $10 revive bonus (capped per hour by rank, paid at clock-out)
//!   and LF_JobLevelSources awards MEDIC XP.
//! Farming guards: a casualty pays once (it leaves the open list on the first treat, so a second treat
//! finds no casualty); a casualty the medic downed pays nothing; the rank's hourly bonus cap holds.
//! Server only, apart from LooksLikeCasualty, which the action reads on the client.
class LF_CasualtyTreat
{
	static const string FEATURE = "CasualtyTreat";
	//! A treat holds this many times the revive's Duration (4 s on the character prefab): about 8 s.
	static const float HOLD_FACTOR = 2.0; // lf-fixed: card #4828, "about 8 s"
	//! How long a treated resident stays on their feet before they leave.
	static const int LEAVE_MS = 20000; // lf-fixed: the walk-off delay, not a game rule

	protected static int s_iTreated;
	protected static int s_iUnpaid;
	//! Treated residents waiting to leave, oldest first (one CallLater per entry, all LEAVE_MS long).
	protected static ref array<EntityID> s_aLeaving;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Leave);
		s_iTreated = 0;
		s_iUnpaid = 0;
		s_aLeaving = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the open casualty whose body is `body`, or null.
	static LF_Casualty Find(IEntity body)
	{
		if (!body)
			return null;

		EntityID id = body.GetID();
		foreach (LF_Casualty casualty : LF_CasualtyCalls.Open())
		{
			if (casualty && casualty.m_Entity == id)
				return casualty;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: a downed character no player controls that the server flagged as a casualty.
	//! The client cannot read the server's casualty list, but the flag is an RplProp on the body, so this
	//! picks the action's label and hold time; Perform and Find decide on the server (#4828, #4848).
	static bool LooksLikeCasualty(IEntity entity)
	{
		if (!entity || !LF_Downed.IsDowned(entity))
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character || !character.LF_IsCasualty())
			return false;

		PlayerManager players = GetGame().GetPlayerManager();
		return players && players.GetPlayerIdFromControlledEntity(entity) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the action's entry. Reads the medic's duty from the account cache.
	static LF_EReviveRefusal Perform(IEntity medic, IEntity body)
	{
		bool onDuty;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && medic)
			onDuty = LF_MedicDuty.IsOnDuty(accounts.GetAccount(LF_Utils.GetPlayerUID(medic)));

		return Apply(medic, body, onDuty);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: treat the casualty `body` for `medic`, whose duty state the caller resolved.
	//! \return NONE when the casualty was treated, else why not (the medic gets the refusal toast).
	static LF_EReviveRefusal Apply(IEntity medic, IEntity body, bool medicOnDuty)
	{
		if (!Replication.IsServer())
			return LF_EReviveRefusal.NO_PATIENT;

		string medicUid = LF_Utils.GetPlayerUID(medic);
		LF_Casualty casualty = Find(body);
		if (!casualty)
		{
			LF_Debug.Info(FEATURE, string.Format("treat by %1 refused: not an open casualty", medicUid));
			LF_Utils.NotifyEntity(medic, LF_Revive.RefusalKey(LF_EReviveRefusal.NOT_DOWNED), "", 4.0);
			return LF_EReviveRefusal.NO_PATIENT;
		}

		LF_EReviveRefusal refusal = LF_Revive.Check(medic, body, medicOnDuty);
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool costs = !config.m_sReviveItemPrefab.IsEmpty() && config.m_iReviveItemCount > 0;
		if (refusal == LF_EReviveRefusal.NONE && costs && LF_InventoryUtils.GetAmount(medic, config.m_sReviveItemPrefab) < config.m_iReviveItemCount)
			refusal = LF_EReviveRefusal.NO_ITEM;

		if (refusal != LF_EReviveRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("treat of %1 by %2 refused: %3", casualty.m_sCallerUid, medicUid, typename.EnumToString(LF_EReviveRefusal, refusal)));
			LF_Utils.NotifyEntity(medic, LF_Revive.RefusalKey(refusal), "", 4.0);
			return refusal;
		}

		// Read before the wake clears it: who downed this resident.
		string downer;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (character)
			downer = character.LF_GetDownerUid();

		if (!LF_Downed.Clear(body))
			return LF_EReviveRefusal.NOT_DOWNED;

		if (costs)
			LF_InventoryUtils.RemoveAmount(medic, config.m_sReviveItemPrefab, config.m_iReviveItemCount);

		// The call is done: off the open list (so the casualty pays once) and off the dispatch map.
		Close(casualty);
		s_iTreated++;
		vector position = body.GetOrigin();
		bool pays = !medicUid.IsEmpty() && !LF_DutyBonus.SameAccount(medicUid, downer);
		if (pays)
		{
			LF_EventBus.Publish(LF_OnPlayerRevived.Create(casualty.m_sCallerUid, medicUid, position));
			LF_Debug.Info(FEATURE, string.Format("casualty %1 treated by medic %2; call closed, revive bonus and MEDIC xp booked", casualty.m_sCallerUid, medicUid));
		}
		else
		{
			s_iUnpaid++;
			LF_Debug.Warn(FEATURE, string.Format("casualty %1 treated by %2, who downed them: call closed, no bonus", casualty.m_sCallerUid, medicUid));
		}

		LF_Utils.NotifyEntity(medic, "#LF-CasualtyTreat_Done", "", 4.0);
		QueueLeave(body);
		return LF_EReviveRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a casualty a medic put in a clinic bed (#4849): the call is done like a treat. The call leaves
	//! the open list and the board and the dispatch mark go (LF_OnServiceCallClosed), and the woken resident
	//! leaves later like a treated one. The $15 delivery bonus is the bed's own (LF_OnPatientDelivered);
	//! nothing is paid here. \return True when `body` was an open casualty.
	static bool CloseDelivered(IEntity body)
	{
		LF_Casualty casualty = Find(body);
		if (!casualty)
			return false;

		Close(casualty);
		QueueLeave(body);
		LF_Debug.Info(FEATURE, string.Format("casualty %1 put in a bed; call closed", casualty.m_sCallerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the woken resident walks off LEAVE_MS from now.
	protected static void QueueLeave(IEntity body)
	{
		if (!body || !GetGame() || !GetGame().GetCallqueue())
			return;

		if (!s_aLeaving)
			s_aLeaving = {};
		s_aLeaving.Insert(body.GetID());
		GetGame().GetCallqueue().CallLater(Leave, LEAVE_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take a casualty off the open list and its mark off the dispatch map.
	protected static void Close(notnull LF_Casualty casualty)
	{
		// The flag goes with the call: the body is an ordinary resident from here (it walks off or lies in a bed).
		SCR_ChimeraCharacter character;
		if (GetGame() && GetGame().GetWorld())
			character = SCR_ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(casualty.m_Entity));
		if (character)
			character.LF_SetCasualty(false);

		array<ref LF_Casualty> open = LF_CasualtyCalls.Open();
		for (int i = open.Count() - 1; i >= 0; i--)
		{
			if (open[i] == casualty)
				open.Remove(i);
		}

		LF_EventBus.Publish(LF_OnServiceCallClosed.Create(LF_CasualtyCalls.SERVICE, casualty.m_sCallerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the treated resident leaves (the body is deleted), unless a medic is moving them.
	protected static void Leave()
	{
		if (!s_aLeaving || s_aLeaving.IsEmpty() || !GetGame() || !GetGame().GetWorld())
			return;

		EntityID id = s_aLeaving[0];
		s_aLeaving.RemoveOrdered(0);

		IEntity body = GetGame().GetWorld().FindEntityByID(id);
		if (!body)
			return;

		// In a seat or held: never delete a body out of a vehicle; try again later. (A resident cannot be
		// downed again: LF_Downed.IsEligible takes players only, so a second hit kills and the corpse goes.)
		if (body.GetParent() || LF_Carry.IsSeatedIn(body))
		{
			s_aLeaving.Insert(id);
			if (GetGame().GetCallqueue())
				GetGame().GetCallqueue().CallLater(Leave, LEAVE_MS, false);
			LF_Debug.Info(FEATURE, "treated resident is in a vehicle; leaving deferred");
			return;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(body);
		LF_Debug.Info(FEATURE, "treated resident walked off");
	}

	//------------------------------------------------------------------------------------------------
	static int GetUnpaidCount()
	{
		return s_iUnpaid;
	}
}

//------------------------------------------------------------------------------------------------
//! The casualty flag on the body, replicated so a client labels only real casualties (#4848). The server
//! sets it when the generator spawns the body (LF_CasualtyCalls) and clears it when the call closes.
modded class SCR_ChimeraCharacter
{
	[RplProp()]
	protected bool m_bLF_Casualty;

	//------------------------------------------------------------------------------------------------
	//! Server: flag this body as a medic-call casualty, or take the flag off.
	void LF_SetCasualty(bool casualty)
	{
		if (!Replication.IsServer() || m_bLF_Casualty == casualty)
			return;

		m_bLF_Casualty = casualty;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	bool LF_IsCasualty()
	{
		return m_bLF_Casualty;
	}
}

//------------------------------------------------------------------------------------------------
//! On a casualty the revive hold becomes "Treat the patient": its own label, twice the hold, and the
//! server ends it in LF_CasualtyTreat. Every other target keeps the revive.
modded class LF_ReviveAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!LF_CasualtyTreat.LooksLikeCasualty(GetOwner()))
			return false;

		outName = "#LF-CasualtyTreat_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override float GetActionProgressScript(float fProgress, float timeSlice)
	{
		if (LF_CasualtyTreat.LooksLikeCasualty(GetOwner()))
			timeSlice = timeSlice / LF_CasualtyTreat.HOLD_FACTOR;

		return super.GetActionProgressScript(fProgress, timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !LF_CasualtyTreat.Find(pOwnerEntity))
		{
			super.PerformAction(pOwnerEntity, pUserEntity);
			return;
		}

		if (LF_Systems.Refuses("Medic", "LF_ReviveAction"))
			return;

		LF_ReviveAnim.End(pUserEntity, "treat done");
		LF_CasualtyTreat.Perform(pUserEntity, pOwnerEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CasualtyTreat's statics (LF_StaticReset finds it by reflection).
class LF_CasualtyTreat_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CasualtyTreat";
	}

	override void Apply()
	{
		LF_CasualtyTreat.ResetStatics();
	}
}
