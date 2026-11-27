//------------------------------------------------------------------------------------------------
//! The two-layer jailbreak. A jail with an outer door (LF_JailCellComponent.m_sOuterDoorTargetId)
//! is broken in two charges on the existing LF_Breach path: the outer door first, then the cell door
//! (LF_Jail.DoorTargetId). A charge on the cells while the outer door is still shut is refused, and
//! so is a charge set by a prisoner who is serving (nobody breaches from inside) and any charge on
//! either door inside the cooldown after the cells last blew.
//!
//! When the cells blow, every prisoner whose body is in the cell is freed (LF_Jail.FreeInside): the
//! sentence ends where they stand and they are posted as escaped custody (LF_PrisonEscape.OnEscaped,
//! wanted). Whoever charged the cells already has the jailbreak offence from the arm
//! (OFFENCE_PRISON_BREACH, a crime catalogue row, loud alarm, dispatch with the door's location).
//! Each blown door seals itself again after LF_JailBreakConfig.ResealSeconds; police can repair it
//! sooner with the breach repair action.
//!
//! Hooks (each a line or two): LF_Breach.Arm (ArmRefusal, FuseFor, OffenceFor) and LF_Breach.Open
//! (OnOpened). A jail with no outer door, or the break switched off, is the plain single door of #235.
//!
//! Server authority: every decision is in LF_Breach.Arm/Open on the server. The door state reaches a
//! late joiner through LF_BreachTargetComponent's replicated open flag. The cooldown is session state.
class LF_JailBreak
{
	static const string FEATURE = "JailBreak";
	//! The crime catalogue row the cell charge reports (Configs/Crime/LF_CrimeCatalog.conf).
	static const string OFFENCE_PRISON_BREACH = "#LF-Offence_PrisonBreach";

	//! The LF_AlarmKind id of the jailbreak siren (Configs/Police/LF_Police.conf).
	static const string ALARM_KIND = "jail";

	static const string REFUSE_FROM_INSIDE = "#LF-JailBreak_FromInside";
	static const string REFUSE_OUTER_FIRST = "#LF-JailBreak_OuterFirst";
	static const string REFUSE_COOLDOWN = "#LF-JailBreak_Cooldown";

	//! Unix seconds the cells last blew, 0 when they have not this session.
	protected static int s_iLastBreakAt;
	//! Outer and cell door ids as every machine reads them from the placed cell (for the client offer).
	protected static string s_sOfferOuterId;
	protected static string s_sOfferCellId;

