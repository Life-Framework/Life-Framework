//------------------------------------------------------------------------------------------------
//! Game mode component: keeps the chat hidden until a player opens it.
[ComponentEditorProps(category: "EveronLife/Feature/Chat")]
class LF_ChatManagerComponentClass : ScriptComponentClass
{
}

class LF_ChatManagerComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! Chat off (#1076): the base game's chat panels stay as they are.
	static bool HidesPanels()
	{
		return LF_Systems.IsEnabled("Chat");
	}

	//------------------------------------------------------------------------------------------------
	void ToggleChat(bool active)
	{
		if (!HidesPanels())
			return;

		SCR_ChatPanelManager chatManager = SCR_ChatPanelManager.GetInstance();
		if (!chatManager)
			return;
		chatManager.HideAllChatPanels();
	}

	//------------------------------------------------------------------------------------------------
	void LateInit()
	{
		ToggleChat(false);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		GetGame().GetCallqueue().CallLater(LateInit, 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		// INIT only: there is no EOnFrame, so a FRAME mask would just schedule an empty per-frame call.
		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, true);
	}
}