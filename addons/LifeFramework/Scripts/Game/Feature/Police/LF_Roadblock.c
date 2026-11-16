//------------------------------------------------------------------------------------------------
//! Police roadblock kit (#1941, chapter On patrol #1014, #646). An on-duty officer carries one
//! folded kit (Prefabs/Props/Roleplay/Roadblock/Roadblock_Kit.et) and deploys it as a composition
//! (#1940, LF_PlacementCompositions): a barrier across the road with two cones either side, each
//! piece on the ground, placed by one server-checked confirm where the officer's ghost showed it,
//! and removed by one pick-up that gives the kit back. Only on-duty police deploy or pick it up.
//! A vehicle that arrives at the set moving and stops beside it is logged ([LFDebug:Roadblock]).
//! Server: the checks, the deploy, the pick-up, the stop log. Client: the ghost (LF_RoadblockPreview).
//! Nothing persists: a roadblock is temporary (docs/roadmap/building-foundation.md section 5).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! One vehicle near a set: whether it was seen moving and whether its stop is already logged.
class LF_RoadblockTrack
{
	EntityID m_Vehicle;
	bool m_bMoving;
	bool m_bLogged;
}

//------------------------------------------------------------------------------------------------
//! A deployed roadblock: its pieces (the placed composition), who laid it, and the vehicles near it.
class LF_RoadblockSet
{
	//! A vehicle at least this fast (m/s) counts as arriving at the roadblock.
	static const float MOVING_SPEED = 3; // lf-fixed: separates a car rolling up from one parked, not a gameplay knob
	//! A vehicle no faster than this (m/s) is stopped.
	static const float STOPPED_SPEED = 0.5; // lf-fixed: a car at rest still creeps a little
	//! A vehicle this close (metres) to the set's middle is stopped at it.
	static const float STOP_RADIUS = 9; // lf-fixed: geometry, the set is about 7 m wide plus a car's length

	ref LF_PlacedComposition m_Placed;
	string m_sDeployer;
	vector m_vCentre;
	int m_iStopped;
	protected ref array<ref LF_RoadblockTrack> m_aTracks = {};

