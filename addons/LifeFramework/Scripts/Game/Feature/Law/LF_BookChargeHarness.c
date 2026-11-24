//------------------------------------------------------------------------------------------------
//! Harness stand-in for a fineable charge (#4699 proof). The island's real crimes a session can commit
//! (the fuel till robbery) are jail only, so their charge sheet never offers "Pay $X now", and the
//! licence seizure of #4662 needs a mapped code. A proof that walks the booking desk files one on the
//! sending player instead:
//!   bookcharge <weapontheft|vehicletheft|shoplifting|robbery>   (server, behind the harness gate) files
//!       one open crime report of that offence against the sending player and makes them wanted (level
//!       1 at least), the state a witnessed crime leaves; replies
//!       `ok bookcharge offence=<id> reports=<n> wanted=<w>`
//! The desk's own rules do the rest (LF_ChargeSheetDesk reads the open reports). HARNESS ONLY: Run refuses
//! in any build without -scrDefine LF_HARNESS, on top of the gate the RPC already passed.
//------------------------------------------------------------------------------------------------
class LF_BookChargeHarness
{
	static const string FEATURE = "AgentHarness";
	static const string VERB = "bookcharge";

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a harness tool string is this verb.
	static bool IsVerb(string tool)
	{
		return tool.StartsWith(VERB + " ");
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the offence id a short word names, or "" for a word this verb does not know.
	static string OffenceFor(string word)
	{
		if (word == "weapontheft")
			return LF_WantedReports.OFFENCE_WEAPON_THEFT;

		if (word == "vehicletheft")
			return LF_WantedReports.OFFENCE_VEHICLE_THEFT;

		if (word == "shoplifting")
			return LF_WantedReports.OFFENCE_SHOPLIFTING;

		if (word == "robbery")
			return LF_WantedReports.OFFENCE_ROBBERY;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: file the report and raise the wanted level for one account. Not gated by the build: the
	//! gate is Run, the one entry a client can reach.
	//! \return The report count after the filing, or -1 when the account or offence is unusable.
	static int File(string uid, string offence)
	{
		if (uid.IsEmpty() || offence.IsEmpty())
			return -1;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts)
			account = accounts.GetAccount(uid);

		if (!account)
			return -1;

		LF_WantedReports.Add(uid, offence, string.Empty, string.Empty, string.Empty, true);
		if (account.GetWantedLevel() < 1)
			account.SetWantedLevel(1);

		return LF_WantedReports.CountFor(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the `bookcharge` verb for this character's player.
	//! \return "ok ..." or "error <reason>"
	static string Run(IEntity character, string tool)
	{
		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "bookcharge refused: not a LF_HARNESS build");
			return "error harness_only";
		}

		array<string> words = {};
		tool.Split(" ", words, true);
		if (words.Count() < 2)
			return "error usage_bookcharge_offence";

		string offence = OffenceFor(words[1]);
		if (offence.IsEmpty())
			return "error unknown_offence " + words[1];

		if (!character)
			return "error no_character";

		string uid = LF_Utils.GetPlayerUid(character);
		int reports = File(uid, offence);
		if (reports < 0)
			return "error no_account";

		int wanted;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && accounts.GetAccount(uid))
			wanted = accounts.GetAccount(uid).GetWantedLevel();

		LF_Debug.Info(FEATURE, string.Format("bookcharge: %1 charged with %2 (%3 open report(s), wanted %4)", uid, offence, reports, wanted));
		return string.Format("ok bookcharge offence=%1 reports=%2 wanted=%3", offence, reports, wanted);
	}
}

//------------------------------------------------------------------------------------------------
//! Client side of `bookcharge`: hands the word to the sending character's server request.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != LF_BookChargeHarness.VERB)
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		character.LF_AskHarnessScanner(cmd.Id, LF_BookChargeHarness.VERB + " " + cmd.Arg1);
		return true;
	}
}
