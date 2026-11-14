//------------------------------------------------------------------------------------------------
//! Organization tuning: the founding fee, name length, offer lifetime, fund cash limit, upgrade
//! prices and the desk reach. Loaded from Configs/Organizations/LF_Org.conf. A missing or broken
//! file logs once and falls back to the same defaults.
[BaseContainerProps(configRoot: true)]
class LF_OrgConfig
{
	static const ResourceName CONFIG_PATH = "{EA86F8E6A6BCA687}Configs/Organizations/LF_Org.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "organizations";

	static const int FEE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int FEE_MAX = 10000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int NAME_LENGTH_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int NAME_LENGTH_MAX = 64;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int OFFER_MS_MIN = 5000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int OFFER_MS_MAX = 3600000;	// lf-fixed: the schema limit, one hour
	static const int CASH_MOVE_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int CASH_MOVE_MAX = 100000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float DESK_RANGE_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float DESK_RANGE_MAX = 30;	// lf-fixed: the schema limit, mirrors the attribute params

	static const int DEFAULT_FOUNDING_FEE = 250;
	static const int DEFAULT_NAME_MIN = 3;
	static const int DEFAULT_NAME_MAX = 32;
	static const int DEFAULT_OFFER_MS = 120000;
	static const int DEFAULT_MAX_CASH_MOVE = 1000000;
	static const int DEFAULT_PRICE_EXTRA_STORAGE = 500;
	static const int DEFAULT_PRICE_PROCESSING_SLOT = 250;
	static const int DEFAULT_PRICE_PARKED_VEHICLE = 750;
	static const float DEFAULT_DESK_RANGE = 5;

	[Attribute("250", UIWidgets.Auto, "Carried cash the founder hands over at the registry desk to found an organization", params: "0 10000000")]
	int m_iFoundingFee = 250;

	[Attribute("3", UIWidgets.Auto, "Shortest organization name, in characters", params: "1 64")]
	int m_iNameMin = 3;

	[Attribute("32", UIWidgets.Auto, "Longest organization name, in characters", params: "1 64")]
	int m_iNameMax = 32;

	[Attribute("120000", UIWidgets.Auto, "Milliseconds an invitation stands before the invitee has to be asked again", params: "5000 3600000")]
	int m_iOfferMs = 120000;

	[Attribute("1000000", UIWidgets.Auto, "Largest single cash move into or out of an organization fund", params: "1 100000000")]
	int m_iMaxCashMove = 1000000;

	[Attribute("500", UIWidgets.Auto, "Fund price of the extra storage upgrade", params: "0 10000000")]
	int m_iPriceExtraStorage = 500;

	[Attribute("250", UIWidgets.Auto, "Fund price of the processing slot upgrade", params: "0 10000000")]
	int m_iPriceProcessingSlot = 250;

	[Attribute("750", UIWidgets.Auto, "Fund price of the parked vehicle upgrade", params: "0 10000000")]
	int m_iPriceParkedVehicle = 750;

	[Attribute("5", UIWidgets.Auto, "Metres a player may stand from an organization desk when the server moves fund cash", params: "1 30")]
	float m_fDeskRange = 5;

	[Attribute("", UIWidgets.Object, "Rights of the default ranks a new organization starts with, one entry per rank id (founder, officer, member). A rank with no entry keeps its shipped rights. Applies to organizations founded after the change; a saved organization keeps its own ranks")]
	ref array<ref LF_OrgRankDefault> m_aRankDefaults;

