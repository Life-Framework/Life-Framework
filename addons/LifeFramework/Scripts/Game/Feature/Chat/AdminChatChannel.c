//------------------------------------------------------------------------------------------------
//! Admin-only chat channel. Processes the !wadd command to whitelist a player UUID.
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
				return false;

			LF_WhitelistRespawnHandlerComponent whitelistComponent = LF_WhitelistRespawnHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(LF_WhitelistRespawnHandlerComponent));
			if (!whitelistComponent)
				return false;

			whitelistComponent.EnableWhitelistOnPlayer(uuid, type);
			LF_Debug.Log("Chat", string.Format("admin !wadd type=%1 uuid=%2", SCR_Enum.GetEnumName(LF_WhitelistType, type), uuid));
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
				adminUid = LF_Utils.GetPlayerUID(adminPC.GetPlayerId());

			LF_IncidentFlags.Handle(number, adminUid);
		}

		return true;
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

		return playerManager.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR);
	}
}