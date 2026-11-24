// Harness verb `checkpoint <hour>` (#4825): puts a fresh character at the state the 0.3.0 plan gives that hour of
// the 10-hour arc (docs/roadmap/next-release-0.3.0.md section 1): exact cash and bank, the trade levels, the
// licences, police and medic rank and underworld standing. The X-5 and X-6 full runs start each hour from one,
// so ten hours of play take about five hours of agent time.
//
// One JSON per hour, 0 to 10, in agent-box/checkpoints/hour-<N>.json. The server reads it from its profile:
// `session up --seed agent-box/checkpoints/hour-7.json` copies it to $profile:LifeFramework/hour-7.json (the
// test runner mirrors the folder to $profile:LifeFramework/checkpoints/). Keys: cash, bank (exact amounts),
// policeRank, medicRank (rank indexes), levels {JOB: level}, standing {FACTION: points}, licences ("A,B").
// A key that is absent leaves that part alone.
//
// A harness build only: the verb and its RPC are compiled out without -scrDefine LF_HARNESS, so a real server
// has neither, and nothing here touches a real player's save path (the values go in through the same public
// mutators play uses). The statics stay in every build because the suite (built without the define) tests
// them; `Apply` refuses outside a harness build.

//------------------------------------------------------------------------------------------------
class LF_Checkpoint
{
	static const string FEATURE = "Checkpoint";
	static const int MAX_HOUR = 10; // lf-fixed: the arc has hours 0 to 10 and one file per hour (agent-box/checkpoints)
	static const string REASON = "checkpoint";

