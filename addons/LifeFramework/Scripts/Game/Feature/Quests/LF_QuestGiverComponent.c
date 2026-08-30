// LF_QuestGiverComponent.c - quest giver on an NPC or a board prop. Holds the
// quest definitions in the prefab (same pattern as EL_TraderManagerComponent's
// tradable items) and registers them with the manager on server init.

[ComponentEditorProps(category: "LifeFramework/Feature/Quests", description: "Quest giver on an NPC or board")]
class LF_QuestGiverComponentClass : ScriptComponentClass
{
}

class LF_QuestGiverComponent : ScriptComponent
{
	//! Unique id referenced by TALK objectives and logs.
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Unique giver id")]
	string m_sGiverId;

	//! Quests this giver offers.
	[Attribute("", UIWidgets.Object, "Quests offered by this giver")]
	ref array<ref LF_QuestDefinition> m_aQuests;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		LF_QuestManager.GetInstance().RegisterGiver(this);
	}

	//------------------------------------------------------------------------------------------------
	string GetGiverId()
	{
		return m_sGiverId;
	}

	//------------------------------------------------------------------------------------------------
	array<ref LF_QuestDefinition> GetQuests()
	{
		return m_aQuests;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the quest menu on the interacting player's client. The menu's open
	//! handshake (AskOpenGiver) is what reports the TALK objective server-side.
	void OpenQuestMenu(IEntity user)
	{
		if (!user)
			return;

		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		LF_QuestMenu menu = LF_QuestMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.QuestMenu));
		if (menu)
			menu.SetGiver(this, user);
	}
}
