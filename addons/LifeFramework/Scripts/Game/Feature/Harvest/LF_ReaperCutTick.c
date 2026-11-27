// LF_ReaperCutTick.c: the hitched reaper cuts the field and fills its hopper (#3289, harvester spike H4).
// The server runs one 200 ms loop over every reaper that LF_ImplementComponent registered. Each tick reads the
// towing vehicle's physics speed and heading, finds the field under the bar, and applies the H2 rules
// (LF_WheatFieldRules): inside the speed band the bar's cells are cut and their yield goes into the hopper,
// otherwise nothing is cut. A refusal (too fast, stopped, hopper full) is logged and shown to the driver once
// each time it changes, not every tick. The hopper is the implement's replicated kg; it is session-only like the
// implement itself (the reaper is not saved; the unload card H7 and the rack H8 own that).

//------------------------------------------------------------------------------------------------
//! What the tick remembers about one reaper between two ticks.
class LF_ReaperCutState
{
	EntityID m_Reaper = EntityID.INVALID;
	//! The field the bar was last in, so the world is searched only when the bar has left it.
	EntityID m_Field = EntityID.INVALID;
	LF_EHarvestRefusal m_eLast = LF_EHarvestRefusal.OK;
}

//------------------------------------------------------------------------------------------------
//! A sphere query that keeps the wheat fields it meets.
class LF_ReaperFieldQuery
{
	ref array<LF_WheatField> m_aFields = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		LF_WheatField field = LF_WheatField.Cast(entity);
		if (field)
			m_aFields.Insert(field);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ReaperCutTick
{
	static const string FEATURE = "Harvest";
	//! The server's cutting cadence.
	static const int TICK_MS = 200; // lf-fixed: the spike's 200 ms cut tick
	//! Metres the bar is long along the heading (the width comes from the implement). Thin, so a cell is cut when
	//! the bar passes over it; at 6 m/s the bar moves 1.2 m a tick and a cell is 3 m, so none is skipped.
	static const float BAR_DEPTH_M = 1; // lf-fixed: the spike's bar geometry, moves to the reaper config with H8
	//! How far from the bar the world is searched for a field's middle.
	static const float FIELD_SEARCH_M = 120; // lf-fixed: a field is at most about 100 m across

	protected static ref array<ref LF_ReaperCutState> s_aStates;
	protected static array<ref LF_ReaperCutState> LzStates()
	{
		if (!s_aStates)
			s_aStates = new array<ref LF_ReaperCutState>();
		return s_aStates;
	}
	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	//! Server: the reaper joins the tick. Called by LF_ImplementComponent when it spawns on the server.
	static void Register(IEntity reaper)
	{
		if (!reaper || !Replication.IsServer())
			return;

		foreach (LF_ReaperCutState known : LzStates())
		{
			if (known.m_Reaper == reaper.GetID())
				return;
		}

		LF_ReaperCutState state = new LF_ReaperCutState();
		state.m_Reaper = reaper.GetID();
		LzStates();
		s_aStates.Insert(state);
		if (!s_bRunning)
		{
			s_bRunning = true;
			GetGame().GetCallqueue().CallLater(TickNow, TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		GetGame().GetCallqueue().Remove(TickNow);
		LzStates();
		s_aStates.Clear();
		s_bRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over every registered reaper. The loop calls it every TICK_MS; a test calls it by hand.
	static void TickNow()
	{
		LzStates();
		for (int i = s_aStates.Count() - 1; i >= 0; i--)
		{
			LF_ReaperCutState state = LzStates()[i];
			IEntity reaper = GetGame().GetWorld().FindEntityByID(state.m_Reaper);
			if (!reaper)
			{
				LzStates();
				s_aStates.Remove(i);
				continue;
			}

			TickOne(state, reaper);
		}

		LzStates();
		if (s_aStates.IsEmpty() && s_bRunning)
		{
			GetGame().GetCallqueue().Remove(TickNow);
			s_bRunning = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The reaper's refusal this tick, or OK when it is cutting or has no field under it. Public for the test.
	static LF_EHarvestRefusal TickOne(LF_ReaperCutState state, IEntity reaper)
	{
		LF_ImplementComponent implement = LF_ImplementComponent.Of(reaper);
		IEntity vehicle = reaper.GetParent();
		if (!implement || !vehicle)
			return LF_EHarvestRefusal.OK;

		// The bar sits at the implement's hitch offset in the vehicle's frame. Computed from the vehicle, not read from the
		// child: a child's world origin lags the parent by a frame, which a vehicle that just moved would show.
		vector local[4];
		reaper.GetLocalTransform(local);
		vector at = vehicle.CoordToParent(local[3]);
		float speed = SpeedOf(vehicle);
		LF_WheatField field = FieldUnder(state, at, speed);
		if (!field)
		{
			state.m_eLast = LF_EHarvestRefusal.OK;
			return LF_EHarvestRefusal.OK;
		}

		LF_EHarvestRefusal refusal = LF_WheatFieldRules.CheckSpeed(speed);
		if (refusal == LF_EHarvestRefusal.OK)
			refusal = CutBar(field, implement, vehicle, at);

		Report(state, vehicle, reaper, refusal, speed);
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Cuts what the bar covers, as much as the hopper takes. HOPPER_FULL when standing cells remain that did not fit.
	protected static LF_EHarvestRefusal CutBar(LF_WheatField field, LF_ImplementComponent implement, IEntity vehicle, vector at)
	{
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		float heading = Math.Atan2(transform[2][0], transform[2][2]) * Math.RAD2DEG;
		LF_WheatGrid grid = field.Grid();
		array<int> touched;
		grid.Footprint(at[0], at[2], heading, implement.GetWidth(), BAR_DEPTH_M, touched);
		array<int> standing = {};
		foreach (int cell : touched)
		{
			if (!grid.IsCut(cell))
				standing.Insert(cell);
		}

		if (standing.IsEmpty())
			return LF_EHarvestRefusal.OK;

		int taken;
		LF_EHarvestRefusal refusal = LF_WheatFieldRules.FillHopper(implement.GetHopperKg(), LF_WheatFieldRules.YieldKg(standing.Count()), taken);
		int fits = taken / LF_WheatFieldRules.CELL_YIELD_KG;
		array<int> toCut = {};
		for (int i = 0; i < fits && i < standing.Count(); i++)
		{
			toCut.Insert(standing[i]);
		}

		int cut = field.CutCells(toCut);
		if (cut > 0)
			implement.SetHopperKg(implement.GetHopperKg() + LF_WheatFieldRules.YieldKg(cut));

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Horizontal ground speed of the towing vehicle in m/s, from its physics body.
	static float SpeedOf(IEntity vehicle)
	{
		Physics physics = vehicle.GetPhysics();
		if (!physics)
			return 0;

		vector velocity = physics.GetVelocity();
		velocity[1] = 0;
		return velocity.Length();
	}

	//------------------------------------------------------------------------------------------------
	//! The field whose box holds the bar's middle, or null. The last field is kept until the bar leaves it; a
	//! stopped reaper outside every field does not search.
	protected static LF_WheatField FieldUnder(LF_ReaperCutState state, vector at, float speed)
	{
		IEntity known = null;
		if (state.m_Field != EntityID.INVALID)
			known = GetGame().GetWorld().FindEntityByID(state.m_Field);

		LF_WheatField field = LF_WheatField.Cast(known);
		if (field && field.Grid().CellAt(at[0], at[2]) >= 0)
			return field;

		state.m_Field = EntityID.INVALID;
		if (speed < LF_WheatFieldRules.MIN_SPEED)
			return null;

		LF_ReaperFieldQuery query = new LF_ReaperFieldQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(at, FIELD_SEARCH_M, query.Collect, null, EQueryEntitiesFlags.ALL);
		foreach (LF_WheatField candidate : query.m_aFields)
		{
			if (candidate.Grid().CellAt(at[0], at[2]) >= 0)
			{
				state.m_Field = candidate.GetID();
				return candidate;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs and shows a refusal once each time it changes; OK clears it so the next one shows again.
	protected static void Report(LF_ReaperCutState state, IEntity vehicle, IEntity reaper, LF_EHarvestRefusal refusal, float speed)
	{
		if (refusal == state.m_eLast)
			return;

		state.m_eLast = refusal;
		if (refusal == LF_EHarvestRefusal.OK)
			return;

		LF_Debug.Info(FEATURE, string.Format("reaper %1 refused: %2 (%3 m/s, hopper %4 kg)", LF_ImplementHitch.RplIdOf(reaper),
			LF_WheatFieldRules.RefusalName(refusal), speed.ToString(-1, 1), LF_ImplementComponent.Of(reaper).GetHopperKg()));
		string key = LF_WheatFieldRules.RefusalKey(refusal);
		if (key.IsEmpty())
			return;

		IEntity driver = DriverOf(vehicle);
		if (driver)
			LF_Utils.NotifyEntity(driver, key);
	}

	//------------------------------------------------------------------------------------------------
	//! The first occupant (the pilot seat comes first), or null for an empty vehicle.
	protected static IEntity DriverOf(IEntity vehicle)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return null;

		array<IEntity> occupants = {};
		compartments.GetOccupants(occupants);
		if (occupants.IsEmpty())
			return null;

		return occupants[0];
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the tick's registry and loop (LF_StaticReset finds it by reflection).
class LF_ReaperCutTick_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ReaperCutTick";
	}

	override void Apply()
	{
		LF_ReaperCutTick.ResetStatics();
	}
}