	protected static ref LF_OrgConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_OrgConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Org", string.Format("organization config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_OrgConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Org", string.Format("organization config has an unexpected root class, using defaults: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the owner file again (/configreload).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_OrgConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the layers over this instance, which holds the core `.conf`: each pack overlay of this
	//! class in load order (its `m_aRankDefaults`, LF_ConfigOverlay, #1108), then the owner's
	//! "organizations" section, then (on a client) the server's streamed values over that.
	void ApplyLayers()
	{
		if (!m_aRankDefaults)
			m_aRankDefaults = new array<ref LF_OrgRankDefault>();

		LF_OrgRankDefaultList list = new LF_OrgRankDefaultList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_OrgConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aRankDefaults", names[i], claims);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("organizations: pack overlay %1 changed %2 rank defaults", names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			ApplyOwner(reader);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("organizations: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		// Client: the server's effective values replace the result (#285). Null on the server.
		LF_ConfigReader streamed = LF_ConfigStream.OpenReceivedSection(SECTION);
		if (streamed)
		{
			ApplyOwner(streamed);
			LF_Debug.Info(LF_ConfigService.FEATURE, "organizations: using the server's values");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "organizations" section field by field; a key left out keeps the shipped
	//! value. `rankDefaults` is keyed by default rank id (founder, officer, member), each record with an
	//! optional "op" (add, replace, patch, remove), and sets that rung's rights for organizations
	//! founded afterwards (#857). A remove drops the entry, so the rung is back to its shipped rights.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"foundingFee", "nameMin", "nameMax", "offerMs", "maxCashMove", "priceExtraStorage", "priceProcessingSlot", "priceParkedVehicle", "deskRange", "rankDefaults"};
		reader.WarnUnknownKeys(ownerKeys);
		int nameMin = m_iNameMin;
		int nameMax = m_iNameMax;
		reader.ReadInt("foundingFee", m_iFoundingFee, FEE_MIN, FEE_MAX);
		reader.ReadInt("nameMin", m_iNameMin, NAME_LENGTH_MIN, NAME_LENGTH_MAX);
		reader.ReadInt("nameMax", m_iNameMax, NAME_LENGTH_MIN, NAME_LENGTH_MAX);
		reader.ReadInt("offerMs", m_iOfferMs, OFFER_MS_MIN, OFFER_MS_MAX);
		reader.ReadInt("maxCashMove", m_iMaxCashMove, CASH_MOVE_MIN, CASH_MOVE_MAX);
		reader.ReadInt("priceExtraStorage", m_iPriceExtraStorage, FEE_MIN, FEE_MAX);
		reader.ReadInt("priceProcessingSlot", m_iPriceProcessingSlot, FEE_MIN, FEE_MAX);
		reader.ReadInt("priceParkedVehicle", m_iPriceParkedVehicle, FEE_MIN, FEE_MAX);
		reader.ReadFloat("deskRange", m_fDeskRange, DESK_RANGE_MIN, DESK_RANGE_MAX);

		if (m_iNameMin > m_iNameMax)
		{
			LF_Debug.Error("Org", string.Format("organizations: nameMin %1 is above nameMax %2, so no name would be valid; keeping %3 and %4", m_iNameMin, m_iNameMax, nameMin, nameMax));
			m_iNameMin = nameMin;
			m_iNameMax = nameMax;
		}

		if (!m_aRankDefaults)
			m_aRankDefaults = new array<ref LF_OrgRankDefault>();

		LF_ConfigOverlay.ApplyRecords(reader, "rankDefaults", new LF_OrgRankDefaultList(this), LF_ConfigService.GetOwnerSource());
	}

	//------------------------------------------------------------------------------------------------
	//! The owner's rights entry for a default rank, or null.
	LF_OrgRankDefault FindRankDefault(string rankId)
	{
		if (!m_aRankDefaults)
			return null;

		foreach (LF_OrgRankDefault entry : m_aRankDefaults)
		{
			if (entry && entry.m_sRank == rankId)
				return entry;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] rankId A default rank id.
	//! \param[out] rights The configured rights mask, set only when the config has an entry.
	//! \return True when the config sets that rank's rights.
	static bool RankRights(string rankId, out int rights)
	{
		LF_OrgConfig config = Get();
		if (!config)
			return false;

		LF_OrgRankDefault entry = config.FindRankDefault(rankId);
		if (!entry)
			return false;

		rights = entry.Rights();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the effective values a client reads into the config stream (#285), under the owner
	//! section's own keys so the client applies them with ApplyOwner. The rank defaults stay on the
	//! server: a new organization's ranks are built there and replicated as the organization's own.
	//! \param[in] ctx A save context with this section's object open.
	void WriteStream(notnull SCR_JsonSaveContext ctx)
	{
		ctx.WriteValue("foundingFee", m_iFoundingFee);
		ctx.WriteValue("nameMin", m_iNameMin);
		ctx.WriteValue("nameMax", m_iNameMax);
		ctx.WriteValue("offerMs", m_iOfferMs);
		ctx.WriteValue("maxCashMove", m_iMaxCashMove);
		ctx.WriteValue("priceExtraStorage", m_iPriceExtraStorage);
		ctx.WriteValue("priceProcessingSlot", m_iPriceProcessingSlot);
		ctx.WriteValue("priceParkedVehicle", m_iPriceParkedVehicle);
		ctx.WriteValue("deskRange", m_fDeskRange);
	}

	//------------------------------------------------------------------------------------------------
	static int FoundingFee()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iFoundingFee < 0)
			return DEFAULT_FOUNDING_FEE;

		return config.m_iFoundingFee;
	}

	//------------------------------------------------------------------------------------------------
	static int NameMin()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iNameMin < 1)
			return DEFAULT_NAME_MIN;

		return config.m_iNameMin;
	}

