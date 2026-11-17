//------------------------------------------------------------------------------------------------
//! Admin-only chat channel. Processes the !wadd command: POLICE puts the UUID on the police roster.
//! The fork also carried !shutdown and !hint here; both depended on the fork's
//! LF_RpcSenderComponent (global hints) which this repo does not have, so they are not ported.
class AdminChatChannel : BaseChatChannel
{
	//------------------------------------------------------------------------------------------------
	override bool ProcessMessage(BaseChatComponent sender, string message, bool isAuthority)
	{
		if (!isAuthority)
			return true;

		// The authority re-checks the sender's role. IsAvailable only gates the UI; a modded
		// client can still send on this channel, and !wadd grants police whitelist.
		if (!IsAdminSender(sender))
		{
			LF_Debug.Warn("Chat", "admin message refused: sender is not an administrator");
			return false;
		}

		//!wadd TYPE UID
		//eg. "!wadd POLICE 544-242-544-242"
		if (message.StartsWith("!wadd"))
		{
			LF_WhitelistType type;
			string uuid;
			if (!TryParseWhitelistCommand(message, type, uuid))
			{
				LF_Debug.Warn("Chat", string.Format("admin !wadd refused: '%1' is not '!wadd TYPE UID' with a known type", message));
				return false;
			}

			string wlAdminUid = "";
			SCR_PlayerController wlAdmin = SCR_PlayerController.Cast(sender.GetOwner());
			if (wlAdmin)
				wlAdminUid = LF_AdminAction.UidOfPlayer(wlAdmin.GetPlayerId());

			ApplyWhitelistAdd(type, uuid, wlAdminUid);
		}

		//!preject UID (#803): rejects the open police application of that account and starts the reapply cooldown
		if (message.StartsWith("!preject"))
		{
			string rejectUid;
			if (!LF_PoliceApplicationRules.TryParseReject(message, rejectUid))
				return false;

			LF_PoliceApplications.Reject(rejectUid, true, System.GetUnixTime());
		}

		//!handled NUMBER (#241): an incident a player reported has been dealt with
		if (message.StartsWith("!handled"))
		{
			int number;
			if (!LF_IncidentFlags.TryParseHandled(message, number))
				return false;

			SCR_PlayerController adminPC = SCR_PlayerController.Cast(sender.GetOwner());
			string adminUid = "";
			if (adminPC)
				adminUid = LF_Utils.GetAccountUID(adminPC.GetPlayerId());

			LF_IncidentFlags.Handle(number, adminUid);
			LF_AdminAction.ReportHandled(adminUid, number);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the grant behind "!wadd TYPE UID". Writes the roster police duty reads
	//! (LF_WhitelistManager.AddFactionWhitelist) and logs and reports to staff only when that stored a
	//! new entry. A type with no roster, or a uid already on it, logs and reports nothing.
	//! \return True when a new roster entry was stored.
	static bool ApplyWhitelistAdd(LF_WhitelistType type, string uuid, string adminUid)
	{
		string typeName = SCR_Enum.GetEnumName(LF_WhitelistType, type);

		LF_EFactionType faction;
		if (!TryRosterFaction(type, faction))
		{
			LF_Debug.Warn("Chat", string.Format("admin !wadd %1 %2 refused: %1 has no roster to grant", typeName, uuid));
			return false;
		}

		if (!LF_WhitelistManager.AddFactionWhitelist(uuid, faction))
		{
			LF_Debug.Warn("Chat", string.Format("admin !wadd %1 %2 stored nothing: already on the roster", typeName, uuid));
			return false;
		}

		LF_AdminAction.ReportWhitelistAdd(adminUid, type, uuid);
		LF_Debug.Log("Chat", string.Format("admin !wadd type=%1 uuid=%2", typeName, uuid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The roster a !wadd type writes. Only POLICE has one: there is no medic faction type.
	static bool TryRosterFaction(LF_WhitelistType type, out LF_EFactionType faction)
	{
		if (type == LF_WhitelistType.POLICE)
		{
			faction = LF_EFactionType.POLICE;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Parses "!wadd TYPE UID" into a whitelist type and a player UUID.
	static bool TryParseWhitelistCommand(string message, out LF_WhitelistType type, out string uuid)
	{
		array<string> strs = new array<string>();
		message.Split(" ", strs, true);
		if (strs.Count() != 3 || strs[0] != "!wadd")
			return false;

		LF_WhitelistType parsedType = typename.StringToEnum(LF_WhitelistType, strs[1]);
		if (parsedType == -1)
			return false;

		type = parsedType;
		uuid = strs[2];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsDelivering(BaseChatComponent sender, BaseChatComponent receiver)
	{
		return IsAvailable(receiver);
	}

	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(BaseChatComponent sender)
	{
		if (!LF_Systems.IsEnabled("Chat"))
			return false;

		return IsAdminSender(sender);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the chat component belongs to a player controller whose player holds the
	//! ADMINISTRATOR role. Null-safe.
	static bool IsAdminSender(BaseChatComponent sender)
	{
		if (!sender)
			return false;

		SCR_PlayerController senderPC = SCR_PlayerController.Cast(sender.GetOwner());
		if (!senderPC)
			return false;

		return IsAdminPlayer(senderPC.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! True when playerId holds the ADMINISTRATOR role. An unknown or invalid id is not an admin.
	static bool IsAdminPlayer(int playerId)
	{
		if (playerId <= 0)
			return false;

		if (!GetGame())
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		if (playerManager.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR))
			return true;

		// A harness session (-scrDefine LF_HARNESS) has no server.json to list game.admins in, and the engine
		// refuses a script grant of ADMINISTRATOR, so `up --admin-client N` gives SESSION_ADMINISTRATOR
		// (LF_HarnessStaff) and a harness build counts it. No other build does.
		return LF_DevGate.HarnessBuild() && playerManager.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR);
	}
}