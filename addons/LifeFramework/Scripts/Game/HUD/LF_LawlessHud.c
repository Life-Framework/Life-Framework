//! #591: the local player's lawless-zone cue. A small red tag stays up while the controlled
//! character stands in a LAWLESS zone, and a toast fires on enter and on exit. Pure client side:
//! zone entities exist in every client's world, so LF_Zones.RuleAt answers locally with no RPC.
//! A late joiner or respawn already inside starts "outside" here, so the first tick reads as an
//! enter: the tag shows and the enter hint fires once.
class LF_LawlessState
{
	static const int NONE = 0;
	static const int ENTERED = 1;
	static const int LEFT = 2;

	//------------------------------------------------------------------------------------------------
	//! Pure rule: what the HUD does when the inside flag goes from wasInside to nowInside.
	static int Transition(bool wasInside, bool nowInside)
	{
		if (!wasInside && nowInside)
			return ENTERED;

		if (wasInside && !nowInside)
			return LEFT;

		return NONE;
	}
}

//------------------------------------------------------------------------------------------------
class LF_LawlessHud : SCR_InfoDisplay
{
	static const float CHECK_SECONDS = 0.25;
	static const float TOAST_SECONDS = 5.0;

	protected Widget m_wTag;
	protected bool m_bInside;
	protected float m_fCheck;

	//------------------------------------------------------------------------------------------------
	override event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);
		m_bInside = false;
		m_fCheck = CHECK_SECONDS;
		if (m_wRoot)
			m_wTag = m_wRoot.FindAnyWidget("Tag");

		if (!m_wTag)
			LF_Debug.Error("Zones", "lawless HUD tag widget missing from LF_LawlessTag.layout");
		else
			m_wTag.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	override event void UpdateValues(IEntity owner, float timeSlice)
	{
		if (!owner || !m_wTag)
			return;

		m_fCheck += timeSlice;
		if (m_fCheck < CHECK_SECONDS)
			return;

		m_fCheck = 0;
		bool inside = LF_Zones.IsLawless(owner.GetOrigin());
		int change = LF_LawlessState.Transition(m_bInside, inside);
		m_bInside = inside;
		if (change == LF_LawlessState.NONE)
			return;

		m_wTag.SetVisible(inside);
		LF_Debug.Info("Zones", string.Format("lawless HUD: local player %1 a lawless zone", LF_LawlessHud.Verb(change)));
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!manager)
			return;

		string message = "#LF-LawlessHud_Exit";
		if (change == LF_LawlessState.ENTERED)
			message = "#LF-LawlessHud_Enter";

		manager.ShowNotificationToast("#LF-LawlessHud_Title", message, TOAST_SECONDS, LF_ENotificationType.INFO);
	}

	//------------------------------------------------------------------------------------------------
	static string Verb(int change)
	{
		if (change == LF_LawlessState.ENTERED)
			return "entered";

		return "left";
	}
}
