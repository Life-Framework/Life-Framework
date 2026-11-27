//------------------------------------------------------------------------------------------------
//! Admin money grant: `#lfmoney <player id or name> <amount> [cash|bank]`.
//! A vanilla server command (as #kick and #banrecord), so the engine routes it from the chat of a
//! logged-in administrator (`#login <password>`, or a name in game.admins) and from RCON with the
//! admin permission. The server decides: the handler checks the caller's role again, validates the
//! amount, and moves the money through the one existing path (LF_MoneyUtils for cash,
//! LF_BankAccount for the bank), so the ledger records it with reason "admin-grant" and the next
//! save carries it. A negative amount takes money. The target must be connected.
//! Every grant is logged (LF_Debug "Admin") and reported as an admin action to staff.
//------------------------------------------------------------------------------------------------
class LF_AdminMoneyRequest
{
	//! A player id (digits) or a player name, as typed.
	string m_sTarget;
	int m_iAmount;
	bool m_bBank;
}

//------------------------------------------------------------------------------------------------
class LF_AdminMoney
{
	static const string FEATURE = "Admin";
	static const string KEYWORD = "lfmoney";
	static const string REASON = "admin-grant";
	static const string USAGE = "#lfmoney <player id or name> <amount> [cash|bank]  (negative amount takes money)";

	//! The most one call may give or take.
	static const int MAX_AMOUNT = 1000000; // lf-fixed: an admin safety cap, not a game tunable
	//! A bank balance never goes above this through a grant (int headroom).
	static const int MAX_BANK_BALANCE = 2000000000; // lf-fixed: int headroom, not a game tunable

	//------------------------------------------------------------------------------------------------
	//! True when the caller may grant. A chat caller must hold the administrator role (the engine
	//! gates it too; this is the second check). RCON is gated by its admin permission before the
	//! handler runs, so it has no player role to check.
	static bool MayGrant(bool viaChat, bool callerIsAdmin)
	{
		if (!viaChat)
			return true;

		return callerIsAdmin;
	}

