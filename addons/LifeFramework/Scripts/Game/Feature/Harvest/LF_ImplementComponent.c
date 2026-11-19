// LF_ImplementComponent.c: a towed farm implement (#3284, harvester spike H1). The reaper prefab carries it. The
// server spawns the implement as a rigid child of a hitch-list vehicle (LF_ImplementHitch); the entity hierarchy
// replicates through the RplComponents, so every client and a late joiner get it under the same vehicle. The
// implement has no physics body, so it never drags or flips the vehicle. The hopper is the one replicated number
// the cut tick (#3289) fills and the prompt shows.

[ComponentEditorProps(category: "LifeFramework/Harvest", description: "A hitched farm implement: bar width and hopper")]
class LF_ImplementComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ImplementComponent : ScriptComponent
{
	//! How often a machine checks whether the towing vehicle moved far enough to log the implement's place again.
	protected static const int WATCH_MS = 2000; // lf-fixed: a log cadence for the replication proof, not gameplay
	//! Metres the towing vehicle moves between two "implement follows" lines on one machine.
	protected static const float LOG_STEP_M = 10; // lf-fixed: a log cadence for the replication proof, not gameplay

	[Attribute("3", UIWidgets.EditBox, "Metres across the cutting bar")]
	protected float m_fWidth;

	[RplProp()]
	protected int m_iHopperKg;

	protected vector m_vLastLoggedParentPos;
	protected bool m_bLogged;

	//------------------------------------------------------------------------------------------------
	//! The implement on an entity, or null.
	static LF_ImplementComponent Of(IEntity entity)
	{
		if (!entity)
			return null;

		return LF_ImplementComponent.Cast(entity.FindComponent(LF_ImplementComponent));
	}

	//------------------------------------------------------------------------------------------------
	float GetWidth()
	{
		return m_fWidth;
	}

	//------------------------------------------------------------------------------------------------
	int GetHopperKg()
	{
		return m_iHopperKg;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: set the hopper and replicate it. Callers throttle (the cut tick changes it a few times a second at most).
	void SetHopperKg(int kg)
	{
		if (kg < 0)
			kg = 0;

		if (kg == m_iHopperKg)
			return;

		m_iHopperKg = kg;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;

		// The parent is set by the time the next frame runs, on the server (spawned with it) and on a proxy
		// (the replicated hierarchy, a late joiner's snapshot included).
		GetGame().GetCallqueue().CallLater(Watch, 0, false);
		GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Watch);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: one log line when the implement appears and each time its vehicle has moved LOG_STEP_M.
	//! The two-client and late-join proof reads these lines from each client's log.
	protected void Watch()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		IEntity parent = owner.GetParent();
		if (!parent)
		{
			if (!m_bLogged)
			{
				m_bLogged = true;
				LF_Debug.Warn("Harvest", "implement " + LF_ImplementHitch.RplIdOf(owner) + " has no towing vehicle on this machine");
			}

			return;
		}

		vector parentPos = parent.GetOrigin();
		if (m_bLogged && vector.Distance(parentPos, m_vLastLoggedParentPos) < LOG_STEP_M)
			return;

		string what = "follows";
		if (!m_bLogged)
			what = "present";

		m_bLogged = true;
		m_vLastLoggedParentPos = parentPos;
		vector local[4];
		owner.GetLocalTransform(local);
		// Behind = the implement's position in the vehicle's frame has a negative Z (the vehicle faces +Z).
		vector inVehicle = parent.CoordToLocal(owner.GetOrigin());
		LF_Debug.Info("Harvest", string.Format("implement %1 on %2 (rpl %3, proxy %4): local %5, vehicle at %6, implement at %7, behind %8",
			what, LF_ImplementHitch.Describe(parent), LF_ImplementHitch.RplIdOf(owner), IsProxy(), local[3], parentPos, owner.GetOrigin(), inVehicle[2] < 0));
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsProxy()
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		return rpl && rpl.IsProxy();
	}
}
