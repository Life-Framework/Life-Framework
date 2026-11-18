//------------------------------------------------------------------------------------------------
//! Where a refused player gets a licence and what it costs (#3150): "The gun store counter sells it
//! for $250." The where text is a per-licence string key (LicenceWhere_ plus the licence type, with the LF- prefix); core rows are neutral
//! ("The licence desk") and the pack's table overrides the same Id with the place (AGENTS.md rule 7).
//! A licence the desk does not sell says which job unlocks it (never a level); one with no job says
//! to ask at the desk. The fee is the licence config's, so a server's own price shows.
class LF_LicenceWhere
{
	static const string KEY_DEFAULT = "#LF-LicenceWhere_Default";
	static const string KEY_SOLD = "#LF-LicenceWhere_Sold";
	static const string KEY_JOB = "#LF-LicenceWhere_Job";

	//------------------------------------------------------------------------------------------------
	//! Whether a counter sells this licence for cash (the firearm permit, the driving licence).
	static bool IsSold(LF_ELicenseType licence)
	{
		return licence == LF_LicenceDesk.PERMIT || licence == LF_LicenceDesk.DRIVING;
	}

	//------------------------------------------------------------------------------------------------
	//! The fee in carried cash for a licence a counter sells; 0 for any other.
	static int Fee(LF_ELicenseType licence)
	{
		if (licence == LF_LicenceDesk.PERMIT)
			return LF_LicencesConfig.PermitFee();

		if (licence == LF_LicenceDesk.DRIVING)
			return LF_LicencesConfig.DriverLicenceFee();

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The per-licence key is the prefix plus the licence type; it is joined from pieces because a whole
	//! prefix literal would read as an undeclared key to regen-localization.
	protected static string Prefix()
	{
		return "#LF" + "-LicenceWhere_";
	}

	//------------------------------------------------------------------------------------------------
	//! The key naming the place that sells the licence; the neutral default for a licence with none.
	static string WhereKey(LF_ELicenseType licence)
	{
		if (!IsSold(licence))
			return KEY_DEFAULT;

		return Prefix() + typename.EnumToString(LF_ELicenseType, licence);
	}

	//------------------------------------------------------------------------------------------------
	//! The line a refusal appends for a licence: where it is sold and the fee, which job unlocks it, or
	//! the neutral fallback. Always a sentence; never empty.
	static string Line(LF_ELicenseType licence)
	{
		if (IsSold(licence))
			return WidgetManager.Translate(KEY_SOLD, WidgetManager.Translate(WhereKey(licence)), LF_MoneyFormat.FormatMoney(Fee(licence)));

		if (licence != LF_ELicenseType.UNEMPLOYED)
		{
			string job;
			int minLevel;
			LF_LicenseGate.Resolve(licence, 1, job, minLevel);
			if (job != LF_JobLevels.JOB_CIVILIAN && !job.IsEmpty())
				return WidgetManager.Translate(KEY_JOB, WidgetManager.Translate(LF_ItemCatalogue.JobNameKey(job)));
		}

		return WidgetManager.Translate(KEY_DEFAULT);
	}

	//------------------------------------------------------------------------------------------------
	//! The line for a refusal that names the licence by its name key or enum name (what a shop reply
	//! carries); the neutral fallback when the name is none this machine knows. Client-safe.
	static string LineForName(string name)
	{
		LF_ELicenseType licence;
		if (FromName(name, licence))
			return Line(licence);

		return WidgetManager.Translate(KEY_DEFAULT);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a licence from its enum name, a desk name key, or the local player's catalog name key.
	static bool FromName(string name, out LF_ELicenseType licence)
	{
		if (name.IsEmpty())
			return false;

		LF_LicenseManagerComponent licences;
		PlayerController controller = GetGame().GetPlayerController();
		if (controller)
			licences = LF_Component<LF_LicenseManagerComponent>.Find(controller.GetControlledEntity());

		foreach (LF_ELicenseType candidate : LF_LicenseBits.Order())
		{
			bool match = name == typename.EnumToString(LF_ELicenseType, candidate) || name == LF_LicenceDesk.NameKey(candidate);
			if (!match && licences)
			{
				LF_LicenseConfig config = licences.GetLicenseConfig(candidate);
				match = config && !config.m_sLicenseName.IsEmpty() && config.m_sLicenseName == name;
			}

			if (match)
			{
				licence = candidate;
				return true;
			}
		}

		return false;
	}
}
