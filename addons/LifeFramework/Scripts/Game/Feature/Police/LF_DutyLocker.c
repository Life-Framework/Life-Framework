//------------------------------------------------------------------------------------------------
//! Police gear is a second inventory (docs/decisions.md). The duty desk swaps the officer's whole
//! carried inventory, clothes included, on the server in one call:
//!   clock in  : carried -> SLOT_CIVILIAN, then SLOT_POLICE (or the starter police kit) -> carried
//!   clock out : carried -> SLOT_POLICE,   then SLOT_CIVILIAN (or the civilian kit)     -> carried
//! Both slots are LF_StorageSnapshot records keyed by the character persistence id, so they ride
//! the carried payload across a restart. After a swap the stored side holds exactly one snapshot
//! and the other side is live on the body: the slot just applied is cleared, so neither inventory
//! exists twice and neither is ever missing. The carried snapshot is refreshed at once.
//!
//! Death on duty ends the shift: the police gear stays on the body (death drops what you carry),
//! the stored civilian inventory becomes the carried snapshot the respawn restores, and the next
//! clock-in issues the starter police kit again. A disconnect on duty keeps the officer on duty
//! with the police gear carried; the civilian stash waits for clock-out.
//------------------------------------------------------------------------------------------------
class LF_DutyLocker
{
	static const string FEATURE = "Duty";
	static const string SLOT_POLICE = "police_duty";
	static const string SLOT_CIVILIAN = "civilian_stash";

