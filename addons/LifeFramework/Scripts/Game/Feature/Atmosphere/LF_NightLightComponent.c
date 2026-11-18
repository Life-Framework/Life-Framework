//------------------------------------------------------------------------------------------------
//! A light that comes on at dusk and goes off at dawn: a window glowing in a house, a lamp head on
//! a post whose vanilla light does not switch itself on. Put it on the prefab root; the LightEntity
//! it drives is a child of that entity. Every 30 s it reads the local world hour and enables the
//! child light between m_fHourOn and m_fHourOff (the window may wrap past midnight).
//!
//! Local to machines that render: no replication, no RPC, no state. A dedicated server does not
//! tick it (no one sees the light there); the clock on each client is the replicated world time.
//! The rule is LF_NightLight.IsOn, a pure function the test drives at fixed hours.
//------------------------------------------------------------------------------------------------
class LF_NightLight
{
	static const string FEATURE = "NightLight";

	//! Milliseconds between two looks at the clock.
	static const int CHECK_MS = 30000; // lf-fixed: a dusk look every 30 s costs nothing; the card fixes it, no server setting

	//------------------------------------------------------------------------------------------------
	//! True when `hour` (0 to 24) is inside [hourOn, hourOff). The window wraps past midnight when
	//! hourOn is later than hourOff (18 to 6). Equal bounds mean always on.
	static bool IsOn(float hour, float hourOn, float hourOff)
	{
		return LF_AtmosphereMix.InHours(hour, hourOn, hourOff);
	}

	//------------------------------------------------------------------------------------------------
	static string Word(bool on)
	{
		if (on)
			return "on";

		return "off";
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Atmosphere", description: "Enables the child light between two hours of the local world time. Client only.")]
class LF_NightLightComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_NightLightComponent : ScriptComponent
{
	[Attribute("18", UIWidgets.Auto, "The light comes on at this hour of the day (0 to 24)")]
	protected float m_fHourOn;

	[Attribute("6", UIWidgets.Auto, "The light goes off at this hour. A window may wrap past midnight (18 to 6)")]
	protected float m_fHourOff;

	protected bool m_bLit;
	protected bool m_bApplied;
	protected bool m_bTicking;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// A dedicated server renders nothing: no light to switch, no clock to read.
		if (RplSession.Mode() == RplMode.Dedicated || System.IsConsoleApp())
			return;

		// The child light exists once the entity has initialised; the first look is a frame later.
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (m_bTicking)
			return;

		m_bTicking = true;
		Tick();
		GetGame().GetCallqueue().CallLater(Tick, LF_NightLight.CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bTicking)
			GetGame().GetCallqueue().Remove(Tick);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!GetOwner())
			return;

		Evaluate(LF_Atmosphere.HourNow());
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the rule for `hour` to the child light and returns the state it should have. Also the
	//! entry for the test, which has no client clock to wait on. A headless server builds no
	//! LightEntity, so there the state is kept and no light is touched; a machine that renders and
	//! finds no light says so once.
	bool Evaluate(float hour)
	{
		bool wanted = LF_NightLight.IsOn(hour, m_fHourOn, m_fHourOff);
		LightEntity light = FindLight();
		if (!light)
		{
			bool renders = RplSession.Mode() != RplMode.Dedicated && !System.IsConsoleApp();
			if (renders && !m_bApplied)
				LF_Debug.Error(LF_NightLight.FEATURE, string.Format("%1 has no LightEntity child; it stays dark", LF_Atmosphere.LabelOf(GetOwner())));
		}
		else if (!m_bApplied || light.IsEnabled() != wanted)
		{
			light.SetEnabled(wanted);
			LF_Debug.Info(LF_NightLight.FEATURE, string.Format("%1 %2 at hour %3", LF_Atmosphere.LabelOf(GetOwner()), LF_NightLight.Word(wanted), hour.ToString(-1, 1)));
		}

		m_bLit = wanted;
		m_bApplied = true;
		return wanted;
	}

	//------------------------------------------------------------------------------------------------
	//! The first LightEntity among the owner's children, or null.
	LightEntity FindLight()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		IEntity child = owner.GetChildren();
		while (child)
		{
			LightEntity light = LightEntity.Cast(child);
			if (light)
				return light;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	bool IsLit()
	{
		return m_bLit;
	}
}