	//------------------------------------------------------------------------------------------------
	//! \return The first path that holds the hour's file ($profile only: the dedicated server reads nothing else),
	//! or empty when neither seed place has it.
	static string FindFile(int hour)
	{
		string folder = "$profile:LifeFramework/checkpoints/hour-" + hour + ".json";
		if (FileIO.FileExists(folder))
			return folder;

		string flat = "$profile:LifeFramework/hour-" + hour + ".json";
		if (FileIO.FileExists(flat))
			return flat;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The text of the hour's file, or empty (and an error line) when it is missing or empty.
	static string ReadHour(int hour)
	{
		if (hour < 0 || hour > MAX_HOUR)
		{
			LF_Debug.Error(FEATURE, string.Format("checkpoint refused: hour %1 is outside 0 to %2", hour, MAX_HOUR));
			return "";
		}

		string path = FindFile(hour);
		if (path.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("checkpoint hour %1: no file in $profile:LifeFramework (seed agent-box/checkpoints/hour-%1.json)", hour));
			return "";
		}

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("checkpoint hour %1: %2 would not open", hour, path));
			return "";
		}

		string text;
		string line;
		while (handle.ReadLine(line) >= 0)
		{
			text += line;
		}

		handle.Close();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: applies one checkpoint document to a character. Behind the harness gate.
	//! \return How many parts could not be applied; -1 when refused (not a harness build, no uid, bad JSON).
	static int Apply(string uid, IEntity character, string json)
	{
		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "checkpoint refused: not a LF_HARNESS build");
			return -1;
		}

		return ApplyUnchecked(uid, character, json);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: applies one checkpoint document; callers go through Apply, the suite calls this one.
	//! \return How many parts could not be applied (0 is a clean checkpoint); -1 when refused.
	static int ApplyUnchecked(string uid, IEntity character, string json)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return -1;

		if (uid.IsEmpty() || !character)
		{
			LF_Debug.Error(FEATURE, string.Format("checkpoint refused: uid '%1' character %2", uid, character));
			return -1;
		}

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (json.IsEmpty() || !ctx.ImportFromString(json))
		{
			LF_Debug.Error(FEATURE, "checkpoint refused: the file is not valid JSON");
			return -1;
		}

		int failed;
		failed += ApplyCash(ctx, uid, character);
		failed += ApplyBank(ctx, uid);
		failed += ApplyLevels(ctx, uid);
		failed += ApplyStanding(ctx, uid);
		failed += ApplyRanks(ctx, uid);
		failed += ApplyLicences(ctx, character);

		LF_Debug.Info(FEATURE, string.Format("checkpoint applied to %1: cash %2, bank %3, %4 part(s) failed", uid, LF_MoneyUtils.GetCash(character), BankOf(uid), failed));
		return failed;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True and the int at `key` of the open object; false when it is absent or not a number.
	protected static bool ReadInt(notnull SCR_JsonLoadContext ctx, string key, out int value)
	{
		if (LF_ConfigReader.KindOf(ctx, key) != LF_EConfigJsonKind.NUMBER)
			return false;

		return ctx.ReadValue(key, value);
	}

	//------------------------------------------------------------------------------------------------
	protected static int BankOf(string uid)
	{
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(uid);
		if (!bank)
			return -1;

		return bank.GetBalance();
	}

	//------------------------------------------------------------------------------------------------
	//! Carried cash to exactly `cash`. The playtest top-up tick leaves it alone from then on.
	protected static int ApplyCash(notnull SCR_JsonLoadContext ctx, string uid, IEntity character)
	{
		int cash;
		if (!ReadInt(ctx, "cash", cash) || cash < 0)
			return 0;

		LF_DebugMoneyTopUp.HoldCash(uid);
		int have = LF_MoneyUtils.GetCash(character);
		if (have > cash)
			LF_MoneyUtils.RemoveCash(character, have - cash, REASON);
		else if (have < cash)
			LF_MoneyUtils.AddCash(character, cash - have, REASON);

		if (LF_MoneyUtils.GetCash(character) != cash)
		{
			LF_Debug.Error(FEATURE, string.Format("checkpoint cash: wanted %1, the body holds %2 (no room for it?)", cash, LF_MoneyUtils.GetCash(character)));
			return 1;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The bank balance to exactly `bank`, through deposit and withdraw so the ledger stays whole.
	protected static int ApplyBank(notnull SCR_JsonLoadContext ctx, string uid)
	{
		int bank;
		if (!ReadInt(ctx, "bank", bank) || bank < 0)
			return 0;

		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		if (!account)
		{
			LF_Debug.Error(FEATURE, string.Format("checkpoint bank: no account for %1", uid));
			return 1;
		}

		int have = account.GetBalance();
		if (have > bank)
			account.Withdraw(have - bank, REASON);
		else if (have < bank)
			account.Deposit(bank - have, REASON);

		if (account.GetBalance() != bank)
		{
			LF_Debug.Error(FEATURE, string.Format("checkpoint bank: wanted %1, the account holds %2", bank, account.GetBalance()));
			return 1;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every job in `levels` to its level (the XP that reaches it; the daily cap is skipped and not booked). A job
	//! with no track in this build's jobs config is logged and counted as failed, never a VME.
	protected static int ApplyLevels(notnull SCR_JsonLoadContext ctx, string uid)
	{
		array<string> jobs = {};
		if (LF_ConfigReader.KindOf(ctx, "levels") != LF_EConfigJsonKind.OBJECT || !LF_ConfigReader.ListKeys(ctx, "levels", jobs))
			return 0;

		int failed;
		if (!ctx.StartObject("levels"))
			return 1;

		foreach (string job : jobs)
		{
			int level;
			if (!ReadInt(ctx, job, level))
			{
				LF_Debug.Error(FEATURE, string.Format("checkpoint levels: '%1' is not a number", job));
				failed++;
				continue;
			}

			if (LF_JobLevels.SetLevelUnchecked(uid, job, level) < 0)
			{
				LF_Debug.Error(FEATURE, string.Format("checkpoint levels: %1 level %2 did not apply", job, level));
				failed++;
			}
		}

		ctx.EndObject();
		return failed;
	}

	//------------------------------------------------------------------------------------------------
	//! Standing with each faction in `standing` to exactly that many points (the harness set, not an award).
	protected static int ApplyStanding(notnull SCR_JsonLoadContext ctx, string uid)
	{
		array<string> factions = {};
		if (LF_ConfigReader.KindOf(ctx, "standing") != LF_EConfigJsonKind.OBJECT || !LF_ConfigReader.ListKeys(ctx, "standing", factions))
			return 0;

		int failed;
		if (!ctx.StartObject("standing"))
			return 1;

		foreach (string faction : factions)
		{
			int points;
			if (!ReadInt(ctx, faction, points) || !LF_Reputation.FactionDef(faction))
			{
				LF_Debug.Error(FEATURE, string.Format("checkpoint standing: '%1' is not a faction with a number", faction));
				failed++;
				continue;
			}

			LF_Reputation.HarnessSet(uid, faction, points);
		}

		ctx.EndObject();
		return failed;
	}

	//------------------------------------------------------------------------------------------------
	//! Police and medic rank indexes, kept inside the shipped ladders. A rank 0 with no account stays as it is.
	protected static int ApplyRanks(notnull SCR_JsonLoadContext ctx, string uid)
	{
		int police = -1;
		int medic = -1;
		ReadInt(ctx, "policeRank", police);
		ReadInt(ctx, "medicRank", medic);
		if (police < 0 && medic < 0)
			return 0;

		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (manager)
			account = manager.GetAccount(uid);

		if (!account && (police > 0 || medic > 0))
			account = LF_PlayerAccountManager.GetOrCreate(uid);

		if (!account)
			return 0;

		if (police >= 0)
			account.SetPoliceRank(LF_Utils.MinInt(police, LF_PoliceRanks.TopRank()));

		if (medic >= 0)
			account.SetMedicRank(LF_Utils.MinInt(medic, LF_MedicRanks.TopRank()));

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Unlocks every licence named in the comma list that the character does not hold yet; starter licences stay.
	protected static int ApplyLicences(notnull SCR_JsonLoadContext ctx, IEntity character)
	{
		string list;
		if (LF_ConfigReader.KindOf(ctx, "licences") != LF_EConfigJsonKind.STRING || !ctx.ReadValue("licences", list))
			return 0;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(character);
		array<string> names = {};
		list.Split(",", names, true);
		if (!licences)
			return names.Count();

		int failed;
		foreach (string name : names)
		{
			string upper = name.Trim();
			upper.ToUpper();
			LF_ELicenseType licence;
			if (!LF_ItemCatalogue.ParseLicence(upper, licence))
			{
				LF_Debug.Error(FEATURE, string.Format("checkpoint licences: '%1' is not a licence name", name));
				failed++;
				continue;
			}

			if (!licences.HasLicense(licence) && !licences.UnlockLicense(licence, true))
			{
				LF_Debug.Error(FEATURE, string.Format("checkpoint licences: %1 did not unlock", upper));
				failed++;
			}
		}

		return failed;
	}
}

#ifdef LF_HARNESS
//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "checkpoint")
			return super.Handle(cmd);

		if (cmd.Arg1.IsEmpty() || (cmd.Arg1.ToInt() == 0 && cmd.Arg1 != "0"))
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "checkpoint <hour 0-" + LF_Checkpoint.MAX_HOUR + ">");
			return true;
		}

		int hour = cmd.Arg1.ToInt();
		if (hour < 0 || hour > LF_Checkpoint.MAX_HOUR)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "checkpoint hour " + hour + " is outside 0 to " + LF_Checkpoint.MAX_HOUR);
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no player controller");
			return true;
		}

		controller.LF_AskCheckpoint(hour);
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked for the hour " + hour + " checkpoint (the verdict is the [LFDebug:Checkpoint] line)");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskCheckpoint(int hour)
	{
		if (Replication.IsServer())
			RpcAsk_LF_Checkpoint(hour);
		else
			Rpc(RpcAsk_LF_Checkpoint, hour);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Checkpoint(int hour)
	{
		// Compiled only in a LF_HARNESS build, where this check is open; the dev gate stays the one door.
		IEntity character = GetControlledEntity();
		if (!LF_DevGate.CheckHarnessRequest(character, "checkpoint"))
			return;

		string json = LF_Checkpoint.ReadHour(hour);
		if (json.IsEmpty())
			return;

		string uid = LF_Utils.GetPlayerUID(GetPlayerId());
		LF_Checkpoint.Apply(uid, character, json);
	}
}
#endif