	//------------------------------------------------------------------------------------------------
	static int NameMax()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iNameMax < 1)
			return DEFAULT_NAME_MAX;

		return config.m_iNameMax;
	}

	//------------------------------------------------------------------------------------------------
	static int OfferMs()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iOfferMs <= 0)
			return DEFAULT_OFFER_MS;

		return config.m_iOfferMs;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxCashMove()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iMaxCashMove <= 0)
			return DEFAULT_MAX_CASH_MOVE;

		return config.m_iMaxCashMove;
	}

	//------------------------------------------------------------------------------------------------
	static int PriceExtraStorage()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iPriceExtraStorage < 0)
			return DEFAULT_PRICE_EXTRA_STORAGE;

		return config.m_iPriceExtraStorage;
	}

	//------------------------------------------------------------------------------------------------
	static int PriceProcessingSlot()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iPriceProcessingSlot < 0)
			return DEFAULT_PRICE_PROCESSING_SLOT;

		return config.m_iPriceProcessingSlot;
	}

	//------------------------------------------------------------------------------------------------
	static int PriceParkedVehicle()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iPriceParkedVehicle < 0)
			return DEFAULT_PRICE_PARKED_VEHICLE;

		return config.m_iPriceParkedVehicle;
	}

	//------------------------------------------------------------------------------------------------
	static float DeskRange()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_fDeskRange <= 0)
			return DEFAULT_DESK_RANGE;

		return config.m_fDeskRange;
	}
}

//------------------------------------------------------------------------------------------------
//! One default rank's rights as the owner config states them (#857): a bool per permission. The
//! saved form stays the rights mask (LF_OrgRights), so no save format changes.
[BaseContainerProps()]
class LF_OrgRankDefault : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "The default rank id: founder, officer or member")]
	string m_sRank;

	[Attribute("0", UIWidgets.CheckBox, "May take cash out of the fund")]
	bool m_bWithdraw;

	[Attribute("0", UIWidgets.CheckBox, "May invite players")]
	bool m_bInvite;

	[Attribute("0", UIWidgets.CheckBox, "May kick members below this rank")]
	bool m_bKick;

	[Attribute("0", UIWidgets.CheckBox, "May buy upgrades from the fund")]
	bool m_bBuyUpgrade;

	[Attribute("0", UIWidgets.CheckBox, "May promote and demote members below this rank")]
	bool m_bManageRanks;

	[Attribute("0", UIWidgets.CheckBox, "Reserved for the war record: no effect yet")]
	bool m_bDeclareWar;

	[Attribute("0", UIWidgets.CheckBox, "Reserved: the hideout stash opens for every member today")]
	bool m_bUseStash;

	[Attribute("0", UIWidgets.CheckBox, "Reserved for territory: no effect yet")]
	bool m_bManageTerritory;

	[Attribute("0", UIWidgets.CheckBox, "Reserved: spending the fund on something other than an upgrade")]
	bool m_bSpendFund;

	//------------------------------------------------------------------------------------------------
	//! An entry holding exactly the permissions a rights mask grants.
	static LF_OrgRankDefault FromRights(string rankId, int rights)
	{
		LF_OrgRankDefault entry = new LF_OrgRankDefault();
		entry.m_sRank = rankId;
		entry.m_bWithdraw = Has(rights, LF_EOrganizationPermission.WITHDRAW);
		entry.m_bInvite = Has(rights, LF_EOrganizationPermission.INVITE);
		entry.m_bKick = Has(rights, LF_EOrganizationPermission.KICK);
		entry.m_bBuyUpgrade = Has(rights, LF_EOrganizationPermission.BUY_UPGRADE);
		entry.m_bManageRanks = Has(rights, LF_EOrganizationPermission.MANAGE_RANKS);
		entry.m_bDeclareWar = Has(rights, LF_EOrganizationPermission.DECLARE_WAR);
		entry.m_bUseStash = Has(rights, LF_EOrganizationPermission.USE_STASH);
		entry.m_bManageTerritory = Has(rights, LF_EOrganizationPermission.MANAGE_TERRITORY);
		entry.m_bSpendFund = Has(rights, LF_EOrganizationPermission.SPEND_FUND);
		return entry;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Has(int rights, LF_EOrganizationPermission permission)
	{
		return (rights & LF_OrgRights.Bit(permission)) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The rights mask this entry grants.
	int Rights()
	{
		int rights;
		if (m_bWithdraw)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.WITHDRAW);
		if (m_bInvite)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.INVITE);
		if (m_bKick)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.KICK);
		if (m_bBuyUpgrade)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.BUY_UPGRADE);
		if (m_bManageRanks)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.MANAGE_RANKS);
		if (m_bDeclareWar)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.DECLARE_WAR);
		if (m_bUseStash)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH);
		if (m_bManageTerritory)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.MANAGE_TERRITORY);
		if (m_bSpendFund)
			rights = rights | LF_OrgRights.Bit(LF_EOrganizationPermission.SPEND_FUND);
		return rights;
	}
}

