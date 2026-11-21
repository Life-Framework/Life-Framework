//------------------------------------------------------------------------------------------------
//! The cash van route (#274). The van parks at a stop, waits, and when no player is near moves to
//! the next stop and is stocked afresh. Base-game AI driving is not reliable enough to carry a
//! robbery, so the van is parked on a schedule: it is moved between stop markers, not driven. The
//! van and its stops are LF_Places entries under the route id (LF_PlaceComponent), found by
//! component and never by entity name (#1045).
//------------------------------------------------------------------------------------------------
class LF_CashVanRoute
{
	//! LF_Places kind of a route's van; the id is the route id.
	static const string PLACE_KIND_VAN = "CashVan";
	//! LF_Places kind of a route's stops; the id is the route id and the order (LF_Places.OrderedId).
	static const string PLACE_KIND_STOP = "CashVanStop";

	//------------------------------------------------------------------------------------------------
	//! \return The van of a route, or null when none is placed.
	static IEntity FindVan(string routeId)
	{
		return LF_Places.Find(PLACE_KIND_VAN, routeId);
	}

	//------------------------------------------------------------------------------------------------
	//! The stops of a route in order, from stop 0 to the first gap. \return how many were added.
	static int FindStops(string routeId, notnull array<IEntity> into)
	{
		int added = 0;
		for (int order = 0; order <= added; order++)
		{
			IEntity stop = LF_Places.Find(PLACE_KIND_STOP, LF_Places.OrderedId(routeId, order));
			if (!stop)
				break;

			into.Insert(stop);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Index of the stop after current, wrapping. A route of fewer than two stops stays put.
	static int NextStop(int current, int stopCount)
	{
		if (stopCount < 2)
			return 0;

		return (current + 1) % stopCount;
	}

	//------------------------------------------------------------------------------------------------
	//! The van holds while a player is close: nobody sees it vanish mid-robbery or mid-chase.
	static bool CanMove(int playersNear)
	{
		return playersNear <= 0;
	}

	//------------------------------------------------------------------------------------------------
	static int PlayersNear(vector at, float radius)
	{
		LF_PlayerControllerManagerCompat playerManager = LF_PlayerControllerManagerCompat.GetInstance();
		if (!playerManager)
			return 0;

		array<int> playerIds = {};
		playerManager.GetAllPlayerIds(playerIds);
		int near = 0;
		foreach (int playerId : playerIds)
		{
			PlayerController controller = playerManager.GetPlayerController(playerId);
			if (!controller)
				continue;

			IEntity body = controller.GetControlledEntity();
			if (body && vector.Distance(body.GetOrigin(), at) <= radius)
				near++;
		}

		return near;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the van at the next stop and stops it there. Update() moves its bounds too: without it a
	//! vehicle query still found the van at the old stop (the island car lot's pads read as taken).
	static void MoveVan(notnull IEntity van, vector transform[4])
	{
		van.SetTransform(transform);
		van.Update();
		Physics physics = van.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Sits on the van's cash box (the heist target). Moves the van and the box with it to the next stop
//! on the timer and restocks it there.
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Moves the cash van and its box between stops on a schedule.")]
class LF_CashVanRouteComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_CashVanRouteComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Route id: the id of the van and of the stops (LF_PlaceComponent kinds CashVan and CashVanStop, stops ordered from 0). The van starts at the first. Empty uses the entity name.")]
	protected string m_sRouteId;

	[Attribute("300", UIWidgets.Auto, desc: "Seconds the van waits at a stop before it may move on.")]
	protected float m_fDwellSeconds;

	[Attribute("40", UIWidgets.Auto, desc: "The van does not move while a player is this close, in metres.")]
	protected float m_fClearMetres;

	//------------------------------------------------------------------------------------------------
	//! Id of the route this box drives. The box is its own entity, not a child of the van: a
	//! replicated child under a replicated vehicle makes the client item table disagree with the server.
	string GetRouteId()
	{
		if (!m_sRouteId.IsEmpty())
			return m_sRouteId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}

	[Attribute("1.35 0 -1.4", UIWidgets.Coords, desc: "Where the box stands, in the van's local space: on the ground at the kerb-side door. It used to hang off the rear step over the road.")]
	protected vector m_vBoxOffset;

	[Attribute("90", UIWidgets.Auto, desc: "The box's heading relative to the van, in degrees. 90 faces it out from the kerb side.")]
	protected float m_fBoxYaw;

	protected int m_iStop;
	//! Errors this route has logged for a route it cannot drive (no van, or fewer than two stops).
	protected int m_iNoVanErrors;
	protected bool m_bDegraded;

	//------------------------------------------------------------------------------------------------
	//! Errors logged by this route for a missing van or stops. A degraded route logs once, then none.
	int NoVanErrors()
	{
		return m_iNoVanErrors;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs once and stops the timer: a route that cannot drive its van does not retry each dwell.
	protected void Degrade(string message)
	{
		m_bDegraded = true;
		m_iNoVanErrors++;
		LF_Debug.Error("Bank", message);
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().RemoveByName(this, "Step");
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		if (m_fDwellSeconds < 5)
			m_fDwellSeconds = 5;

		int delayMs = m_fDwellSeconds * 1000;
		GetGame().GetCallqueue().CallLaterByName(this, "Step", delayMs, true);
		LF_Debug.Log("Bank", string.Format("cash van route %1 dwell=%2", GetRouteId(), m_fDwellSeconds));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().RemoveByName(this, "Step");

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server timer: move to the next stop and restock, unless a player is near.
	void Step()
	{
		if (m_bDegraded)
			return;

		IEntity door = GetOwner();
		if (!door)
			return;

		string routeId = GetRouteId();
		IEntity van = LF_CashVanRoute.FindVan(routeId);
		if (!van)
		{
			Degrade(string.Format("cash van route: van %1 is not placed", routeId));
			return;
		}

		array<IEntity> stops = {};
		LF_CashVanRoute.FindStops(routeId, stops);
		if (stops.Count() < 2)
		{
			Degrade(string.Format("cash van route %1 has %2 stop(s), needs two", routeId, stops.Count()));
			return;
		}

		int near = LF_CashVanRoute.PlayersNear(van.GetOrigin(), m_fClearMetres);
		if (!LF_CashVanRoute.CanMove(near))
		{
			LF_Debug.Info("Bank", string.Format("cash van holds: %1 player(s) within %2 m", near, m_fClearMetres));
			return;
		}

		int next = LF_CashVanRoute.NextStop(m_iStop, stops.Count());
		IEntity stop = stops[next];

		vector transform[4];
		stop.GetWorldTransform(transform);
		LF_CashVanRoute.MoveVan(van, transform);
		// The box goes with the van: set down at its kerb-side door from the van's new pose, on the surface there.
		door.SetOrigin(LF_Dispatch.RestingSpot(van.CoordToParent(m_vBoxOffset) + "0 0.5 0", door));
		door.SetAngles(van.GetAngles() + Vector(0, m_fBoxYaw, 0));

		m_iStop = next;
		LF_BankVaultComponent vault = LF_Component<LF_BankVaultComponent>.Find(door);
		if (vault)
			LF_Bank.Restock(vault.GetVaultId());

		LF_Debug.Info("Bank", string.Format("cash van moved to stop %1 of %2 and restocked", next, routeId));
	}
}