	//------------------------------------------------------------------------------------------------
	//! Two-layer mode: the break is on and the jail names an outer door and a cell door.
	static bool IsTwoLayer()
	{
		if (!LF_JailBreakConfig.Get().m_bBreakEnabled)
			return false;

		return !LF_Jail.OuterDoorTargetId().IsEmpty() && !LF_Jail.DoorTargetId().IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOuter(string targetId)
	{
		return IsTwoLayer() && LF_Jail.IsOuterDoorTarget(targetId);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsCell(string targetId)
	{
		return IsTwoLayer() && LF_Jail.IsDoorTarget(targetId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_Breach.Arm before the charge is taken: why this arm is refused, or "".
	//! \param now Unix seconds.
	static string ArmRefusal(string targetId, string playerUid, int now)
	{
		if (!IsOuter(targetId) && !IsCell(targetId))
			return "";

		if (LF_Jail.IsInCustody(playerUid))
			return REFUSE_FROM_INSIDE;

		int left = CooldownLeft(now);
		if (left > 0)
			return REFUSE_COOLDOWN;

		if (IsCell(targetId) && LF_Breach.IsLocked(LF_Jail.OuterDoorTargetId()))
			return REFUSE_OUTER_FIRST;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds before the jail doors may be charged again, 0 when they may.
	static int CooldownLeft(int now)
	{
		if (s_iLastBreakAt <= 0)
			return 0;

		int until = s_iLastBreakAt + LF_JailBreakConfig.Get().CooldownSeconds();
		if (now >= until)
			return 0;

		return until - now;
	}

	//------------------------------------------------------------------------------------------------
	//! The fuse a charge on this target burns: the config's for a jail layer, else `fallback`.
	static float FuseFor(string targetId, float fallback)
	{
		if (IsOuter(targetId))
			return LF_JailBreakConfig.Get().OuterFuseSeconds();

		if (IsCell(targetId))
			return LF_JailBreakConfig.Get().CellFuseSeconds();

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! The offence a charge on this target reports: the jailbreak row for the cells, else `fallback`.
	static string OffenceFor(string targetId, string fallback)
	{
		if (IsCell(targetId))
			return OFFENCE_PRISON_BREACH;

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! The alarm kind both jail layers sound: the jailbreak siren (LF_Police.conf kind "jail"), a prison klaxon no
	//! robbery kind shares. Any other target keeps `fallback`.
	static string AlarmKindFor(string targetId, string fallback)
	{
		if (IsOuter(targetId) || IsCell(targetId))
			return ALARM_KIND;

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Both jail layers ring the loud alarm.
	static bool IsLoud(string targetId, bool fallback)
	{
		if (IsOuter(targetId) || IsCell(targetId))
			return true;

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_Breach.Open. The outer door: its reseal is scheduled. The cells: every prisoner
	//! inside is freed and wanted, the cooldown starts and both doors reseal later.
	//! \return Prisoners freed (0 for the outer door or any other target).
	static int OnOpened(string targetId, string armedBy, int now)
	{
		if (IsOuter(targetId))
		{
			LF_Debug.Warn(FEATURE, string.Format("outer door %1 blown by %2: the cells can be charged", targetId, armedBy));
			ScheduleReseal(true);
			return 0;
		}

		if (!IsCell(targetId))
			return 0;

		s_iLastBreakAt = now;
		array<string> freed = {};
		array<IEntity> bodies = {};
		LF_Jail.FreeInside(freed, bodies);
		foreach (int i, string uid : freed)
			LF_PrisonEscape.OnEscaped(uid, bodies[i]);

		if (!armedBy.IsEmpty())
			LF_PrisonEscape.OnOpened(armedBy);

		LF_Debug.Warn(FEATURE, string.Format("cells %1 blown by %2: %3 prisoner(s) freed, cooldown %4 s", targetId, armedBy, freed.Count(), LF_JailBreakConfig.Get().CooldownSeconds()));
		ScheduleReseal(false);
		return freed.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleReseal(bool outerOnly)
	{
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		int ms = LF_JailBreakConfig.Get().ResealSeconds() * 1000;
		ScriptCallQueue queue = GetGame().GetCallqueue();
		queue.Remove(ResealOuter);
		queue.CallLater(ResealOuter, ms, false);
		if (outerOnly)
			return;

		queue.Remove(ResealCell);
		queue.CallLater(ResealCell, ms, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The scheduled reseal of the outer door. A door police already repaired stays as it is.
	static void ResealOuter()
	{
		Reseal(LF_Jail.OuterDoorTargetId());
	}

	//------------------------------------------------------------------------------------------------
	static void ResealCell()
	{
		Reseal(LF_Jail.DoorTargetId());
	}

	//------------------------------------------------------------------------------------------------
	//! Locks a blown jail door again. \return True when it was open and is locked now.
	static bool Reseal(string targetId)
	{
		if (targetId.IsEmpty() || !LF_Breach.IsOpen(targetId))
			return false;

		LF_Breach.Reset(targetId);
		LF_Debug.Info(FEATURE, string.Format("jail door %1 sealed itself again", targetId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine, from LF_JailCellComponent.OnPostInit: the door ids the client offer reads.
	static void NoteOfferDoors(string cellId, string outerId)
	{
		s_sOfferCellId = cellId;
		s_sOfferOuterId = outerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Client offer of the breach arm action: the cells show "blow the outer door first" while the
	//! outer door's replicated flag says it is shut. The server decides again in ArmRefusal.
	static string OfferRefusal(string targetId)
	{
		// The server (a listen host too) knows the doors: the same rule Arm applies.
		if (Replication.IsServer())
		{
			if (IsCell(targetId) && LF_Breach.IsLocked(LF_Jail.OuterDoorTargetId()))
				return REFUSE_OUTER_FIRST;

			return "";
		}

		if (targetId.IsEmpty() || s_sOfferOuterId.IsEmpty() || targetId != s_sOfferCellId)
			return "";

		if (!LF_JailBreakConfig.Get().m_bBreakEnabled)
			return "";

		IEntity outer = LF_Places.Find(LF_Breach.PLACE_KIND, s_sOfferOuterId);
		if (!outer)
			return "";

		LF_BreachTargetComponent component = LF_Component<LF_BreachTargetComponent>.Find(outer);
		if (component && !component.IsOpenForOffer())
			return REFUSE_OUTER_FIRST;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: the clock and any scheduled reseal, leaving the placed cell's offer ids alone.
	static void ResetForTest()
	{
		s_iLastBreakAt = 0;
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(ResealOuter);
			GetGame().GetCallqueue().Remove(ResealCell);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iLastBreakAt = 0;
		s_sOfferOuterId = "";
		s_sOfferCellId = "";
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(ResealOuter);
			GetGame().GetCallqueue().Remove(ResealCell);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_JailBreak's statics (LF_StaticReset finds it by reflection).
class LF_JailBreak_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_JailBreak";
	}

	override void Apply()
	{
		LF_JailBreak.ResetStatics();
	}
}