	//------------------------------------------------------------------------------------------------
	//! "" when the amount may be moved, else the refusal text. Zero and anything past the cap are refused.
	static string AmountError(int amount)
	{
		if (amount == 0)
			return "Amount must not be 0.";

		int size = amount;
		if (size < 0)
			size = -size;

		if (size > MAX_AMOUNT)
			return string.Format("Amount is limited to %1 per call.", MAX_AMOUNT);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a whole number: optional leading + or -, then digits only. Anything else (empty, a
	//! decimal, text, more than 9 digits) does not parse, so a typo never becomes 0 or a wrapped value.
	static bool TryParseAmount(string text, out int amount)
	{
		amount = 0;
		string digits = "0123456789";
		int length = text.Length();
		int start = 0;
		bool negative = false;
		if (length > 0 && (text.Get(0) == "-" || text.Get(0) == "+"))
		{
			negative = text.Get(0) == "-";
			start = 1;
		}

		int count = length - start;
		if (count < 1 || count > 9)
			return false;

		for (int i = start; i < length; i++)
		{
			if (!digits.Contains(text.Get(i)))
				return false;
		}

		amount = text.Substring(start, count).ToInt();
		if (negative)
			amount = -amount;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the command words (argv[0] is the keyword): everything between the keyword and the
	//! amount is the target (a name may have spaces), an optional last word is cash or bank.
	//! \return "" when parsed, else the text to answer.
	static string TryParse(array<string> argv, out LF_AdminMoneyRequest request)
	{
		request = null;
		if (!argv || argv.Count() < 3)
			return USAGE;

		int last = argv.Count() - 1;
		bool bank = false;
		string mode = argv[last];
		mode.ToLower();
		if (mode == "bank" || mode == "cash")
		{
			bank = mode == "bank";
			last = last - 1;
		}

		if (last < 2)
			return USAGE;

		int amount;
		if (!TryParseAmount(argv[last], amount))
			return "Amount must be a whole number, for example 500 or -500.";

		string target = "";
		for (int i = 1; i < last; i++)
		{
			if (!target.IsEmpty())
				target = target + " ";

			target = target + argv[i];
		}

		target.Replace("\"", "");
		if (target.IsEmpty())
			return USAGE;

		string error = AmountError(amount);
		if (!error.IsEmpty())
			return error;

		request = new LF_AdminMoneyRequest();
		request.m_sTarget = target;
		request.m_iAmount = amount;
		request.m_bBank = bank;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The player id a target text names: digits are an id (it must be in `ids`), else the one player
	//! whose name matches ignoring case. -1 when nobody matches, -2 when several do.
	static int ResolveTarget(string target, notnull array<int> ids, notnull array<string> names)
	{
		if (ids.Count() != names.Count())
			return -1;

		int asId;
		if (TryParseAmount(target, asId) && asId > 0 && !target.StartsWith("+"))
		{
			if (ids.Find(asId) != -1)
				return asId;

			return -1;
		}

		string wanted = target;
		wanted.ToLower();
		int found = -1;
		for (int i = 0; i < ids.Count(); i++)
		{
			string name = names[i];
			name.ToLower();
			if (name != wanted)
				continue;

			if (found != -1)
				return -2;

			found = ids[i];
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the amount on a bank account: a deposit for a positive amount, a charge (never into debt)
	//! for a negative one. A deposit that would pass MAX_BANK_BALANCE moves nothing.
	//! \return the money that moved, signed like the request (0 when nothing moved)
	static int ApplyBank(notnull LF_BankAccount account, int amount)
	{
		if (amount > 0)
		{
			if (amount > MAX_BANK_BALANCE - account.GetBalance())
				return 0;

			account.Deposit(amount, REASON);
			return amount;
		}

		if (amount < 0)
			return -account.Charge(-amount, false, REASON);

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the amount in the character's pockets; cash that does not fit lands at their feet.
	//! \return the money that moved, signed like the request
	static int ApplyCash(IEntity character, int amount)
	{
		if (!character)
			return 0;

		if (amount > 0)
			return LF_MoneyUtils.AddCash(character, amount, REASON, "", true);

		if (amount < 0)
			return -LF_MoneyUtils.RemoveCash(character, -amount, REASON);

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The answer to the admin for a finished grant.
	static string Describe(int requested, int moved, bool bank, string targetName)
	{
		string place = "cash";
		if (bank)
			place = "bank";

		if (moved == 0)
			return string.Format("Nothing moved for %1 (%2): no room or no funds for %3.", targetName, place, LF_MoneyFormat.FormatMoney(requested));

		string verb = "Gave";
		int shown = moved;
		if (moved < 0)
		{
			verb = "Took";
			shown = -moved;
		}

		string note = "";
		if (moved != requested)
			note = string.Format(" (asked for %1)", LF_MoneyFormat.FormatMoney(requested));

		return string.Format("%1 %2 (%3) for %4%5.", verb, LF_MoneyFormat.FormatMoney(shown), place, targetName, note);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: runs one parsed request for a caller. `callerId` is the player id (0 for RCON).
	//! \return the text to answer; `ok` tells an answer from a refusal
	static string Execute(array<string> argv, int callerId, bool viaChat, out bool ok)
	{
		ok = false;
		if (!Replication.IsServer())
			return "The server decides this.";

		if (!MayGrant(viaChat, AdminChatChannel.IsAdminPlayer(callerId)))
		{
			LF_Debug.Warn(FEATURE, string.Format("#lfmoney refused: player %1 is not an administrator", callerId));
			return "Only a logged-in administrator can use #lfmoney.";
		}

		LF_AdminMoneyRequest request;
		string error = TryParse(argv, request);
		if (!error.IsEmpty() || !request)
			return error;

		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return "No player manager.";

		array<int> ids = {};
		array<string> names = {};
		array<int> connected = {};
		manager.GetPlayers(connected);
		foreach (int id : connected)
		{
			ids.Insert(id);
			names.Insert(manager.GetPlayerName(id));
		}

		int targetId = ResolveTarget(request.m_sTarget, ids, names);
		if (targetId == -2)
			return string.Format("More than one player is called '%1'; use the player id.", request.m_sTarget);

		if (targetId < 1)
			return string.Format("No connected player '%1'.", request.m_sTarget);

		string targetName = manager.GetPlayerName(targetId);
		string uid = LF_Utils.GetPlayerUID(targetId);
		if (uid.IsEmpty())
			return string.Format("%1 has no account yet; try again in a moment.", targetName);

		int moved;
		if (request.m_bBank)
		{
			moved = ApplyBank(LF_ATMManager.GetOrCreate(uid), request.m_iAmount);
		}
		else
		{
			IEntity character = manager.GetPlayerControlledEntity(targetId);
			// A corpse or an unconscious body would take the cash for a body that may never be looted by its owner.
			if (!character || !LF_Utils.IsAliveState(character))
				return string.Format("%1 has no living character to carry cash; try again or use bank.", targetName);

			moved = ApplyCash(character, request.m_iAmount);
		}

		string staff = LF_BanRecord.STAFF_RCON;
		if (viaChat)
			staff = LF_AdminAction.UidOfPlayer(callerId);

		string accountName = "cash";
		if (request.m_bBank)
			accountName = "bank";

		LF_Debug.Info(FEATURE, string.Format("#lfmoney caller=%1 target=%2 (%3) account=%4 requested=%5 moved=%6", staff, targetName, uid, accountName, request.m_iAmount, moved));
		LF_AdminAction.Report(staff, LF_Utils.GetAccountUID(targetId), string.Format("#lfmoney %1 %2 %3", request.m_iAmount, accountName, moved), vector.Zero);

		ok = true;
		return Describe(request.m_iAmount, moved, request.m_bBank, targetName);
	}
}

//------------------------------------------------------------------------------------------------
//! `#lfmoney`: administrators only, from chat (after #login) or RCON.
class LF_AdminMoneyCommand : ScrServerCommand
{
	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return LF_AdminMoney.KEYWORD;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsServerSide()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredRCONPermission()
	{
		return ERCONPermissions.PERMISSIONS_ADMIN;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredChatPermission()
	{
		return EPlayerRole.ADMINISTRATOR;
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Answer(array<string> argv, int callerId, bool viaChat)
	{
		bool ok;
		string text = LF_AdminMoney.Execute(argv, callerId, viaChat, ok);
		if (ok)
			return ScrServerCmdResult(text, EServerCmdResultType.OK);

		return ScrServerCmdResult(text, EServerCmdResultType.ERR);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return Answer(argv, playerId, true);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return Answer(argv, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}
}