	//------------------------------------------------------------------------------------------------
	//! Server rule for Clock in. "" accepts; otherwise the key shown to the player.
	static string RefuseClockIn(EL_PlayerAccount account)
	{
		if (!account || !account.GetActiveCharacter())
			return "#LF-Duty_NoAccount";

		if (account.IsOnDuty())
			return "#LF-Duty_AlreadyOn";

		if (!EL_WhitelistManager.MayJoinFaction(account.GetPersistentId(), EL_EFactionType.POLICE))
			return "#LF-Duty_NotOnRoster";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server rule for Clock out.
	static string RefuseClockOut(EL_PlayerAccount account)
	{
		if (!account || !account.GetActiveCharacter())
			return "#LF-Duty_NoAccount";

		if (!account.IsOnDuty())
			return "#LF-Duty_NotOn";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: clock the officer in. Stores the civilian inventory and dresses them in the police
	//! inventory from their last clock-out, or the starter police kit on a first shift.
	//! \return "" on success, or the refusal key.
	static string ClockIn(EL_PlayerAccount account, IEntity officer)
	{
		string refusal = RefuseClockIn(account);
		if (!refusal.IsEmpty())
		{
			EL_Debug.Warn(FEATURE, string.Format("clock-in refused for %1: %2", AccountId(account), refusal));
			return refusal;
		}

		if (!officer || !LF_StorageSnapshot.IsAlive(officer))
		{
			EL_Debug.Error(FEATURE, string.Format("clock-in refused for %1: no living body", AccountId(account)));
			return "#LF-Duty_NoAccount";
		}

		string characterId = account.GetActiveCharacter().GetId();
		LF_StorageSnapshotRecord civilian = LF_StorageSnapshot.Capture(characterId, SLOT_CIVILIAN, officer);
		if (!civilian)
			return "#LF-Duty_NoAccount";

		LF_StorageSnapshotRecord police = LF_StorageSnapshot.Get(characterId, SLOT_POLICE);
		LF_StorageSnapshot.Set(civilian);
		bool starter = !police;
		if (police)
		{
			LF_StorageSnapshot.Apply(officer, police);
			LF_StorageSnapshot.Clear(characterId, SLOT_POLICE);
		}
		else
		{
			DressInKit(officer, EL_Faction.POLICE);
		}

		account.SetFaction(EL_Faction.POLICE);
		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChanged(account, true);
		AfterSwap(account, officer);

		EL_Debug.Info(FEATURE, string.Format("clocked in %1: civilian inventory stored (%2 items), police %3", AccountId(account), civilian.m_aItems.Count(), StarterText(starter)));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: clock the officer out. Pays the shift, stores the police inventory, and hands the
	//! civilian inventory back.
	//! \return "" on success, or the refusal key.
	static string ClockOut(EL_PlayerAccount account, IEntity officer)
	{
		string refusal = RefuseClockOut(account);
		if (!refusal.IsEmpty())
		{
			EL_Debug.Warn(FEATURE, string.Format("clock-out refused for %1: %2", AccountId(account), refusal));
			return refusal;
		}

		if (!officer || !LF_StorageSnapshot.IsAlive(officer))
		{
			EL_Debug.Error(FEATURE, string.Format("clock-out refused for %1: no living body", AccountId(account)));
			return "#LF-Duty_NoAccount";
		}

		string characterId = account.GetActiveCharacter().GetId();
		LF_StorageSnapshotRecord police = LF_StorageSnapshot.Capture(characterId, SLOT_POLICE, officer);
		if (!police)
			return "#LF-Duty_NoAccount";

		LF_StorageSnapshotRecord civilian = LF_StorageSnapshot.Get(characterId, SLOT_CIVILIAN);
		LF_StorageSnapshot.Set(police);
		if (civilian)
		{
			LF_StorageSnapshot.Apply(officer, civilian);
			LF_StorageSnapshot.Clear(characterId, SLOT_CIVILIAN);
		}
		else
		{
			// A shift that started before the second inventory existed: plain civilian clothes.
			EL_Debug.Warn(FEATURE, string.Format("clock-out for %1 found no stored civilian inventory; issuing the civilian kit", AccountId(account)));
			DressInKit(officer, EL_Faction.CIVILIAN);
		}

		// Duty pay settles while the account is still police (the clock and the pay check it).
		LF_PoliceRanks.OnDutyChanged(account, false);
		LF_DutyPay.PayAtClockOut(account, officer);

		account.SetOnDuty(false);
		account.SetFaction(EL_Faction.CIVILIAN);
		AfterSwap(account, officer);

		EL_Debug.Info(FEATURE, string.Format("clocked out %1: police inventory stored (%2 items)", AccountId(account), police.m_aItems.Count()));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the death hook (after the carried snapshot was dropped): ends a shift. The body
	//! keeps the police gear; the stored civilian inventory becomes what the respawn carries.
	//! \return True when a shift was ended.
	static bool OnDied(EL_PlayerAccount account)
	{
		if (!account || !account.IsOnDuty() || !account.GetActiveCharacter())
			return false;

		string characterId = account.GetActiveCharacter().GetId();
		LF_StorageSnapshotRecord civilian = LF_StorageSnapshot.Get(characterId, SLOT_CIVILIAN);
		if (civilian)
		{
			LF_StorageSnapshotRecord carried = LF_StorageSnapshotRecord.Create(characterId, LF_StorageSnapshot.SLOT_CARRIED);
			carried.m_aItems = civilian.m_aItems;
			LF_StorageSnapshot.Set(carried);
			LF_StorageSnapshot.Clear(characterId, SLOT_CIVILIAN);
		}

		// The police inventory was on the body; nothing of it is stored, so the next shift starts
		// from the starter kit. A stale stored copy would be a second set next to the lootable one.
		LF_StorageSnapshot.Clear(characterId, SLOT_POLICE);

		LF_PoliceRanks.OnDutyChanged(account, false);
		account.SetOnDuty(false);
		account.SetFaction(EL_Faction.CIVILIAN);
		EL_Debug.Info(FEATURE, string.Format("shift ended by death for %1: civilian inventory returns at respawn=%2", AccountId(account), civilian != null));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the body (clothes included), then issues the role's configured kit.
	protected static void DressInKit(IEntity officer, EL_Faction role)
	{
		LF_StorageSnapshot.Apply(officer, LF_StorageSnapshotRecord.Create("kit", "empty"));
		EL_SpawnLogic spawnLogic = EL_SpawnLogic.GetInstance();
		if (!spawnLogic || !spawnLogic.GiveRoleKit(officer, role))
			EL_Debug.Error(FEATURE, string.Format("no %1 kit issued: the spawn logic has none configured", typename.EnumToString(EL_Faction, role)));
	}

	//------------------------------------------------------------------------------------------------
	//! The new carried tree is the truth now: refresh its snapshot, reprint the ID card a restored
	//! tree carries blank, and save the account.
	protected static void AfterSwap(EL_PlayerAccount account, IEntity officer)
	{
		EL_PlayerCharacter identity = account.GetActiveCharacter();
		LF_StorageSnapshot.CaptureCarried(identity.GetId(), officer);

		array<LF_IdCardComponent> cards = {};
		LF_IdCard.FindCarried(officer, cards);
		if (cards.Count() > 0)
			LF_IdCard.Grant(officer, identity.GetId(), identity.GetFirstName(), identity.GetLastName(), identity.GetAge());

		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (accounts)
			accounts.SaveAndReleaseAccount(account);
	}

	//------------------------------------------------------------------------------------------------
	protected static string AccountId(EL_PlayerAccount account)
	{
		if (!account)
			return "<no account>";

		return account.GetPersistentId();
	}

	//------------------------------------------------------------------------------------------------
	protected static string StarterText(bool starter)
	{
		if (starter)
			return "starter kit issued";

		return "inventory restored";
	}
}
