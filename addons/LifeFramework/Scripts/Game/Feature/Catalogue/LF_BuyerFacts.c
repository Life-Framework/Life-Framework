//------------------------------------------------------------------------------------------------
//! What an outlet's gate asks about a buyer (#988). The rules (LF_ItemCatalogue.Check, OffenceFor)
//! read only this, so a LOGIC test answers with a stub and the server with LF_EntityBuyerFacts.
class LF_BuyerFacts
{
	//------------------------------------------------------------------------------------------------
	bool HasLicence(LF_ELicenseType licence)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The buyer's rank in a job: the police rank index for an on-duty officer ("POLICE"); for any
	//! other job id (an LF_EJobType name) the job level (LF_JobLevels, 1 or more) when that is the
	//! buyer's current job. -1 when the buyer is not in that job, or no job has that id.
	int JobRank(string job)
	{
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The buyer's organisation rights mask (LF_OrgRights); 0 outside an organisation.
	int OrgRights()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The localization key of a licence's name, for a refusal.
	string LicenceName(LF_ELicenseType licence)
	{
		return typename.EnumToString(LF_ELicenseType, licence);
	}

	//------------------------------------------------------------------------------------------------
	//! The localization key of a rank's title in a job, for a refusal.
	string RankName(string job, int rank)
	{
		if (job == LF_EntityBuyerFacts.JOB_POLICE)
			return "#" + LF_PoliceRanks.TitleKey(rank);

		return LF_ItemCatalogue.JobNameKey(job);
	}
}

//------------------------------------------------------------------------------------------------
//! Server: the facts about one character, read from its components and the server's stores.
class LF_EntityBuyerFacts : LF_BuyerFacts
{
	static const string JOB_POLICE = "POLICE";

	protected IEntity m_Buyer;

	//------------------------------------------------------------------------------------------------
	static LF_EntityBuyerFacts Of(IEntity buyer)
	{
		LF_EntityBuyerFacts facts = new LF_EntityBuyerFacts();
		facts.m_Buyer = buyer;
		return facts;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLicence(LF_ELicenseType licence)
	{
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(m_Buyer);
		if (!licences)
			return false;

		return licences.HasLicense(licence);
	}

	//------------------------------------------------------------------------------------------------
	override int JobRank(string job)
	{
		if (job != JOB_POLICE)
		{
			// The buyer must be in that job now (LF_EJobType name); an id no job has never matches.
			LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(m_Buyer);
			if (!jobs || typename.EnumToString(LF_EJobType, jobs.GetJob()) != job)
				return -1;

			return LF_JobLevels.LevelOfBody(m_Buyer, job);
		}

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return -1;

		LF_PlayerAccount account = accounts.GetAccount(m_Buyer);
		if (!account || !LF_AccountStateBits.IsOnDutyPolice(LF_AccountStateBits.FromAccount(account)))
			return -1;

		return account.GetPoliceRank();
	}

	//------------------------------------------------------------------------------------------------
	override int OrgRights()
	{
		string characterId = LF_Hideout.CharacterIdOf(m_Buyer);
		if (characterId.IsEmpty())
			return 0;

		return LF_OrgRules.RightsOf(LF_OrganizationManager.GetByCharacter(characterId), characterId);
	}

	//------------------------------------------------------------------------------------------------
	override string LicenceName(LF_ELicenseType licence)
	{
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(m_Buyer);
		if (licences)
		{
			LF_LicenseConfig config = licences.GetLicenseConfig(licence);
			if (config && !config.m_sLicenseName.IsEmpty())
				return config.m_sLicenseName;
		}

		return super.LicenceName(licence);
	}
}