//------------------------------------------------------------------------------------------------
//! The organizations `rankDefaults` list for the shared merge (LF_ConfigOverlay), keyed by default
//! rank id (founder, officer, member). A rank with no entry keeps its shipped rights, so a remove
//! puts a rank back to them. A new or replaced entry starts from the shipped rights.
class LF_OrgRankDefaultList : LF_ConfigList
{
	protected LF_OrgConfig m_Config;

	void LF_OrgRankDefaultList(LF_OrgConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aRankDefaults)
			m_Config.m_aRankDefaults = new array<ref LF_OrgRankDefault>();
	}

	override string GetName()
	{
		return "organizations.rankDefaults";
	}

	override int Count()
	{
		return m_Config.m_aRankDefaults.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aRankDefaults[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_OrgRankDefault.Cast(record).m_sRank;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sRank", id);
		return id;
	}

	override string CanonicalId(string key)
	{
		array<string> ladder = {};
		LF_OrganizationConfig.DefaultLadder(ladder);
		if (ladder.Contains(key))
			return key;

		return string.Empty;
	}

	override string IdHint()
	{
		return "a default rank (founder, officer or member)";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_OrgRankDefault.FromRights(id, LF_OrganizationConfig.ShippedRights(id));
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_OrgRankDefault entry = LF_OrgRankDefault.Cast(record);
		if (index < m_Config.m_aRankDefaults.Count())
			m_Config.m_aRankDefaults.Set(index, entry);
		else
			m_Config.m_aRankDefaults.Insert(entry);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRankDefaults.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (CanonicalId(LF_OrgRankDefault.Cast(record).m_sRank).IsEmpty())
			return "the id is not a default rank (founder, officer or member)";

		return string.Empty;
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_OrgRankDefault entry = LF_OrgRankDefault.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_bWithdraw") && container.Get("m_bWithdraw", entry.m_bWithdraw))
			changed++;
		if (container.IsVariableSetDirectly("m_bInvite") && container.Get("m_bInvite", entry.m_bInvite))
			changed++;
		if (container.IsVariableSetDirectly("m_bKick") && container.Get("m_bKick", entry.m_bKick))
			changed++;
		if (container.IsVariableSetDirectly("m_bBuyUpgrade") && container.Get("m_bBuyUpgrade", entry.m_bBuyUpgrade))
			changed++;
		if (container.IsVariableSetDirectly("m_bManageRanks") && container.Get("m_bManageRanks", entry.m_bManageRanks))
			changed++;
		if (container.IsVariableSetDirectly("m_bDeclareWar") && container.Get("m_bDeclareWar", entry.m_bDeclareWar))
			changed++;
		if (container.IsVariableSetDirectly("m_bUseStash") && container.Get("m_bUseStash", entry.m_bUseStash))
			changed++;
		if (container.IsVariableSetDirectly("m_bManageTerritory") && container.Get("m_bManageTerritory", entry.m_bManageTerritory))
			changed++;
		if (container.IsVariableSetDirectly("m_bSpendFund") && container.Get("m_bSpendFund", entry.m_bSpendFund))
			changed++;
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_OrgRankDefault entry = LF_OrgRankDefault.Cast(record);
		array<string> recordKeys = {"op", "withdraw", "invite", "kick", "buyUpgrade", "manageRanks", "declareWar", "useStash", "manageTerritory", "spendFund"};
		reader.WarnUnknownKeys(recordKeys);
		reader.ReadBool("withdraw", entry.m_bWithdraw);
		reader.ReadBool("invite", entry.m_bInvite);
		reader.ReadBool("kick", entry.m_bKick);
		reader.ReadBool("buyUpgrade", entry.m_bBuyUpgrade);
		reader.ReadBool("manageRanks", entry.m_bManageRanks);
		reader.ReadBool("declareWar", entry.m_bDeclareWar);
		reader.ReadBool("useStash", entry.m_bUseStash);
		reader.ReadBool("manageTerritory", entry.m_bManageTerritory);
		reader.ReadBool("spendFund", entry.m_bSpendFund);
	}
}
