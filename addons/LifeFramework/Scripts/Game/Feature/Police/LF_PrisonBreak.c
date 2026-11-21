//------------------------------------------------------------------------------------------------
//! The break (#3421, #798). A free member of an organisation that has a member inside forces the
//! jail's outer door (the existing LF_Breach, the fence charge): when the rules (LF_PrisonBreakRules,
//! #3420) allow it, a window opens, dispatch radios every officer, and the door stays open for it.
//! A prisoner who walks out while the window is open is `escaped`: still serving, the remaining time
//! kept in the jail payload, no longer pulled back to the cell (LF_Jail.Confines). At the end of the
//! window the door locks again (LF_Breach.Reset); whoever stayed in serves on.
//!
//! Hooks (each a few lines): LF_Breach.Arm (OnArmed), LF_Breach.Open (OnOpened), LF_Jail.Confines
//! (IsEscaped) and LF_Jail.NoteEscape (OnWalkedOut). A breach the rules refuse (breaks off, nobody
//! inside, too few officers, the cooldown) is the plain break-in it always was: no window, no escape.
//!
//! PERSISTENCE. The escaped list is the "prisonbreak" save section (LF_PrisonBreakSavePayload): the
//! character record key and the seconds left when they walked out. The sentence itself is the jail
//! payload. The window is session state: it is not saved.
class LF_PrisonBreakRow : Managed
{
	string m_sUid;
	float m_fRemaining;

