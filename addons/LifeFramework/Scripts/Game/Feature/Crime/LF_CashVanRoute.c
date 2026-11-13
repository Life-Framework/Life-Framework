//------------------------------------------------------------------------------------------------
//! The cash van route (#274). The van parks at a stop, waits, and when no player is near moves to
//! the next stop and is stocked afresh. Base-game AI driving is not reliable enough to carry a
//! robbery, so the van is parked on a schedule: it is moved between stop markers, not driven. The
//! stops are world entities named in m_sStops.
//------------------------------------------------------------------------------------------------
class LF_CashVanRoute
{
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
	[Attribute("", UIWidgets.EditBox, desc: "World entity names of the stops, comma separated, in route order. The van starts at the first.")]
	protected string m_sStops;

	[Attribute("300", UIWidgets.Auto, desc: "Seconds the van waits at a stop before it may move on.")]
	protected float m_fDwellSeconds;

	[Attribute("40", UIWidgets.Auto, desc: "The van does not move while a player is this close, in metres.")]
	protected float m_fClearMetres;

	[Attribute("", UIWidgets.EditBox, desc: "World entity name of the van; set on the placed box (the pack names its van). The box is its own entity, not a child: a replicated child under a replicated vehicle makes the client item table disagree with the server.")]
	protected string m_sVanEntity;

	//------------------------------------------------------------------------------------------------
	//! World entity name of the van this box drives.
	string GetVanEntityName()
	{
		return m_sVanEntity;
	}

	[Attribute("1.35 0 -1.4", UIWidgets.Coords, desc: "Where the box stands, in the van's local space: on the ground at the kerb-side door. It used to hang off the rear step over the road.")]
	protected vector m_vBoxOffset;

	[Attribute("90", UIWidgets.Auto, desc: "The box's heading relative to the van, in degrees. 90 faces it out from the kerb side.")]
	protected float m_fBoxYaw;

	protected int m_iStop;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		if (m_fDwellSeconds < 5)
			m_fDwellSeconds = 5;

		int delayMs = m_fDwellSeconds * 1000;
		GetGame().GetCallqueue().CallLater(Step, delayMs, true);
		LF_Debug.Log("Bank", string.Format("cash van route %1 stops=[%2] dwell=%3", owner.GetName(), m_sStops, m_fDwellSeconds));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Step);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server timer: move to the next stop and restock, unless a player is near.
	protected void Step()
	{
		IEntity door = GetOwner();
		if (!door)
			return;

		IEntity van = GetGame().GetWorld().FindEntityByName(m_sVanEntity);
		if (!van)
		{
			LF_Debug.Error("Bank", string.Format("cash van route: van %1 is not in the world", m_sVanEntity));
			return;
		}

		array<string> stops = {};
		m_sStops.Split(",", stops, true);
		if (stops.Count() < 2)
			return;

		int near = LF_CashVanRoute.PlayersNear(van.GetOrigin(), m_fClearMetres);
		if (!LF_CashVanRoute.CanMove(near))
		{
			LF_Debug.Info("Bank", string.Format("cash van holds: %1 player(s) within %2 m", near, m_fClearMetres));
			return;
		}

		int next = LF_CashVanRoute.NextStop(m_iStop, stops.Count());
		IEntity stop = GetGame().GetWorld().FindEntityByName(stops[next]);
		if (!stop)
		{
			LF_Debug.Error("Bank", string.Format("cash van route: stop %1 is not in the world", stops[next]));
			return;
		}

		vector transform[4];
		stop.GetWorldTransform(transform);
		van.SetTransform(transform);
		// The box goes with the van: set down at its kerb-side door from the van's new pose, on the surface there.
		door.SetOrigin(LF_Dispatch.RestingSpot(van.CoordToParent(m_vBoxOffset) + "0 0.5 0", door));
		door.SetAngles(van.GetAngles() + Vector(0, m_fBoxYaw, 0));
		Physics physics = van.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}

		m_iStop = next;
		LF_BankVaultComponent vault = LF_Component<LF_BankVaultComponent>.Find(door);
		if (vault)
			LF_Bank.Restock(vault.GetVaultId());

		LF_Debug.Info("Bank", string.Format("cash van moved to %1 and restocked", stops[next]));
	}
}
