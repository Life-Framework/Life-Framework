//------------------------------------------------------------------------------------------------
//! Harness `chat [channel] <text>` (#1451 admin proof): this client types one chat line the way the
//! chat box does, through the real path (SCR_ChatComponent.SendMessage on the channel's id), so the
//! server's channel handler sees a real sender: `!handled <n>`, `!wadd POLICE <uid>`, `!preject <uid>`
//! go on the Admin channel (AdminChatChannel re-checks the sender's role on the server, so a client
//! that is not an administrator is refused there: `[LFDebug:Chat] admin message refused`).
//! The reply says only that the line was sent; the verdict is the server log.
//!   channel: admin, global, local, ooc (OOCGlobal). Left out, a line starting with `!` goes to admin,
//!   anything else to global. `/` is the chat box's own UI command prefix and is not a message, so it
//!   is refused here; engine `#` commands travel as ordinary text on the global channel.
//! Allowed only in a harness build or where dev tools are open (LF_DevGate), like the other stand-ins.
//------------------------------------------------------------------------------------------------
class LF_HarnessChat
{
	static const string FEATURE = "AgentHarness";
	static const int MAX_LENGTH = 400;

	//------------------------------------------------------------------------------------------------
	//! Pure: the channel name a token names, "" when it is not one.
	static string ChannelOf(string token)
	{
		string lower = token;
		lower.ToLower();
		if (lower == "admin")
			return "Admin";

		if (lower == "global")
			return "Global";

		if (lower == "local")
			return "Local";

		if (lower == "ooc" || lower == "oocglobal")
			return "OOCGlobal";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the channel a line goes to when the sender named none.
	static string DefaultChannelFor(string text)
	{
		if (text.StartsWith("!"))
			return "Admin";

		return "Global";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: splits the words after the verb into channel and text. Returns false for no text.
	static bool Split(array<string> words, out string channel, out string text)
	{
		channel = "";
		text = "";
		int first = 0;
		if (words.Count() > 1)
		{
			channel = ChannelOf(words[0]);
			if (!channel.IsEmpty())
				first = 1;
		}

		for (int i = first; i < words.Count(); i++)
		{
			if (!text.IsEmpty())
				text = text + " ";

			text = text + words[i];
		}

		if (text.IsEmpty())
			return false;

		if (channel.IsEmpty())
			channel = DefaultChannelFor(text);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: sends the line. \return "ok sent <channel> ..." or "error <reason>".
	static string Send(string channelName, string text)
	{
		if (text.IsEmpty())
			return "error no_text";

		if (text.Length() > MAX_LENGTH)
			return "error text_too_long";

		if (text.StartsWith("/"))
			return "error slash_is_a_chat_box_command";

		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return "error no_controller";

		if (!LF_DevGate.HarnessBuild() && !LF_DevGate.IsAllowedForPlayer(controller.GetPlayerId()))
			return "error dev_gate_closed";

		SCR_ChatComponent chat = SCR_ChatComponent.Cast(controller.FindComponent(SCR_ChatComponent));
		BaseChatEntity entity = GetGame().GetChat();
		if (!chat || !entity)
			return "error no_chat";

		int channelId = -1;
		int count = entity.GetChannelsCount();
		for (int i = 0; i < count; i++)
		{
			if (entity.GetChannelName(i) == channelName)
			{
				channelId = i;
				break;
			}
		}

		if (channelId < 0)
			return "error no_channel " + channelName;

		chat.SendMessage(text, channelId);
		LF_Debug.Info(FEATURE, string.Format("chat sent on %1 (%2): %3", channelName, channelId, text));
		return string.Format("ok sent %1 %2", channelName, text);
	}
}
