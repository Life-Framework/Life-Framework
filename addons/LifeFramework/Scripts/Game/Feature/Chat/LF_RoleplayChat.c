// SPDX-License-Identifier: MIT
// Ported from https://github.com/thebonbon/EveronLife branch dedicated_server.
// OOC channel, admin channel styling, VoN display-name suppression, and
// ArmaVision limited to administrators. The commented-out kill log, item log,
// and map icons on that branch are not ported.

enum LF_ERoleplayChatStyle
{
	SYSTEM,
	GLOBAL,
	OOC_GLOBAL,
	LOCAL,
	ADMIN,
	GROUP,
	FACTION,
	VEHICLE,
	DIRECT
}

class LF_RoleplayChatStyle
{
	string m_sName;
	string m_sPrefix;
	string m_sIconName;
	bool m_bColoredBackground;
}

class LF_RoleplayChat
{
	//------------------------------------------------------------------------------------------------
	//! VoN overlay keeps the incoming-name widget hidden.
	static bool ShowsVonDisplayName()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! ArmaVision (editor PHOTO mode) is limited to administrators.
	static bool ArmaVisionAllowed(bool isAdministrator)
	{
		return isAdministrator;
	}

	//------------------------------------------------------------------------------------------------
	//! Channel class and name decide the roleplay style. OOC is the BaseChatChannel
	//! named OOCGlobal. Admin styling follows AdminChatChannel.
	static LF_ERoleplayChatStyle ResolveStyle(typename channelType, string channelName)
	{
		if (channelType == AdminChatChannel)
			return LF_ERoleplayChatStyle.ADMIN;

		if (channelType == LocalChatChannel)
			return LF_ERoleplayChatStyle.LOCAL;

		if (channelType == GroupChatChannel)
			return LF_ERoleplayChatStyle.GROUP;

		if (channelType == FactionChatChannel)
			return LF_ERoleplayChatStyle.FACTION;

		if (channelType == SCR_VehicleChatChannel)
			return LF_ERoleplayChatStyle.VEHICLE;

		if (channelType == PrivateMessageChannel)
			return LF_ERoleplayChatStyle.DIRECT;

		if (channelType == BaseChatChannel)
		{
			if (channelName == "Global")
				return LF_ERoleplayChatStyle.GLOBAL;

			if (channelName == "OOCGlobal")
				return LF_ERoleplayChatStyle.OOC_GLOBAL;
		}

		return LF_ERoleplayChatStyle.SYSTEM;
	}

	//------------------------------------------------------------------------------------------------
	static LF_RoleplayChatStyle Describe(LF_ERoleplayChatStyle style)
	{
		LF_RoleplayChatStyle described = new LF_RoleplayChatStyle();

		if (style == LF_ERoleplayChatStyle.GLOBAL)
		{
			described.m_sName = "Twatter";
			described.m_sPrefix = "";
			described.m_sIconName = "twatter";
			described.m_bColoredBackground = true;
			return described;
		}

		if (style == LF_ERoleplayChatStyle.OOC_GLOBAL)
		{
			described.m_sName = "OOC";
			described.m_sPrefix = "/ooc";
			described.m_sIconName = "ooc";
			described.m_bColoredBackground = false;
			return described;
		}

		if (style == LF_ERoleplayChatStyle.LOCAL)
		{
			described.m_sName = "OOC Local";
			described.m_sPrefix = "/oocl";
			described.m_sIconName = "ooc";
			described.m_bColoredBackground = false;
			return described;
		}

		if (style == LF_ERoleplayChatStyle.ADMIN)
		{
			described.m_sName = "Admin";
			described.m_sPrefix = "/admin";
			described.m_sIconName = "admin";
			described.m_bColoredBackground = true;
			return described;
		}

		return described;
	}
}
