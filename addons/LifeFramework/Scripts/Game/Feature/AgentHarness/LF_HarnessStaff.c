//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the staff side of the police application (#1450, agent-box/proofs/
//! staff-approve.mjs). Three things a session needs and a player has through the game master:
//!   staffapprove <uid>   (server, behind the harness gate) approves the open application of that
//!                        account through LF_PoliceApplicationStaff.ApproveByUid. The caller is the
//!                        sender's own account and AdminChatChannel.IsAdminPlayer for it, exactly as
//!                        the GM panel will pass them, so the staff check still decides: a client that
//!                        is not an administrator (and not on staff.txt) is refused.
//!   harness-admins.txt   `session.mjs up --admin-client N` seeds $profile:LifeFramework/harness-admins.txt
//!                        with that client's session identity. A harness build (-scrDefine LF_HARNESS,
//!                        sessions only) gives a joining player whose account is listed the engine's
//!                        SESSION_ADMINISTRATOR role, and AdminChatChannel.IsAdminPlayer counts that
//!                        role in a harness build (and only there). A `-server` boot has no server.json
//!                        to put game.admins in (-config is fatal with -addons) and the engine refuses a
//!                        script grant of ADMINISTRATOR, so this is the one way a session gets an admin.
//!   harness-flags.txt    `up --no-instant-approval` and `up --skip-playtime-gate` seed one flag per
//!                        line. The first runs the application form's gated path on a mission whose
//!                        header approves on submit; the second sets the minimum playtime to 0 for
//!                        the form, so a fresh test account passes it. Both are read only in a
//!                        harness build. The gameplay defaults (LF_Police.conf, the mission header)
//!                        are never changed.
//! None of this exists in a build without LF_HARNESS: every entry point checks LF_DevGate.HarnessBuild().
//------------------------------------------------------------------------------------------------
class LF_HarnessStaff
{
	static const string FEATURE = "AgentHarness";
	static const string ADMINS_FILE = "$profile:LifeFramework/harness-admins.txt";
	static const string FLAGS_FILE = "$profile:LifeFramework/harness-flags.txt";
	static const string FLAG_NO_INSTANT_APPROVAL = "no-instant-approval";
	static const string FLAG_SKIP_PLAYTIME_GATE = "skip-playtime-gate";

