//------------------------------------------------------------------------------------------------
//! Live plates. A second issue of a plate that is already out is refused so two
//! vehicles cannot share one by construction. The plate string is the key.
//------------------------------------------------------------------------------------------------
class EL_LicensePlateRegistry
{
	protected static const int MAX_ATTEMPTS = 32;
	protected static ref map<string, string> s_mPlates;

	//------------------------------------------------------------------------------------------------
	protected static map<string, string> Plates()
	{
		if (!s_mPlates)
			s_mPlates = new map<string, string>();

		return s_mPlates;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLive(string plate)
	{
		if (plate.IsEmpty())
			return false;

		return Plates().Contains(plate);
	}

	//------------------------------------------------------------------------------------------------
	static void Remember(string plate, string vehicleId)
	{
		if (plate.IsEmpty())
			return;

		Plates().Set(plate, vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	static void Release(string plate)
	{
		if (plate.IsEmpty())
			return;

		if (!Plates().Contains(plate))
			return;

		Plates().Remove(plate);
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps a candidate when it is not already live.
	//! \return The plate when it was kept, or empty when it is blank or already issued.
	static string TakeUnique(string plate, string vehicleId)
	{
		if (plate.IsEmpty())
			return "";

		if (IsLive(plate))
			return "";

		Remember(plate, vehicleId);
		return plate;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws from the generator until the plate is not already live.
	//! \return The issued plate, or empty when none could be kept.
	static string Issue(EL_LicensePlateGeneratorBase generator, string vehicleId)
	{
		if (!generator)
		{
			EL_Debug.Error("LicensePlate", "issue refused: no generator");
			return "";
		}

		for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++)
		{
			string plate = TakeUnique(generator.GenerateLicensePlate(), vehicleId);
			if (!plate.IsEmpty())
			{
				EL_Debug.Log("LicensePlate", string.Format("issued plate %1 for %2", plate, vehicleId));
				return plate;
			}
		}

		EL_Debug.Error("LicensePlate", string.Format("issue failed: no unique plate for %1", vehicleId));
		return "";
	}
}
