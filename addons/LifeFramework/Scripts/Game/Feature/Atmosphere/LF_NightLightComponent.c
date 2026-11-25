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

	//! The child light of a world-streamed entity can appear after its first init. A miss is retried
	//! this often, this many times, before the "no LightEntity child" error is logged (#4831).
	static const int RETRY_MS = 500; // lf-fixed: a few frames; the child is built by the next second
	static const int RETRY_MAX = 6; // lf-fixed: 3 s in all, then the 30 s tick keeps looking

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

	[Attribute("1 0.72 0.42", UIWidgets.ColorPicker, "Colour of the light the component builds when the prefab's own child light is missing at run time")]
	protected vector m_vColor;

	[Attribute("3", UIWidgets.Auto, "LV of the built light")]
	protected float m_fLV;

	[Attribute("4", UIWidgets.Auto, "Radius in metres of the built light")]
	protected float m_fRadius;

	[Attribute("0", UIWidgets.CheckBox, "The built light is a spot light that points down (a lamp head); otherwise a point light")]
	protected bool m_bSpot;

	[Attribute("140", UIWidgets.Auto, "Cone angle in degrees of a built spot light")]
	protected float m_fConeAngle;

	[Attribute("0 0 0", UIWidgets.Coords, "Where the built light sits, in the owner's local space. The prefab's own child carries the same place (a fascia lamp stands in front of its plate)")]
	protected vector m_vLightOffset;

	[Attribute("0 -1 0", UIWidgets.Coords, "Where a built spot light points, in the owner's local space (down for a lamp head; toward the plate for a fascia lamp)")]
	protected vector m_vLightDirection;

	protected bool m_bLit;
	protected bool m_bApplied;
	protected bool m_bTicking;
	protected bool m_bRetryPending;
	protected bool m_bGaveUp;
	protected bool m_bBuilt;
	protected bool m_bForceRenders;
	protected int m_iMisses;

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

		if (m_bRetryPending)
			GetGame().GetCallqueue().Remove(Retry);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Retry()
	{
		m_bRetryPending = false;
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	//! True on a machine that builds the light (not a dedicated or console-app server). The test, which
	//! runs headless, forces it on to drive the miss path.
	protected bool Renders()
	{
		if (m_bForceRenders)
			return true;

		return RplSession.Mode() != RplMode.Dedicated && !System.IsConsoleApp();
	}

	//------------------------------------------------------------------------------------------------
	void ForceRenders(bool on)
	{
		m_bForceRenders = on;
	}

	//------------------------------------------------------------------------------------------------
	//! True once the retries ran out, no light could be built either, and the error was logged.
	bool GaveUp()
	{
		return m_bGaveUp;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab's child light never appeared and the component built its own.
	bool Built()
	{
		return m_bBuilt;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the built light sits in the owner's local space.
	vector LightOffset()
	{
		return m_vLightOffset;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a built spot light points in the owner's local space.
	vector LightDirection()
	{
		return m_vLightDirection;
	}

	//------------------------------------------------------------------------------------------------
	int Misses()
	{
		return m_iMisses;
	}

	//------------------------------------------------------------------------------------------------
	//! No child light yet. Look again shortly; after RETRY_MAX looks build the light from the attributes
	//! (a LightEntity child declared in a prefab is not there at run time on a client: #4831), and only
	//! when that fails log the error once.
	protected void OnMiss()
	{
		if (m_bGaveUp || m_bBuilt || !Renders())
			return;

		m_iMisses++;
		if (m_iMisses > LF_NightLight.RETRY_MAX)
		{
			if (BuildLight())
				return;

			m_bGaveUp = true;
			LF_Debug.Error(LF_NightLight.FEATURE, string.Format("%1 has no LightEntity child after %2 looks and the light could not be built; it stays dark", LF_Atmosphere.LabelOf(GetOwner()), m_iMisses));
			return;
		}

		if (m_bRetryPending)
			return;

		m_bRetryPending = true;
		GetGame().GetCallqueue().CallLater(Retry, LF_NightLight.RETRY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the light as a child of the owner, the way the other runtime lights are made. The next
	//! look finds it and applies the hour.
	protected bool BuildLight()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		LightType type = LightType.POINT;
		vector direction = vector.Forward;
		if (m_bSpot)
		{
			type = LightType.SPOT;
			direction = m_vLightDirection;
			direction.Normalize();
		}

		LightEntity light = LightEntity.CreateLight(type, 0, m_fRadius, Color.FromVector(m_vColor), m_fLV, m_vLightOffset, direction);
		if (!light)
			return false;

		light.SetLensFlareType(LightLensFlareType.Disabled);
		light.SetCastShadow(false);
		light.SetIntensityEVClip(-10);
		if (m_bSpot)
			light.SetConeAngle(m_fConeAngle);

		owner.AddChild(light, -1);
		m_bBuilt = true;
		LF_Debug.Info(LF_NightLight.FEATURE, string.Format("%1 had no LightEntity child after %2 looks; built its light", LF_Atmosphere.LabelOf(owner), m_iMisses));
		return true;
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
	//! finds no light looks again, then builds one (OnMiss).
	bool Evaluate(float hour)
	{
		bool wanted = LF_NightLight.IsOn(hour, m_fHourOn, m_fHourOff);
		LightEntity light = FindLight();
		if (!light)
		{
			OnMiss();
			light = FindLight();
		}

		if (light)
		{
			m_iMisses = 0;
			if (!m_bApplied || light.IsEnabled() != wanted)
			{
				light.SetEnabled(wanted);
				LF_Debug.Info(LF_NightLight.FEATURE, string.Format("%1 %2 at hour %3", LF_Atmosphere.LabelOf(GetOwner()), LF_NightLight.Word(wanted), hour.ToString(-1, 1)));
			}

			m_bApplied = true;
		}

		m_bLit = wanted;
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
