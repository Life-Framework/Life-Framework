// LF_ItemCatalogue.c — the item catalogue's rules (#988): which outlet sells what at which price,
// the gate in front of each offer and what a refusal names, what carrying an item means to the law,
// and what a worn item hides from a witness. Data: LF_ItemCatalogueConfig. The server decides every
// sale (LF_ShopComponent.Buy); a client reads the same streamed catalogue to draw the price list.

enum LF_EItemCategory
{
	UNKNOWN,
	WEAPON,
	AMMO,
	CLOTHING,
	EQUIPMENT,
	TOOL
}

enum LF_EItemLegality
{
	UNKNOWN,
	LEGAL,
	LICENSED,
	RESTRICTED,
	ILLEGAL
}

//! Why an offer's gate said no. OPEN lets the buyer through.
enum LF_EOutletGate
{
	OPEN,
	NO_OFFER,
	NEEDS_LICENCE,
	NEEDS_JOB,
	NEEDS_RANK,
	NEEDS_ORG_RIGHT
}

class LF_ItemCatalogue
{
	static const string FEATURE = "Catalogue";

	//! Crime catalog ids (Configs/Crime/LF_CrimeCatalog.conf) for carrying an item the law does not
	//! allow the carrier. The catalog holds the wanted, fine and sentence numbers.
	static const string OFFENCE_UNLICENSED = "#LF-Offence_UnlicensedItem";
	static const string OFFENCE_RESTRICTED = "#LF-Offence_RestrictedItem";
	static const string OFFENCE_ILLEGAL = "#LF-Offence_IllegalItem";

