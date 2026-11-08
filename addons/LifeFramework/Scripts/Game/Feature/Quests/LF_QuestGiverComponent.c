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

	//! Name shown on the quest menu title. A localization key (#...). Empty falls back to the id.
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Display name shown on the quest menu (a localization key). Empty shows the giver id.")]
	string m_sDisplayName;

	//! Quests this giver offers.
	[Attribute("", UIWidgets.Object, "Quests offered by this giver")]
	ref array<ref LF_QuestDefinition> m_aQuests;

	//! In person: talking to the giver hands over what is done and takes on the next errand,
	//! with the giver's lines as toasts. No quest menu (the town clerk).
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Talking moves this giver's quests along in person (turn in, take the next one). No quest menu.")]
	bool m_bConverse;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Localization key the giver says when there is nothing left to hand out (converse givers)")]
	string m_sIdleKey;

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
	//! Title for the quest menu: the display name key when set, else the giver id.
	string GetDisplayName()
	{
		if (!m_sDisplayName.IsEmpty())
			return m_sDisplayName;

		return m_sGiverId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsConverse()
	{
		return m_bConverse;
	}

	//------------------------------------------------------------------------------------------------
	string GetIdleKey()
	{
		return m_sIdleKey;
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
