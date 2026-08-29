//------------------------------------------------------------------------------------------------
//! Adds the roleplay channel styles (OOC global, admin) to the vanilla chat panel
//! and routes channel styles by channel name for the custom roleplay channels.
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

		SCR_ChatMessageStyle style;
		switch (channel.Type())
		{
			case BaseChatChannel:
			{
				if (channel.GetName() == "Global")
					style = m_GlobalChannelStyle;
				if (channel.GetName() == "OOCGlobal")
					style = m_OOCGlobalStyle;
				break;
			}

			case GroupChatChannel:
				style = m_GroupChannelStyle;
				break;
			case FactionChatChannel:
				style = m_FactionChannelStyle;
				break;
			case SCR_VehicleChatChannel:
				style = m_VehicleChannelStyle;
				break;
			case LocalChatChannel:
				style = m_LocalChannelStyle;
				break;
			case PrivateMessageChannel:
				style = m_DirectChannelStyle;
				break;
		}

		if (!style)
			return m_SystemChannelStyle;

		return style;
	}
}