	//! Path fragments of the DLC-owned prefab families (the Rally DLC: suit, helmet, gloves, bib,
	//! rally cars and its prefab library). A player without the DLC cannot use them, so no outlet may
	//! sell them. The full list is tools/validation/dlc-owned-prefabs.txt, which validate checks.
	static const ref array<string> DLC_PATH_FRAGMENTS = {"/Suit_Rally_01/", "/Helmet_Rally_01/", "/Gloves_Rally_01/", "/Bib_Competition/", "/S105_rally", "RallyLibrary/"};

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends: the next read layers the catalogue again.
	static void ResetStatics()
	{
		LF_ItemCatalogueConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	static LF_EItemCategory ParseCategory(string word)
	{
		if (word == "weapon")
			return LF_EItemCategory.WEAPON;
		if (word == "ammo")
			return LF_EItemCategory.AMMO;
		if (word == "clothing")
			return LF_EItemCategory.CLOTHING;
		if (word == "equipment")
			return LF_EItemCategory.EQUIPMENT;
		if (word == "tool")
			return LF_EItemCategory.TOOL;

		return LF_EItemCategory.UNKNOWN;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EItemLegality ParseLegality(string word)
	{
		if (word == "legal")
			return LF_EItemLegality.LEGAL;
		if (word == "licensed")
			return LF_EItemLegality.LICENSED;
		if (word == "restricted")
			return LF_EItemLegality.RESTRICTED;
		if (word == "illegal")
			return LF_EItemLegality.ILLEGAL;

		return LF_EItemLegality.UNKNOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a prefab belongs to a DLC family (DLC_PATH_FRAGMENTS).
	static bool IsDlcOwned(ResourceName prefab)
	{
		string path = prefab;
		foreach (string fragment : DLC_PATH_FRAGMENTS)
		{
			if (path.Contains(fragment))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The live (not removed) item with this id, or null.
	static LF_CatalogueItem Item(LF_ItemCatalogueConfig config, string id)
	{
		if (!config)
			return null;

		LF_CatalogueItem item = config.FindItem(id);
		if (!item || item.m_bRemoved)
			return null;

		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! The live item whose prefab is this one (matched by GUID), or null.
	static LF_CatalogueItem ItemByPrefab(LF_ItemCatalogueConfig config, ResourceName prefab)
	{
		if (!config || !config.m_aItems || prefab.IsEmpty())
			return null;

		foreach (LF_CatalogueItem item : config.m_aItems)
		{
			if (item && !item.m_bRemoved && LF_Utils.SamePrefab(item.m_Prefab, prefab))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The live offers of one outlet whose item is live, in catalogue order.
	static void Offers(LF_ItemCatalogueConfig config, string outlet, notnull array<LF_OutletOffer> outOffers)
	{
		outOffers.Clear();
		if (!config || !config.m_aOffers || outlet.IsEmpty())
			return;

		foreach (LF_OutletOffer offer : config.m_aOffers)
		{
			if (!offer || offer.m_bRemoved || offer.m_sOutlet != outlet)
				continue;

			if (Item(config, offer.m_sItem))
				outOffers.Insert(offer);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The live offer at this outlet for this prefab, or null when the outlet does not sell it.
	static LF_OutletOffer OfferFor(LF_ItemCatalogueConfig config, string outlet, ResourceName prefab, out LF_CatalogueItem item)
	{
		item = null;
		array<LF_OutletOffer> offers = {};
		Offers(config, outlet, offers);
		foreach (LF_OutletOffer offer : offers)
		{
			LF_CatalogueItem candidate = Item(config, offer.m_sItem);
			if (candidate && LF_Utils.SamePrefab(candidate.m_Prefab, prefab))
			{
				item = candidate;
				return offer;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The gate in front of one offer, in order licence, job, rank, organisation right. Every gate
	//! that is set must pass; the first that does not is the answer.
	//! \param[out] missing What the buyer lacks, as a localization key the slip prints (the licence's
	//! name, the job's name, the rank's title, the right's name); empty when open.
	static LF_EOutletGate Check(LF_OutletOffer offer, notnull LF_BuyerFacts buyer, out string missing)
	{
		missing = string.Empty;
		if (!offer || offer.m_bRemoved)
			return LF_EOutletGate.NO_OFFER;

		if (!offer.m_sLicence.IsEmpty())
		{
			LF_ELicenseType licence;
			if (!ParseLicence(offer.m_sLicence, licence))
			{
				LF_Debug.Error(FEATURE, string.Format("offer %1 gates on '%2', which is not a licence; refused", offer.Key(), offer.m_sLicence));
				missing = offer.m_sLicence;
				return LF_EOutletGate.NEEDS_LICENCE;
			}

			if (!buyer.HasLicence(licence))
			{
				missing = buyer.LicenceName(licence);
				return LF_EOutletGate.NEEDS_LICENCE;
			}
		}

		if (!offer.m_sJob.IsEmpty())
		{
			int rank = buyer.JobRank(offer.m_sJob);
			if (rank < 0)
			{
				missing = JobNameKey(offer.m_sJob);
				return LF_EOutletGate.NEEDS_JOB;
			}

			if (rank < offer.m_iMinRank)
			{
				missing = buyer.RankName(offer.m_sJob, offer.m_iMinRank);
				return LF_EOutletGate.NEEDS_RANK;
			}
		}

		if (!offer.m_sOrgRight.IsEmpty())
		{
			LF_EOrganizationPermission right = typename.StringToEnum(LF_EOrganizationPermission, offer.m_sOrgRight);
			if (typename.EnumToString(LF_EOrganizationPermission, right) != offer.m_sOrgRight)
			{
				LF_Debug.Error(FEATURE, string.Format("offer %1 gates on '%2', which is not an organisation right; refused", offer.Key(), offer.m_sOrgRight));
				missing = offer.m_sOrgRight;
				return LF_EOutletGate.NEEDS_ORG_RIGHT;
			}

			if (!LF_OrgRights.Has(buyer.OrgRights(), right))
			{
				missing = OrgRightNameKey(offer.m_sOrgRight);
				return LF_EOutletGate.NEEDS_ORG_RIGHT;
			}
		}

		return LF_EOutletGate.OPEN;
	}

	//------------------------------------------------------------------------------------------------
	//! The till's refusal for a gate answer.
	static LF_EShopRefusal RefusalOf(LF_EOutletGate gate)
	{
		if (gate == LF_EOutletGate.OPEN)
			return LF_EShopRefusal.NONE;
		if (gate == LF_EOutletGate.NEEDS_LICENCE)
			return LF_EShopRefusal.NEEDS_LICENCE;
		if (gate == LF_EOutletGate.NEEDS_JOB)
			return LF_EShopRefusal.NEEDS_JOB;
		if (gate == LF_EOutletGate.NEEDS_RANK)
			return LF_EShopRefusal.NEEDS_RANK;
		if (gate == LF_EOutletGate.NEEDS_ORG_RIGHT)
			return LF_EShopRefusal.NEEDS_ORG_RIGHT;

		return LF_EShopRefusal.NOT_SOLD_HERE;
	}

	//------------------------------------------------------------------------------------------------
	//! The offence carrying this item is for this carrier, as a crime catalog id; empty when lawful.
	//! A legal item is always lawful; a licensed or restricted one is lawful with the item's licence
	//! (an item that names none is never lawful to carry); an illegal one never is.
	static string OffenceFor(LF_CatalogueItem item, notnull LF_BuyerFacts carrier)
	{
		if (!item)
			return string.Empty;

		LF_EItemLegality legality = item.GetLegality();
		if (legality == LF_EItemLegality.ILLEGAL)
			return OFFENCE_ILLEGAL;

		if (legality != LF_EItemLegality.LICENSED && legality != LF_EItemLegality.RESTRICTED)
			return string.Empty;

		LF_ELicenseType licence;
		if (ParseLicence(item.m_sLicence, licence) && carrier.HasLicence(licence))
			return string.Empty;

		if (legality == LF_EItemLegality.RESTRICTED)
			return OFFENCE_RESTRICTED;

		return OFFENCE_UNLICENSED;
	}

	//------------------------------------------------------------------------------------------------
	//! What a set of worn prefabs hides from a witness: any item with the flag hides that part.
	static void Concealment(LF_ItemCatalogueConfig config, notnull array<ResourceName> worn, out bool face, out bool build)
	{
		face = false;
		build = false;
		foreach (ResourceName prefab : worn)
		{
			LF_CatalogueItem item = ItemByPrefab(config, prefab);
			if (!item)
				continue;

			if (item.m_bConcealsFace)
				face = true;
			if (item.m_bConcealsBuild)
				build = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A licence by its LF_ELicenseType name. \return False for a name that is not a licence.
	static bool ParseLicence(string name, out LF_ELicenseType licence)
	{
		if (name.IsEmpty())
			return false;

		licence = typename.StringToEnum(LF_ELicenseType, name);
		return typename.EnumToString(LF_ELicenseType, licence) == name;
	}

	//------------------------------------------------------------------------------------------------
	//! A job id's name key for a refusal ("POLICE" -> "#LF-Job_Name_Police"); an id with no name
	//! reads as the unknown job.
	static string JobNameKey(string job)
	{
		if (job == "POLICE")
			return "#LF-Job_Name_Police";
		if (job == "MEDIC")
			return "#LF-Job_Name_Medic";
		if (job == "FARMER")
			return "#LF-Job_Name_Farmer";
		if (job == "MINER")
			return "#LF-Job_Name_Miner";
		if (job == "LUMBERJACK")
			return "#LF-Job_Name_Lumberjack";

		return "#LF-Job_Name_Unknown";
	}

	//------------------------------------------------------------------------------------------------
	//! An organisation right's name key for a refusal (LF_EOrganizationPermission name).
	static string OrgRightNameKey(string right)
	{
		if (right == "WITHDRAW")
			return "#LF-OrgRight_WITHDRAW";
		if (right == "INVITE")
			return "#LF-OrgRight_INVITE";
		if (right == "KICK")
			return "#LF-OrgRight_KICK";
		if (right == "BUY_UPGRADE")
			return "#LF-OrgRight_BUY_UPGRADE";
		if (right == "MANAGE_RANKS")
			return "#LF-OrgRight_MANAGE_RANKS";
		if (right == "DECLARE_WAR")
			return "#LF-OrgRight_DECLARE_WAR";
		if (right == "USE_STASH")
			return "#LF-OrgRight_USE_STASH";
		if (right == "MANAGE_TERRITORY")
			return "#LF-OrgRight_MANAGE_TERRITORY";
		if (right == "SPEND_FUND")
			return "#LF-OrgRight_SPEND_FUND";

		return right;
	}
}