	//------------------------------------------------------------------------------------------------
	static bool IsVerb(string tool)
	{
		return tool.StartsWith("staffapprove ");
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether one trimmed, non-comment line of text equals token. `#` starts a comment.
	static bool ListHas(string text, string token)
	{
		if (token.IsEmpty())
			return false;

		array<string> lines = {};
		text.Split("\n", lines, false);
		foreach (string raw : lines)
		{
			string line = raw;
			int hash = line.IndexOf("#");
			if (hash >= 0)
				line = line.Substring(0, hash);

			line.Replace("\r", "");
			line = line.Trim();
			if (line == token)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a joining player should get the ADMINISTRATOR role.
	static bool ShouldGrant(string accountUid, string adminsText, bool alreadyAdmin)
	{
		if (alreadyAdmin || accountUid.IsEmpty())
			return false;

		return ListHas(adminsText, accountUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReadFile(string path)
	{
		if (!FileIO.FileExists(path))
			return "";

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, "cannot open the harness seed file: " + path);
			return "";
		}

		string text = "";
		string line;
		while (handle.ReadLine(line) >= 0)
			text = text + line + "\n";

		handle.Close();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string AdminsText()
	{
		return ReadFile(ADMINS_FILE);
	}

	//------------------------------------------------------------------------------------------------
	protected static string FlagsText()
	{
		return ReadFile(FLAGS_FILE);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a session flag is on in the seeded flags text. Never outside a harness build.
	static bool FlagIn(string flagsText, string flag, bool harnessBuild)
	{
		if (!harnessBuild)
			return false;

		return ListHas(flagsText, flag);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether the mission header's approve-on-submit applies. A session started with
	//! --no-instant-approval runs the gated path and waits for staff instead.
	static bool InstantApprovalFrom(bool headerFlag, string flagsText, bool harnessBuild)
	{
		if (!headerFlag)
			return false;

		return !FlagIn(flagsText, FLAG_NO_INSTANT_APPROVAL, harnessBuild);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the minimum playtime hours the form checks. A session started with --skip-playtime-gate
	//! checks none, so a fresh test account passes it.
	static int MinPlaytimeFrom(int configured, string flagsText, bool harnessBuild)
	{
		if (FlagIn(flagsText, FLAG_SKIP_PLAYTIME_GATE, harnessBuild))
			return 0;

		return configured;
	}

	//------------------------------------------------------------------------------------------------
	//! The police application form: approve on submit, unless a harness session turned it off.
	static bool InstantApproval(bool headerFlag)
	{
		if (!headerFlag)
			return false;

		bool instant = InstantApprovalFrom(headerFlag, FlagsText(), LF_DevGate.HarnessBuild());
		if (!instant)
			LF_Debug.Info(FEATURE, "no-instant-approval: the application waits for staff");

		return instant;
	}

	//------------------------------------------------------------------------------------------------
	//! The police application form: the minimum playtime hours it checks.
	static int ApplyMinPlaytimeHours(int configured)
	{
		return MinPlaytimeFrom(configured, FlagsText(), LF_DevGate.HarnessBuild());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives a connected player the engine ADMINISTRATOR role when the session seeded their
	//! account in harness-admins.txt. Idempotent; called on connect and again on spawn, because the
	//! account may not have resolved at the first call. A no-op outside a harness build.
	static void GrantAdminIfListed(int playerId)
	{
		if (!LF_DevGate.HarnessBuild() || !Replication.IsServer() || playerId <= 0)
			return;

		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return;

		bool already = AdminChatChannel.IsAdminPlayer(playerId);
		string uid = LF_Utils.GetAccountUID(playerId);
		if (!ShouldGrant(uid, AdminsText(), already))
			return;

		// The engine refuses a script grant of ADMINISTRATOR (checked 2026-11-15, build 26266485); the session
		// administrator role is grantable and AdminChatChannel.IsAdminPlayer counts it in a harness build.
		if (manager.GivePlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR))
			LF_Debug.Info(FEATURE, string.Format("harness admin: player %1 (%2) is a harness administrator", playerId, uid));
		else
			LF_Debug.Error(FEATURE, string.Format("harness admin: GivePlayerRole refused for player %1 (%2)", playerId, uid));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the staff approve as a harness reply: "ok approved <uid>" or "error <localization key>".
	//! \param callerUid The sender's account uid (the server resolves it, never the payload).
	//! \param engineAdmin The engine ADMINISTRATOR role of the sender.
	static string Decide(string callerUid, bool engineAdmin, string targetUid)
	{
		string refusal = LF_PoliceApplicationStaff.ApproveByUid(callerUid, engineAdmin, targetUid);
		if (!refusal.IsEmpty())
			return "error " + refusal;

		return "ok approved " + targetUid;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the `staffapprove <uid>` verb for this character's player.
	//! \param tool "staffapprove <uid>"
	//! \return "ok ..." or "error <reason>"
	static string Run(IEntity character, string tool)
	{
		array<string> parts = {};
		tool.Split(" ", parts, true);
		if (parts.Count() != 2)
			return "error usage_staffapprove_uid";

		if (!character)
			return "error no_character";

		PlayerManager manager = GetGame().GetPlayerManager();
		int playerId = 0;
		if (manager)
			playerId = manager.GetPlayerIdFromControlledEntity(character);

		if (playerId <= 0)
			return "error no_player";

		string callerUid = LF_Utils.GetAccountUID(playerId);
		bool engineAdmin = AdminChatChannel.IsAdminPlayer(playerId);
		string reply = Decide(callerUid, engineAdmin, parts[1]);
		LF_Debug.Info(FEATURE, string.Format("staffapprove %1 by player %2 admin=%3 -> %4", parts[1], playerId, engineAdmin, reply));
		return reply;
	}
}
