//------------------------------------------------------------------------------------------------
//! The fishing licence gate (#3449): when the fishing config names a licence, a cast without it is
//! refused with the licence refusal line (#3150: where to get it and the fee); with none named
//! (UNEMPLOYED) casting is open. Evaluate is pure so a LOGIC test drives it; Allows reads the
//! character's licences on the server.
class LF_FishingLicenceGate
{
	static const string KEY_NEED = "#LF-Fishing_NoLicence";

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a cast is allowed. A config naming no licence is open to everyone.
	static bool Evaluate(LF_ELicenseType required, bool hasLicence)
	{
		if (required == LF_ELicenseType.UNEMPLOYED)
			return true;

		return hasLicence;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether `character` may cast under `config`. No config or no body is not this gate's
	//! refusal (the cast refuses those itself).
	static bool Allows(IEntity character, LF_FishingConfig config)
	{
		if (!config || !character)
			return true;

		LF_ELicenseType required = config.m_eRequiredLicence;
		bool hasLicence = false;
		if (required != LF_ELicenseType.UNEMPLOYED)
		{
			LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(character);
			hasLicence = licences && licences.HasLicense(required);
		}

		return Evaluate(required, hasLicence);
	}

	//------------------------------------------------------------------------------------------------
	//! The line the refusal toast carries (#3150): where the licence is sold and the fee, or which job
	//! unlocks it. Empty when the config names no licence.
	static string Where(LF_ELicenseType required)
	{
		if (required == LF_ELicenseType.UNEMPLOYED)
			return "";

		return LF_LicenceWhere.Line(required);
	}
}
