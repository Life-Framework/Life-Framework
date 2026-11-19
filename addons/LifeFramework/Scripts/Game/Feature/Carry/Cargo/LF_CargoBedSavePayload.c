//------------------------------------------------------------------------------------------------
//! One parked truck's bed in the save ("cargobed" section, #3266): the vehicle's garage key (the
//! LF_VehicleOwnership id, stable across restarts) and the stack as LF_CargoBed.Pack writes it.
//! Field order is the binary save order; append only.
class LF_CargoBedRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sVehicleId;
	string m_sStack;

	//------------------------------------------------------------------------------------------------
	static LF_CargoBedRow Create(string vehicleId, string stack)
	{
		LF_CargoBedRow row = new LF_CargoBedRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sVehicleId = vehicleId;
		row.m_sStack = stack;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The bed loads of owned vehicles, by vehicle id (#3266). A bed lives on the vehicle entity, which is
//! deleted when the car goes to the garage, is towed, or despawns, so the load is taken into this ledger
//! each time LF_VehicleOwnership captures the car's cargo (a hook in CaptureCargo) and put back when the
//! car spawns again (a hook in SpawnOwned). The save section writes the ledger. Only owned vehicles
//! have an id; a map truck nobody owns keeps no load.
class LF_CargoBedPersist
{
	static const string FEATURE = "Cargo";

	protected static ref map<string, string> s_mStacks;

	//------------------------------------------------------------------------------------------------
	protected static map<string, string> Stacks()
	{
		if (!s_mStacks)
			s_mStacks = new map<string, string>();

		return s_mStacks;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mStacks = null;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Stacks().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The saved stack text of a vehicle, "" when none.
	static string StackOf(string vehicleId)
	{
		string stack;
		Stacks().Find(vehicleId, stack);
		return stack;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a live vehicle's bed into the ledger; an empty bed (or no bed) clears the vehicle's row.
	//! \return True when a non-empty load was kept.
	static bool Capture(string vehicleId, IEntity vehicle)
	{
		if (vehicleId.IsEmpty() || !vehicle)
			return false;

		LF_CargoBedComponent bed = LF_Component<LF_CargoBedComponent>.Find(vehicle);
		if (!bed)
			return false;

		if (bed.IsEmpty())
		{
			Forget(vehicleId, "the bed is empty");
			return false;
		}

		Stacks().Set(vehicleId, bed.SavedText());
		LF_Debug.Log(FEATURE, string.Format("bed of %1 captured: %2 unit(s)", vehicleId, bed.GetUnits()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Forget(string vehicleId, string why)
	{
		if (Stacks().Contains(vehicleId))
		{
			Stacks().Remove(vehicleId);
			LF_Debug.Info(FEATURE, string.Format("bed of %1 dropped: %2", vehicleId, why));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: makes a live vehicle's bed equal its saved load (empty when nothing is saved). Safe to call
	//! twice: the second call finds the same stack.
	//! \return True when the vehicle has a bed.
	static bool Apply(string vehicleId, IEntity vehicle)
	{
		if (!Replication.IsServer() || vehicleId.IsEmpty() || !vehicle)
			return false;

		LF_CargoBedComponent bed = LF_Component<LF_CargoBedComponent>.Find(vehicle);
		if (!bed)
			return false;

		string stack = StackOf(vehicleId);
		if (bed.SavedText() == stack)
			return true;

		bed.ServerRestore(stack);
		LF_Debug.Info(FEATURE, string.Format("bed of %1 restored: %2 unit(s)", vehicleId, bed.GetUnits()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_CargoBedRow> ExportAll()
	{
		array<ref LF_CargoBedRow> rows = new array<ref LF_CargoBedRow>();
		foreach (string vehicleId, string stack : Stacks())
		{
			rows.Insert(LF_CargoBedRow.Create(vehicleId, stack));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the ledger with `rows`, then puts each live vehicle's bed in line with it. A row for a
	//! vehicle the vehicle records do not know (scrapped since) or with no readable load is dropped with a
	//! log line; a row whose car is in the garage or the impound waits for it to spawn. Loading the same
	//! rows twice ends in the same state.
	//! \return how many rows were kept.
	static int ApplyAll(notnull array<ref LF_CargoBedRow> rows)
	{
		Stacks().Clear();
		foreach (LF_CargoBedRow row : rows)
		{
			if (!row || row.m_sVehicleId.IsEmpty())
				continue;

			if (LF_VehicleOwnership.GetOwner(row.m_sVehicleId).IsEmpty())
			{
				LF_Debug.Info(FEATURE, "saved bed of an unknown vehicle skipped: " + row.m_sVehicleId);
				continue;
			}

			array<int> units = {};
			LF_CargoBed.Unpack(row.m_sStack, units, 1000);
			if (units.IsEmpty())
			{
				LF_Debug.Info(FEATURE, "saved bed of " + row.m_sVehicleId + " holds no readable load, dropped");
				continue;
			}

			Stacks().Set(row.m_sVehicleId, LF_CargoBed.Pack(units));
		}

		foreach (LF_VehicleRecord record : LF_VehicleOwnership.ExportAll())
		{
			if (!record)
				continue;

			IEntity vehicle = LF_VehicleOwnership.GetVehicle(record.m_sVehicleId);
			if (vehicle)
				Apply(record.m_sVehicleId, vehicle);
		}

		return Stacks().Count();
	}
}

//------------------------------------------------------------------------------------------------
class LF_CargoBedPersist_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CargoBedPersist";
	}

	override void Apply()
	{
		LF_CargoBedPersist.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "cargobed" (#3266): one LF_CargoBedRow per owned vehicle that carries a load, keyed by
//! the vehicle's garage id. Sections apply after the "vehicles" payload, so every owned car's record is
//! back when the rows are checked; the load itself returns when the car spawns again. A save from before
//! the section holds no load, which is the state a boot starts in, so OnAbsent keeps the default no-op.
class LF_CargoBedSavePayload : LF_SavePayload
{
	static const string NAME = "cargobed";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_CargoBedRow> rows = LF_CargoBedPersist.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 cargo bed row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_CargoBedRow> rows = new array<ref LF_CargoBedRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_CargoBedPersist.FEATURE, "cargo beds failed to read; the loads stay as they were");
			return false;
		}

		int kept = LF_CargoBedPersist.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 cargo bed row(s)", kept, rows.Count()));
		return true;
	}
}
