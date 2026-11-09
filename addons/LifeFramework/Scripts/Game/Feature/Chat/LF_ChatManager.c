//------------------------------------------------------------------------------------------------
//! Game mode component: keeps the chat hidden until a player opens it.
[ComponentEditorProps(category: "EveronLife/Feature/Chat")]
class LF_ChatManagerComponentClass : ScriptComponentClass
{
}

class LF_ChatManagerComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	void ToggleChat(bool active)
	{
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