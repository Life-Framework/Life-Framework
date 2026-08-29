//------------------------------------------------------------------------------------------------
//! Admin-only chat channel. Processes the !wadd command to whitelist a player UUID.
//! The fork also carried !shutdown and !hint here; both depended on the fork's
//! EL_RpcSenderComponent (global hints) which this repo does not have, so they are not ported.
class AdminChatChannel : BaseChatChannel
{
	//------------------------------------------------------------------------------------------------
	override bool ProcessMessage(BaseChatComponent sender, string message, bool isAuthority)
	{
		if (!isAuthority)
			return true;

		//!wadd TYPE UID
		//eg. "!wadd POLICE 544-242-544-242"
		if (message.StartsWith("!wadd"))
		{
			EL_WhitelistType type;
			string uuid;
			if (!TryParseWhitelistCommand(message, type, uuid))
				return false;

			EL_WhitelistRespawnHandlerComponent whitelistComponent = EL_WhitelistRespawnHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(EL_WhitelistRespawnHandlerComponent));
			if (!whitelistComponent)
				return false;

			whitelistComponent.EnableWhitelistOnPlayer(uuid, type);
			EL_Debug.Log("Chat", string.Format("admin !wadd type=%1 uuid=%2", SCR_Enum.GetEnumName(EL_WhitelistType, type), uuid));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Parses "!wadd TYPE UID" into a whitelist type and a player UUID.
	static bool TryParseWhitelistCommand(string message, out EL_WhitelistType type, out string uuid)
	{
		array<string> strs = new array<string>();
		message.Split(" ", strs, true);
		if (strs.Count() != 3 || strs[0] != "!wadd")
			return false;

		EL_WhitelistType parsedType = typename.StringToEnum(EL_WhitelistType, strs[1]);
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
		SCR_PlayerController senderPC = SCR_PlayerController.Cast(sender.GetOwner());
		return senderPC && GetGame().GetPlayerManager().HasPlayerRole(senderPC.GetPlayerId(), EPlayerRole.ADMINISTRATOR);
	}
}