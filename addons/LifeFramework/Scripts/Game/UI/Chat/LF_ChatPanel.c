// SPDX-License-Identifier: MIT
// Ported from https://github.com/thebonbon/EveronLife branch dedicated_server.
//------------------------------------------------------------------------------------------------
//! Adds the roleplay channel styles (OOC global, admin) to the vanilla chat panel.
//! Style choice lives in LF_RoleplayChat so admin and OOC stay on the same path.
modded class SCR_ChatPanel
{
	[Attribute()]
	ref SCR_ChatMessageStyle m_OOCGlobalStyle;

	[Attribute()]
	ref SCR_ChatMessageStyle m_AdminStyle;

	//------------------------------------------------------------------------------------------------
	override protected void VerifyChannelStyles()
	{
		int n = 0;

		if (!m_SystemChannelStyle)
		{
			m_SystemChannelStyle = new SCR_ChatMessageStyle();
			n++;
		}

		array<Managed> styles = {
			m_SystemChannelStyle,
			m_RadioProtocolStyle,
			m_GlobalChannelStyle,
			m_FactionChannelStyle,
			m_VehicleChannelStyle,
			m_LocalChannelStyle,
			m_DirectChannelStyle,
			m_OOCGlobalStyle,
			m_AdminStyle
		};

		foreach (Managed style : styles)
		{
			if (!style)
				n++;
		}

		if (n > 0)
		{
			string s = string.Format("[Chat] %1 chat channel styles are not configured.", n);
			Print(s, LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected SCR_ChatMessageStyle GetChannelStyle(BaseChatChannel channel)
	{
		if (channel == null)
			return m_SystemChannelStyle;

		LF_ERoleplayChatStyle kind = LF_RoleplayChat.ResolveStyle(channel.Type(), channel.GetName());
		SCR_ChatMessageStyle style = StyleFor(kind);

		if (!style)
			return m_SystemChannelStyle;

		return style;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ChatMessageStyle StyleFor(LF_ERoleplayChatStyle kind)
	{
		if (kind == LF_ERoleplayChatStyle.GLOBAL)
			return m_GlobalChannelStyle;

		if (kind == LF_ERoleplayChatStyle.OOC_GLOBAL)
			return m_OOCGlobalStyle;

		if (kind == LF_ERoleplayChatStyle.LOCAL)
			return m_LocalChannelStyle;

		if (kind == LF_ERoleplayChatStyle.ADMIN)
			return m_AdminStyle;

		if (kind == LF_ERoleplayChatStyle.GROUP)
			return m_GroupChannelStyle;

		if (kind == LF_ERoleplayChatStyle.FACTION)
			return m_FactionChannelStyle;

		if (kind == LF_ERoleplayChatStyle.VEHICLE)
			return m_VehicleChannelStyle;

		if (kind == LF_ERoleplayChatStyle.DIRECT)
			return m_DirectChannelStyle;

		return m_SystemChannelStyle;
	}
}