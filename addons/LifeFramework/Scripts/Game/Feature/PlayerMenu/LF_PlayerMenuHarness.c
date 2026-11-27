// Harness verb `playermenu open [person|progress]|pick <row>|close|state` (the personal file, client
// side). `open` shows the file on a tab the way the F2 key does and asks the server; `pick <n>` clicks the
// Read button of the n-th skill (person tab) or quest (progress tab), `widget_not_found <name>` when the row has
// none (`pick <n> direct` calls the handler instead); `state` answers
// `open=<0|1> tab=<n> data=<0|1> skills=<n> quests=<n> licences=<n>` so a proof waits for the answer.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "playermenu")
			return super.Handle(cmd);

		if (cmd.Arg1 == "open")
		{
			int tab = LF_PlayerMenuSheet.TAB_PERSON;
			if (cmd.Arg2 == "progress")
				tab = LF_PlayerMenuSheet.TAB_PROGRESS;
			else if (cmd.Arg2 == "next")
				tab = LF_PlayerMenuSheet.TAB_NEXT;

			if (!LF_PlayerMenuSheet.Open(tab))
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "the file did not open");
				return true;
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", "tab=" + LF_PlayerMenuSheet.CurrentTab().ToString());
			return true;
		}

		if (cmd.Arg1 == "pick")
		{
			LF_UIScreen screen = LF_PlayerMenuSheet.Current();
			LF_PlayerMenuData data = LF_PlayerMenuSheet.Data();
			if (!screen || !screen.IsOpen() || !data)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "the file is not open or has no data yet");
				return true;
			}

			string row = LF_PlayerMenuSheet.ROW_SKILL;
			if (LF_PlayerMenuSheet.CurrentTab() == LF_PlayerMenuSheet.TAB_PROGRESS)
				row = LF_PlayerMenuSheet.ROW_QUEST;

			string part = row + cmd.Arg2.ToInt().ToString();
			if (cmd.Arg3 == "direct" || cmd.Arg3 == "--direct")
			{
				screen.Notify(part);
				LF_AgentHarness.Answer(cmd.Id, "ok", "picked " + part + " (direct)");
				return true;
			}

			// The default presses the row's Read button like a click; no visible button is `widget_not_found <name>`.
			string error = LF_AgentHarness.PressWidget(part + "_Action", 0);
			if (!error.IsEmpty())
			{
				LF_AgentHarness.Answer(cmd.Id, "error", error);
				return true;
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", "picked " + part + " (ui)");
			return true;
		}

		if (cmd.Arg1 == "seed")
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (!controller)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "no player controller");
				return true;
			}

			controller.LF_AskPlayerMenuSeed();
			LF_AgentHarness.Answer(cmd.Id, "ok", "asked the server to seed the file");
			return true;
		}

		if (cmd.Arg1 == "close")
		{
			LF_PlayerMenuSheet.Hide();
			LF_AgentHarness.Answer(cmd.Id, "ok", "closed");
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			int open = 0;
			if (LF_PlayerMenuSheet.IsOpen())
				open = 1;

			LF_PlayerMenuData state = LF_PlayerMenuSheet.Data();
			int hasData = 0;
			int skills = 0;
			int quests = 0;
			int licences = 0;
			if (state)
			{
				hasData = 1;
				skills = state.m_aSkills.Count();
				quests = state.m_aQuests.Count();
				licences = state.m_aLicenceKeys.Count();
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("open=%1 tab=%2 data=%3 skills=%4 quests=%5 licences=%6", open, LF_PlayerMenuSheet.CurrentTab(), hasData, skills, quests, licences));
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "usage: playermenu open [person|progress|next]|pick <n> [direct]|seed|close|state");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! `playermenu seed`: a file worth reading for a session proof's pictures. The server grants the
//! caller XP in a few jobs, cash and savings, and the first unowned house. Same gate as `repgrant`
//! and /licence: development worlds and harness builds, or an administrator.
modded class SCR_PlayerController
{
	static const int LF_SEED_CASH = 2350;
	static const int LF_SEED_BANK = 18400;

	//------------------------------------------------------------------------------------------------
	void LF_AskPlayerMenuSeed()
	{
		if (Replication.IsServer())
			RpcAsk_LF_PlayerMenuSeed();
		else
			Rpc(RpcAsk_LF_PlayerMenuSeed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PlayerMenuSeed()
	{
		int playerId = GetPlayerId();
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(playerId, LF_PlayerMenuServer.FEATURE, "harness file seed");
		if (!allowed)
			return;

		string uid = LF_Utils.GetPlayerUID(playerId);
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (uid.IsEmpty() || !body)
		{
			LF_Debug.Error(LF_PlayerMenuServer.FEATURE, string.Format("harness seed refused: player %1 has no id or body", playerId));
			return;
		}

		LF_JobLevels.AddXp(uid, "MINER", 260, "harness_seed");
		LF_JobLevels.AddXp(uid, "DELIVERY", 140, "harness_seed");
		LF_JobLevels.AddXp(uid, "FARMER", 70, "harness_seed");
		LF_JobLevels.AddXp(uid, "FISHER", 35, "harness_seed");
		LF_MoneyUtils.AddCash(body, LF_SEED_CASH, "harness_seed");
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		if (atm)
			LF_ATMManager.GetOrCreate(uid).Deposit(LF_SEED_BANK, "harness_seed");

		int houses = LF_PlayerMenuServer.HousesOf(uid);
		int live = LF_HouseManagerComponent.Live().Count();
		int free;
		foreach (string id, LF_HouseManagerComponent house : LF_HouseManagerComponent.Live())
		{
			if (!house || !house.GetOwnerKey().IsEmpty())
				continue;

			free++;
			if (houses > 0)
				continue;

			house.SetOwner(uid);
			houses = LF_PlayerMenuServer.HousesOf(uid);
		}

		LF_Debug.Info(LF_PlayerMenuServer.FEATURE, string.Format("harness seed for player %1 (%2): xp, $%3 cash, $%4 bank; %5 live house(s), %6 unowned; owns %7 now", playerId, uid, LF_SEED_CASH, LF_SEED_BANK, live, free, houses));
	}
}
