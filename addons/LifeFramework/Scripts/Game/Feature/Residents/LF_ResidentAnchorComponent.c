//------------------------------------------------------------------------------------------------
//! A resident anchor (#2857, epic #991): a spot inside a place where a resident stands, such as the
//! `counter` of a shop, a `bench` or a `door`. The marker's position and facing are the spot. It
//! belongs to the place it is parented to, or the nearest place of the line's kind within 30 m, and
//! is found by LF_ResidentAnchors by anchor id; core script never names it or holds coordinates.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Residents", description: "Where a resident stands in a place: counter, bench, door.")]
class LF_ResidentAnchorComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ResidentAnchorComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Anchor id a timetable line names (counter, bench, door)")]
	protected string m_sAnchorId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sAnchorId.IsEmpty())
		{
			// A spawned anchor gets its id from SetAnchorId right after.
			return;
		}

		LF_ResidentAnchors.Register(owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetAnchorId()
	{
		return m_sAnchorId;
	}

	//------------------------------------------------------------------------------------------------
	//! Runtime setter for a spawned anchor (tests, wave 2 scenes); registers the anchor.
	void SetAnchorId(string id)
	{
		m_sAnchorId = id;
		if (id.IsEmpty())
		{
			LF_Debug.Error(LF_ResidentAnchors.FEATURE, "resident anchor given an empty anchor id; no timetable can find it");
			return;
		}

		if (GetOwner())
			LF_ResidentAnchors.Register(GetOwner());
	}
}
