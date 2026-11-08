// LF_QuestDefinition.c - one quest: objectives, prerequisites, reward, repeatability.
// BaseContainerProps so a quest giver prefab can carry a list of these directly
// (same pattern as EL_JobConfig on the job system).

[BaseContainerProps()]
class LF_QuestDefinition
{
	//! Stable string id: the persistence key and the previous-quest prerequisite reference.
	[Attribute(defvalue: "", UIWidgets.EditBox, "Stable quest id (persistence + prerequisite key)")]
	string m_sQuestId;

	[Attribute(defvalue: "", UIWidgets.EditBox, "Localization key for the title")]
	string m_sTitleKey;

	[Attribute(defvalue: "", UIWidgets.EditBox, "Localization key for the description")]
	string m_sDescriptionKey;

	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Icon for menus/HUD", "edds")]
	ResourceName m_sIcon;

	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Can be accepted again after the cooldown")]
	bool m_bRepeatable;

	[Attribute(defvalue: "0", UIWidgets.Slider, "Cooldown between turn-in and re-accept in seconds", "0 3600 10")]
	float m_fCooldownSeconds;

	[Attribute(defvalue: "0", UIWidgets.Slider, "Whole-quest time limit in seconds (0 = none)", "0 3600 10")]
	float m_fTimeLimitSeconds;

	//! Prerequisites.
	[Attribute(defvalue: "", UIWidgets.EditBox, "Quest id that must be turned in first (empty = none)")]
	string m_sRequiredPreviousQuestId;

	[Attribute(defvalue: "0", UIWidgets.EditBox, "Minimum player level (0 = none)")]
	int m_iMinLevel;

	[Attribute(defvalue: "0", UIWidgets.ComboBox, "Required job (UNEMPLOYED = none)", "", ParamEnumArray.FromEnum(EL_EJobType))]
	EL_EJobType m_eRequiredJob;

	[Attribute(defvalue: "", UIWidgets.EditBox, "Required faction key (empty = any)")]
	string m_sRequiredFaction;

	//! Reward, applied server-side on turn-in by the world layer.
	[Attribute(defvalue: "0", UIWidgets.EditBox, "Reward money")]
	int m_iRewardMoney;

	[Attribute(defvalue: "0", UIWidgets.EditBox, "Reward XP")]
	float m_fRewardXp;

	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Reward items (spawned into inventory on turn-in)", "et")]
	ref array<ResourceName> m_aRewardItems;

	[Attribute(defvalue: "", UIWidgets.EditBox, "Localization key the giver says on turn-in (empty = the generic turned-in hint)")]
	string m_sTurnInKey;

	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Hand over the holder's registered ID card on turn-in (the town clerk). A player who turned this in gets the card back on every spawn.")]
	bool m_bRewardIdCard;

	//! Chain steps. A chained step needs no trip back to the giver between errands.
	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Accepted on its own once the previous quest is turned in (chain step)")]
	bool m_bAutoAccept;

	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Turned in on its own wherever the player stands once complete (chain step)")]
	bool m_bAutoTurnIn;

	[Attribute(defvalue: "", UIWidgets.EditBox, "Objectives")]
	ref array<ref LF_QuestObjective> m_aObjectives;

	//------------------------------------------------------------------------------------------------
	//! Whether a giver's menu lists this quest. A quest whose previous quest is not turned in
	//! stays hidden (a chain shows one step at a time), and a one-off quest already turned in
	//! is not offered again (a returning player never sees the arrival chain).
	//! \param status This player's status for the quest.
	//! \param previousTurnedIn Whether m_sRequiredPreviousQuestId is turned in (ignored when empty).
	bool IsListed(LF_EQuestStatus status, bool previousTurnedIn)
	{
		if (!m_sRequiredPreviousQuestId.IsEmpty() && !previousTurnedIn)
			return false;

		if (status == LF_EQuestStatus.TURNED_IN && !m_bRepeatable)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool HasObjectives()
	{
		return m_aObjectives && m_aObjectives.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetObjectiveCount()
	{
		if (!m_aObjectives)
			return 0;
		return m_aObjectives.Count();
	}

	//------------------------------------------------------------------------------------------------
	LF_QuestObjective GetObjective(int index)
	{
		if (!m_aObjectives || index < 0 || index >= m_aObjectives.Count())
			return null;
		return m_aObjectives[index];
	}
}