	//------------------------------------------------------------------------------------------------
	static LF_PrisonBreakRow Create(string uid, float remaining)
	{
		LF_PrisonBreakRow row = new LF_PrisonBreakRow();
		row.m_sUid = uid;
		row.m_fRemaining = remaining;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PrisonBreak
{
	static const string FEATURE = "PrisonBreak";	// lf-fixed: the log tag, same as LF_PrisonBreakRules.FEATURE

	//! Character key to the seconds left when they walked out of an open door in the window.
	protected static ref map<string, float> s_mEscaped;
	//! Jail door target id to the key of whoever armed it, and their body, until it opens.
	protected static ref map<string, string> s_mStarters;
	protected static ref map<string, EntityID> s_mStarterBodies;
	//! The jail door a running window holds open, "" when no window runs.
	protected static string s_sWindowDoor;

	//------------------------------------------------------------------------------------------------
	protected static map<string, float> Escaped()
	{
		if (!s_mEscaped)
			s_mEscaped = new map<string, float>();

		return s_mEscaped;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, string> Starters()
	{
		if (!s_mStarters)
			s_mStarters = new map<string, string>();

		return s_mStarters;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, EntityID> StarterBodies()
	{
		if (!s_mStarterBodies)
			s_mStarterBodies = new map<string, EntityID>();

		return s_mStarterBodies;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_Breach.Arm: remembers who set the charge on the jail door. Any other target
	//! is ignored.
	static void OnArmed(string targetId, string playerUid, IEntity body)
	{
		if (!LF_Jail.IsDoorTarget(targetId) || playerUid.IsEmpty())
			return;

		Starters().Set(targetId, playerUid);
		if (body)
			StarterBodies().Set(targetId, body.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_Breach.Open: the jail door has just been forced. Gathers the facts and asks the
	//! rules; a refusal leaves the plain break-in.
	static void OnOpened(string targetId)
	{
		if (!LF_Jail.IsDoorTarget(targetId))
			return;

		string starterUid;
		Starters().Find(targetId, starterUid);
		EntityID bodyId = EntityID.INVALID;
		StarterBodies().Find(targetId, bodyId);
		Starters().Remove(targetId);
		StarterBodies().Remove(targetId);
		if (starterUid.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("door %1 opened with no known starter: no break", targetId));
			return;
		}

		IEntity body;
		if (bodyId != EntityID.INVALID && GetGame() && GetGame().GetWorld())
			body = GetGame().GetWorld().FindEntityByID(bodyId);

		Start(starterUid, StarterIsFree(starterUid, body), LF_AccountReplication.CountPoliceOnDuty(), System.GetUnixTime(), body);
	}

	//------------------------------------------------------------------------------------------------
	//! Free: not serving a sentence of their own and not in restraints.
	protected static bool StarterIsFree(string starterUid, IEntity body)
	{
		if (LF_Jail.IsServing(starterUid))
			return false;

		if (body && LF_Restraint.IsRestrained(body))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Members of the organisation of `uid` who are serving a sentence, and whether `uid` is a member.
	static int MembersInside(string uid, out bool isMember)
	{
		isMember = false;
		LF_Organization org = LF_OrganizationManager.GetByCharacter(uid);
		if (!org)
			return 0;

		isMember = true;
		array<string> ids = {};
		org.GetMemberIds(ids);
		int inside = 0;
		foreach (string id : ids)
		{
			if (id != uid && LF_Jail.IsServing(id))
				inside++;
		}

		return inside;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the rules for a break with the facts given. A good start opens the window on the jail door,
	//! radios every officer (a jailbreak report) and schedules the close.
	//! \return True when the window opened.
	static bool Start(string starterUid, bool starterFree, int officersOnDuty, int now, IEntity body = null)
	{
		bool isMember;
		int inside = MembersInside(starterUid, isMember);
		string refusal;
		if (!LF_PrisonBreakRules.TryStart(LF_PrisonBreakConfig.Get(), starterFree, isMember, inside, officersOnDuty, now, refusal))
			return false;

		s_sWindowDoor = LF_Jail.DoorTargetId();
		LF_PrisonEscape.OnOpened(starterUid);
		LF_Dispatch.Report(starterUid, body, LF_WantedReports.OFFENCE_JAILBREAK, vector.Zero, officersOnDuty);

		LF_PrisonBreakConfig config = LF_PrisonBreakConfig.Get();
		if (config && GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(EndWindow);
			GetGame().GetCallqueue().CallLater(EndWindow, config.WindowSeconds() * 1000, false);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The scheduled end of the window.
	static void EndWindow()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the window early or at its end: the jail door locks again. Whoever is outside is
	//! already escaped; whoever is inside is confined again. Safe to call twice.
	static void Close()
	{
		if (s_sWindowDoor.IsEmpty())
			return;

		string doorId = s_sWindowDoor;
		s_sWindowDoor = "";
		LF_PrisonBreakRules.CloseWindow();
		LF_Breach.Reset(doorId);
		LF_Debug.Info(FEATURE, string.Format("break window ended: door %1 locked again", doorId));
	}

	//------------------------------------------------------------------------------------------------
	//! The window check for a caller with its own clock (tests, a poll): closes a window that is over.
	//! \return True while the window is still open at `now`.
	static bool Tick(int now)
	{
		if (s_sWindowDoor.IsEmpty())
			return false;

		if (!LF_PrisonBreakRules.IsWindowOpen(now))
		{
			Close();
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a break window is open at `now`.
	static bool IsWindowOpen(int now)
	{
		return !s_sWindowDoor.IsEmpty() && LF_PrisonBreakRules.IsWindowOpen(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_Jail.NoteEscape: a serving prisoner stands outside the cell with the door open.
	//! In the window that is an escape: the status is set, the remaining time kept.
	//! \return True when this call set the status.
	static bool OnWalkedOut(string playerUid, float remaining, int now)
	{
		if (playerUid.IsEmpty() || !IsWindowOpen(now))
			return false;

		if (Escaped().Contains(playerUid))
			return false;

		Escaped().Set(playerUid, remaining);
		LF_Debug.Warn(FEATURE, string.Format("%1 walked out in the break window remaining=%2: status escaped", playerUid, remaining));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The prisoner has the escaped status and still has a sentence to serve.
	static bool IsEscaped(string playerUid)
	{
		if (!s_mEscaped || !s_mEscaped.Contains(playerUid))
			return false;

		return LF_Jail.IsServing(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds left when they walked out, 0 when they are not escaped.
	static float EscapedRemaining(string playerUid)
	{
		if (!IsEscaped(playerUid))
			return 0;

		return s_mEscaped.Get(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the status off (a recapture back in the cell).
	static void ClearEscaped(string playerUid)
	{
		if (s_mEscaped && s_mEscaped.Contains(playerUid))
			s_mEscaped.Remove(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Save seam: one row per escaped prisoner who still serves.
	static array<ref LF_PrisonBreakRow> ExportAll()
	{
		array<ref LF_PrisonBreakRow> rows = new array<ref LF_PrisonBreakRow>();
		if (!s_mEscaped)
			return rows;

		foreach (string uid, float remaining : s_mEscaped)
		{
			if (LF_Jail.IsServing(uid))
				rows.Insert(LF_PrisonBreakRow.Create(uid, remaining));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Save seam: replaces the list. A row with no key is skipped.
	//! \return Rows kept.
	static int ApplyAll(notnull array<ref LF_PrisonBreakRow> rows)
	{
		Escaped().Clear();
		int kept = 0;
		foreach (LF_PrisonBreakRow row : rows)
		{
			if (!row || row.m_sUid.IsEmpty())
				continue;

			Escaped().Set(row.m_sUid, row.m_fRemaining);
			kept++;
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mEscaped = null;
		s_mStarters = null;
		s_mStarterBodies = null;
		s_sWindowDoor = "";
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(EndWindow);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PrisonBreak's statics (LF_StaticReset finds it by reflection).
class LF_PrisonBreak_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PrisonBreak";
	}

	override void Apply()
	{
		LF_PrisonBreak.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "prisonbreak" (#3421): who walked out in a break window and is still serving.
class LF_PrisonBreakSavePayload : LF_SavePayload
{
	static const string NAME = "prisonbreak";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_PrisonBreakRow> rows = LF_PrisonBreak.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 escaped prisoner row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_PrisonBreakRow> rows = new array<ref LF_PrisonBreakRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_PrisonBreak.FEATURE, "escaped prisoners failed to read; the list stays as it was");
			return false;
		}

		int kept = LF_PrisonBreak.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 escaped prisoner row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the section: nobody has escaped, which is where a boot starts.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_PrisonBreak.FEATURE, "save has no escaped prisoners");
	}
}