	//------------------------------------------------------------------------------------------------
	//! One sample of a vehicle: its speed (m/s) and its distance (metres) from the set's middle.
	//! \return true when this sample is the one that logged a stop (a vehicle seen moving near the set
	//! that is now stopped, once per approach).
	bool Observe(EntityID vehicle, float speed, float distance, string label)
	{
		if (distance > STOP_RADIUS)
			return false;

		LF_RoadblockTrack track = TrackFor(vehicle);
		if (speed >= MOVING_SPEED)
		{
			track.m_bMoving = true;
			track.m_bLogged = false;
			return false;
		}

		if (speed > STOPPED_SPEED || !track.m_bMoving || track.m_bLogged)
			return false;

		track.m_bLogged = true;
		m_iStopped++;
		LF_Debug.Info(LF_Roadblocks.FEATURE, string.Format("%1 stopped at the roadblock of %2 (%3 m from it)", label, m_sDeployer, distance));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RoadblockTrack TrackFor(EntityID vehicle)
	{
		foreach (LF_RoadblockTrack track : m_aTracks)
		{
			if (track.m_Vehicle == vehicle)
				return track;
		}

		LF_RoadblockTrack added = new LF_RoadblockTrack();
		added.m_Vehicle = vehicle;
		m_aTracks.Insert(added);
		return added;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Roadblocks
{
	static const string FEATURE = "Roadblock";
	static const ResourceName KIT = "{A317B2EFCFB528F2}Prefabs/Props/Roleplay/Roadblock/Roadblock_Kit.et";
	static const ResourceName BARRIER = "{23BFE33133831ECE}Prefabs/Props/Roleplay/Roadblock/Roadblock_Barrier.et";
	static const ResourceName CONE = "{712B4AAB7B82396A}Prefabs/Props/Roleplay/Roadblock/Roadblock_Cone.et";
	//! Server tick of the stop check.
	static const int TICK_MS = 500; // lf-fixed: sampling period of a cosmetic log, not a gameplay knob

	protected static ref array<ref LF_RoadblockSet> s_aSets = {};
	protected static ref array<EntityID> s_aFound = {};
	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	//! The set the kit lays: x across the road, z along it (the way the officer faces when they
	//! deploy). The barrier in the middle, two cones either side, the outer ones drawn back.
	static LF_PlacementComposition Composition()
	{
		LF_PlacementComposition composition = new LF_PlacementComposition();
		composition.m_sName = "roadblock";
		composition.m_eWho = LF_EPlacementWho.ON_DUTY_POLICE;
		// The barrier mesh is 2.2 m long along its own z: turned a quarter so it spans the road.
		composition.AddPiece(BARRIER, "0 0 0", 90, "-0.25 0.1 -1.1", "0.25 1.1 1.1");
		AddCone(composition, "-1.9 0 0");
		AddCone(composition, "1.9 0 0");
		AddCone(composition, "-3.3 0 -1.2");
		AddCone(composition, "3.3 0 -1.2");
		return composition;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddCone(LF_PlacementComposition composition, vector offset)
	{
		composition.AddPiece(CONE, offset, 0, "-0.2 0.05 -0.2", "0.2 0.6 0.2");
	}

	//------------------------------------------------------------------------------------------------
	static string LastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Roadblocks standing on this machine.
	static int Count()
	{
		return s_aSets.Count();
	}

	//------------------------------------------------------------------------------------------------
	static LF_RoadblockSet Find(IEntity piece)
	{
		LF_PlacedComposition placed = LF_PlacementCompositions.Find(piece);
		if (!placed)
			return null;

		foreach (LF_RoadblockSet block : s_aSets)
		{
			if (block.m_Placed == placed)
				return block;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The spot an officer's roadblock would lie at from where they stand: the kit's own plan (the
	//! spot ahead of them, turned across the road), with the set's heading along the road, i.e. a
	//! quarter turn from the kit's across-the-road axis. `spot` rows: [2] heading, [3] origin.
	//! \return the kit plan's verdict (the composition judges its own pieces again).
	static LF_EPlacementVerdict SpotFor(IEntity officer, IEntity kit, out vector spot[4])
	{
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(kit);
		if (!placeable)
		{
			LF_Debug.Error(FEATURE, "the kit has no LF_PlaceableComponent; no spot to plan");
			Math3D.MatrixIdentity4(spot);
			return LF_EPlacementVerdict.NO_GROUND;
		}

		vector transform[4];
		LF_PlacementProbe probe = new LF_PlacementProbe();
		LF_EPlacementVerdict verdict = LF_PlacementService.Plan(officer, placeable, kit, transform, probe);
		SpotAt(transform[3], transform[2], spot);
		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	//! The set's transform from the kit plan's origin and its across-the-road axis.
	static void SpotAt(vector origin, vector across, out vector spot[4])
	{
		vector flat = LF_PlacementGeometry.Flat(across);
		vector heading = Vector(-flat[2], 0, flat[0]);
		LF_PlacementGeometry.Transform(origin - heading * 0.5, origin + heading * 0.5, spot);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer lays the kit `kit` carries at `proposal`, the spot their ghost showed
	//! (origin and heading). LF_PlacementCompositions.Confirm checks it again; the kit is used up.
	//! \return the placed set, or null with the refusal logged.
	static LF_PlacedComposition Deploy(IEntity officer, IEntity kit, vector proposal[4])
	{
		if (!Replication.IsServer() || !officer || !kit)
			return Refuse(officer, "deploy refused: no officer or kit, or not the server", "");

		if (!LF_PoliceUtils.IsOnDutyCharacter(officer))
			return Refuse(officer, string.Format("deploy refused: %1 is not on-duty police", LF_PoliceRanks.ResolveUid(officer)), "#LF-Roadblock_PoliceOnly");

		LF_PlacedComposition placed = LF_PlacementCompositions.Confirm(officer, Composition(), proposal, kit);
		if (!placed)
		{
			s_sLastRefusal = "deploy " + LF_PlacementCompositions.LastRefusal();
			return null;
		}

		LF_RoadblockSet block = new LF_RoadblockSet();
		block.m_Placed = placed;
		block.m_sDeployer = LF_PoliceRanks.ResolveUid(officer);
		block.m_vCentre = proposal[3];
		s_aSets.Insert(block);
		if (s_aSets.Count() == 1)
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);

		SCR_EntityHelper.DeleteEntityAndChildren(kit);
		LF_Debug.Info(FEATURE, string.Format("deployed by %1 at %2 heading %3 (%4 pieces)", block.m_sDeployer, proposal[3], proposal[2], placed.m_aIds.Count()));
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Deploy at the server's own plan, for a request that carried no proposal (a server-side
	//! perform, the tests).
	static LF_PlacedComposition DeployOwnSpot(IEntity officer, IEntity kit)
	{
		vector spot[4];
		if (officer && kit)
			SpotFor(officer, kit, spot);

		return Deploy(officer, kit, spot);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer within reach of any piece folds the whole set up; the kit goes
	//! into their inventory, or onto the ground at their feet when it is full.
	//! \return how many pieces were removed (0 when refused).
	static int PickUp(IEntity officer, IEntity piece)
	{
		if (!Replication.IsServer() || !officer || !piece)
		{
			Refuse(officer, "pick-up refused: no officer or piece, or not the server", "");
			return 0;
		}

		if (!LF_PoliceUtils.IsOnDutyCharacter(officer))
		{
			Refuse(officer, string.Format("pick-up refused: %1 is not on-duty police", LF_PoliceRanks.ResolveUid(officer)), "#LF-Roadblock_PoliceOnly");
			return 0;
		}

		LF_RoadblockSet block = Find(piece);
		if (!block)
		{
			Refuse(officer, string.Format("pick-up refused: %1 belongs to no roadblock", LF_PlacementRules.Describe(piece)), "");
			return 0;
		}

		int removed = LF_PlacementCompositions.PickUp(officer, piece);
		if (removed == 0)
		{
			s_sLastRefusal = "pick-up " + LF_PlacementCompositions.LastRefusal();
			return 0;
		}

		s_aSets.RemoveItem(block);
		if (s_aSets.IsEmpty())
			GetGame().GetCallqueue().Remove(Tick);

		int added = LF_InventoryUtils.AddAmount(officer, KIT, 1, true);
		LF_Debug.Info(FEATURE, string.Format("picked up by %1: %2 pieces removed, kit back (%3)", LF_PoliceRanks.ResolveUid(officer), removed, added));
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! How many vehicle stops the sets on this machine have logged.
	static int StoppedCount()
	{
		int total = 0;
		foreach (LF_RoadblockSet block : s_aSets)
		{
			total += block.m_iStopped;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one sample of every vehicle near each set.
	static void Tick()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		for (int i = s_aSets.Count() - 1; i >= 0; i--)
		{
			LF_RoadblockSet block = s_aSets[i];
			s_aFound.Clear();
			world.QueryEntitiesBySphere(block.m_vCentre, LF_RoadblockSet.STOP_RADIUS, CollectVehicle, null, EQueryEntitiesFlags.ALL);
			foreach (EntityID id : s_aFound)
			{
				IEntity vehicle = world.FindEntityByID(id);
				if (!vehicle)
					continue;

				float speed;
				Physics physics = vehicle.GetPhysics();
				if (physics)
					speed = physics.GetVelocity().Length();

				block.Observe(vehicle.GetID(), speed, vector.Distance(vehicle.GetOrigin(), block.m_vCentre), LF_SpikeStrips.Describe(vehicle));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectVehicle(IEntity entity)
	{
		if (Vehicle.Cast(entity))
			s_aFound.Insert(entity.GetID());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_aSets.Clear();
		s_aFound.Clear();
		s_sLastRefusal = "";
		LF_RoadblockPreview.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlacedComposition Refuse(IEntity officer, string message, string toastKey)
	{
		s_sLastRefusal = message;
		LF_Debug.Warn(FEATURE, message);
		if (toastKey.IsEmpty() || !officer || !GetGame() || !GetGame().GetPlayerManager())
			return null;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", toastKey, 3.0, LF_ENotificationType.WARNING);

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Client: the ghost of the roadblock an officer is about to lay. While the kit's Deploy action is
//! selected, one ghost of the whole set lies where the server would put it: green when every piece
//! stands on valid ground, red when not (LF_PlacementCompositionGhost). It runs the server's own plan
//! every TICK_MS, so the colour is the answer the server will give, and Deploy sends the spot it
//! shows as the proposal (LF_RoadblockDeployAction.OnSaveActionData). Behind the same flag as the
//! spike strip's ghost: only while the game mode has dev tools on.
class LF_RoadblockPreview
{
	static const int TICK_MS = 50; // lf-fixed: refresh period of a cosmetic ghost, not a gameplay knob

	protected static EntityID s_KitId = EntityID.INVALID;
	protected static bool s_bRunning;
	protected static LF_EPlacementVerdict s_eVerdict = LF_EPlacementVerdict.NO_GROUND;
	protected static string s_sReason;
	protected static bool s_bSpot;
	protected static vector s_vSpotOrigin;
	protected static vector s_vSpotHeading;
	protected static ref LF_PlacementComposition s_Composition;

	//------------------------------------------------------------------------------------------------
	static bool Enabled()
	{
		return LF_SpikeStripPreview.Enabled();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlacementComposition Set()
	{
		if (!s_Composition)
			s_Composition = LF_Roadblocks.Composition();

		return s_Composition;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: starts (or retargets) the ghost for the kit `kit`. Ends by itself when the kit is gone.
	static LF_EPlacementVerdict BeginFor(IEntity kit)
	{
		if (!LF_Component<LF_PlaceableComponent>.Find(kit))
		{
			LF_Debug.Error(LF_Roadblocks.FEATURE, "preview skipped: the kit has no LF_PlaceableComponent");
			return LF_EPlacementVerdict.NO_GROUND;
		}

		s_KitId = kit.GetID();
		if (!s_bRunning)
		{
			s_bRunning = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
			LF_Debug.Info(LF_Roadblocks.FEATURE, "preview on");
		}

		Tick();
		return s_eVerdict;
	}

	//------------------------------------------------------------------------------------------------
	static void End()
	{
		if (s_bRunning)
			GetGame().GetCallqueue().Remove(Tick);

		if (s_bRunning || LF_PlacementCompositionGhost.Count() > 0)
			LF_Debug.Info(LF_Roadblocks.FEATURE, "preview off");

		s_bRunning = false;
		s_bSpot = false;
		s_KitId = EntityID.INVALID;
		LF_PlacementCompositionGhost.Hide();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the ghost of the set at an explicit spot and leaves it there (the harness, for a picture).
	//! \return the plan's verdict.
	static LF_EPlacementVerdict ShowAt(IEntity officer, vector origin, vector heading)
	{
		vector spot[4];
		LF_PlacementGeometry.Transform(origin - heading * 0.5, origin + heading * 0.5, spot);
		return Update(officer, null, spot);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning()
	{
		return s_bRunning;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EPlacementVerdict GetVerdict()
	{
		return s_eVerdict;
	}

	//------------------------------------------------------------------------------------------------
	static string GetReason()
	{
		return s_sReason;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: where the ghost of `kit` lies now (origin and heading), when one is shown for it.
	static bool CurrentSpot(IEntity kit, out vector origin, out vector heading)
	{
		if (!kit || !s_bSpot || s_KitId != kit.GetID() || LF_PlacementCompositionGhost.Count() == 0)
			return false;

		origin = s_vSpotOrigin;
		heading = s_vSpotHeading;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bRunning = false;
		s_bSpot = false;
		s_KitId = EntityID.INVALID;
		s_eVerdict = LF_EPlacementVerdict.NO_GROUND;
		s_sReason = "";
		s_Composition = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		IEntity officer = LF_Utils.GetLocalCharacter();
		BaseWorld world = GetGame().GetWorld();
		IEntity kit;
		if (world && s_KitId != EntityID.INVALID)
			kit = world.FindEntityByID(s_KitId);

		// The officer is gone, or the kit was laid or taken away: nothing left to preview.
		if (!officer || !world || !kit)
		{
			End();
			return;
		}

		vector spot[4];
		LF_Roadblocks.SpotFor(officer, kit, spot);
		Update(officer, kit, spot);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EPlacementVerdict Update(IEntity officer, IEntity kit, vector spot[4])
	{
		LF_PlacementCompositionPlan plan = new LF_PlacementCompositionPlan();
		LF_EPlacementVerdict verdict = LF_PlacementCompositions.Plan(officer, Set(), kit, spot, plan);
		if (verdict != s_eVerdict)
			LF_Debug.Info(LF_Roadblocks.FEATURE, string.Format("preview %1 at %2 %3", LF_PlacementRules.Name(verdict), spot[3], plan.m_sReason));

		s_eVerdict = verdict;
		s_sReason = plan.m_sReason;
		s_bSpot = LF_PlacementCompositionGhost.Show(Set(), plan);
		s_vSpotOrigin = spot[3];
		s_vSpotHeading = spot[2];
		return verdict;
	}
}

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//! Harness verb for the proof pictures (agent-box/proofs/roadblock-proof.mjs): the ghost of the
//! roadblock at a chosen spot, green or red as the plan judges it.
//!   roadblockghost <x> <z> <yawDeg>   (client) the ghost at x,z heading yawDeg, held there
//!   roadblockghost off                (client) the ghost away
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "roadblockghost")
			return super.Handle(cmd);

		if (cmd.Arg1 == "off")
		{
			LF_RoadblockPreview.End();
			LF_AgentHarness.Answer(cmd.Id, "ok", "ghost off");
			return true;
		}

		IEntity officer = LF_Utils.GetLocalCharacter();
		if (!officer)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		vector origin = Vector(cmd.Arg1.ToFloat(), 0, cmd.Arg2.ToFloat());
		origin[1] = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		float yaw = cmd.Arg3.ToFloat() * Math.DEG2RAD;
		vector heading = Vector(Math.Sin(yaw), 0, Math.Cos(yaw));
		LF_EPlacementVerdict verdict = LF_RoadblockPreview.ShowAt(officer, origin, heading);
		LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("ghost %1 at %2 pieces=%3 %4", LF_PlacementRules.Name(verdict), origin, LF_PlacementCompositionGhost.Count(), LF_RoadblockPreview.GetReason()));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Roadblocks's statics (registered at script load; Core/LF_StaticReset.c).
class LF_Roadblocks_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_Roadblocks_StaticResetHook());

	override string Owner()
	{
		return "LF_Roadblocks";
	}

	override void Apply()
	{
		LF_Roadblocks.ResetStatics();
	}
}
