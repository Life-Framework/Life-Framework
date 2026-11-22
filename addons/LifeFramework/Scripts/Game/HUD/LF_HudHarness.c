// LF_HudHarness.c: the `hud` harness verb, so a session can put the subtle HUD into the states its
// pictures need: `hud <wanted> <hunger> <thirst> [bankDeposit] [hold|nohold]`; `-` (or nothing) leaves that value. The server applies it behind the
// dev gate through the same mutators play uses (SetWantedLevel, SetSurvivalStats, Deposit), so the
// HUD shows what replication really delivers.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "hud")
			return super.Handle(cmd);

		// "hold" (5th word) keeps the delta and the wanted caption up 20 s on this client, for shots.
		if (cmd.Arg5 == "hold")
			LF_HudLogic.SetHarnessHold(20);
		else if (cmd.Arg5 == "nohold")
			LF_HudLogic.SetHarnessHold(0);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		character.LF_AskHarnessHud(cmd.Id, ArgOrKeep(cmd.Arg1), ArgOrKeep(cmd.Arg2), ArgOrKeep(cmd.Arg3), ArgOrKeep(cmd.Arg4));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! -1 (leave the value as it is) for an empty or "-" argument, so a short command never zeroes
	//! the wanted level (which would also take the posters down).
	protected static int ArgOrKeep(string arg)
	{
		if (arg.IsEmpty() || arg == "-")
			return -1;

		return arg.ToInt();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	static const string LF_HUD_HARNESS_REASON = "harness-hud";

	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessHud(int commandId, int wanted, int hunger, int thirst, int bankDeposit)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessHud(commandId, true, wanted, hunger, thirst, bankDeposit);
			return;
		}

		Rpc(RpcAsk_LF_HarnessHud, commandId, wanted, hunger, thirst, bankDeposit);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessHud(int commandId, int wanted, int hunger, int thirst, int bankDeposit)
	{
		bool gateOpen = LF_DevGate.CheckHarnessRequest(this, "hud");
		LF_ApplyHarnessHud(commandId, gateOpen, wanted, hunger, thirst, bankDeposit);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessHud(int commandId, bool gateOpen, int wanted, int hunger, int thirst, int bankDeposit)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		// The account is keyed by the player (as LF_AccountReplication.PublishForPlayer reads it), the
		// bank by the active character (as the ATM reads it).
		string uid = LF_Utils.GetPlayerUID(this);
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(this);
		string accountUid = LF_Utils.GetAccountUID(playerId);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts && !accountUid.IsEmpty())
			account = accounts.GetAccount(accountUid);
		if (account && wanted >= 0)
			account.SetWantedLevel(wanted);

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(FindComponent(LF_CharacterSurvivalComponent));
		if (survival && survival.GetSurvivalStats() && (hunger >= 0 || thirst >= 0))
		{
			LF_SurvivalStats stats = survival.GetSurvivalStats();
			if (hunger >= 0)
				stats.SetHunger(hunger);
			if (thirst >= 0)
				stats.SetThirst(thirst);
			survival.SetSurvivalStats(stats);
		}

		if (bankDeposit > 0)
		{
			LF_BankAccount bank;
			if (!uid.IsEmpty())
				bank = LF_ATMManager.GetOrCreate(uid);
			if (!bank)
			{
				LF_Debug.Warn(LF_HudLogic.FEATURE, "harness hud: no bank account for this body, deposit refused");
				LF_HarnessReply(commandId, "error", "no_bank_account");
				return;
			}

			// Its own ledger reason, so harness money never reads as a real deposit.
			bank.Deposit(bankDeposit, LF_HUD_HARNESS_REASON);
		}

		LF_Debug.Info(LF_HudLogic.FEATURE, string.Format("harness hud: wanted %1 hunger %2 thirst %3 bank +%4 (account %5)", wanted, hunger, thirst, bankDeposit, account != null));
		LF_HarnessReply(commandId, "ok", string.Format("wanted=%1 hunger=%2 thirst=%3 bank+=%4", wanted, hunger, thirst, bankDeposit));
	}
}
