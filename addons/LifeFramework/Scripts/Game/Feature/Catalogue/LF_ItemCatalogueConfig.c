//------------------------------------------------------------------------------------------------
//! One item of the catalogue (#988): what it is, what it costs, and what the law says about it.
//! The id is the stable key packs and the owner patch by; never rename one.
[BaseContainerProps()]
class LF_CatalogueItem : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable item id (pistol_m9, jacket_raincoat). Packs and the owner patch by it, so never rename one")]
	string m_sId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The item's prefab, a {GUID}path. Never a DLC-owned prefab", "et")]
	ResourceName m_Prefab;

	[Attribute("equipment", UIWidgets.EditBox, "Category: weapon, ammo, clothing, equipment or tool")]
	string m_sCategory;

	[Attribute("100", UIWidgets.Auto, "Price a buyer pays at an outlet", params: "0 1000000")]
	int m_iPrice;

	[Attribute("50", UIWidgets.Auto, "Price an outlet pays a seller. Never above the price", params: "0 1000000")]
	int m_iSellPrice;

	[Attribute("legal", UIWidgets.EditBox, "Legality: legal, licensed (lawful with the licence), restricted (lawful only with the licence, e.g. police kit) or illegal")]
	string m_sLegality;

	[Attribute("", UIWidgets.EditBox, "Licence (LF_ELicenseType name) that makes carrying a licensed or restricted item lawful, e.g. FIREARM_PERMIT")]
	string m_sLicence;

	[Attribute("0", UIWidgets.CheckBox, "Clothing: hides the wearer's face from a witness")]
	bool m_bConcealsFace;

	[Attribute("0", UIWidgets.CheckBox, "Clothing: hides the wearer's build from a witness")]
	bool m_bConcealsBuild;

	[Attribute("0", UIWidgets.CheckBox, "Hidden: no outlet sells it; a later layer can set it back. The remove verb takes the item out of the list instead")]
	bool m_bRemoved;

	//------------------------------------------------------------------------------------------------
	static LF_CatalogueItem Make(string id, ResourceName prefab, string category, int price, int sellPrice, string legality)
	{
		LF_CatalogueItem item = new LF_CatalogueItem();
		item.m_sId = id;
		item.m_Prefab = prefab;
		item.m_sCategory = category;
		item.m_iPrice = price;
		item.m_iSellPrice = sellPrice;
		item.m_sLegality = legality;
		return item;
	}

	//------------------------------------------------------------------------------------------------
	LF_EItemCategory GetCategory()
	{
		return LF_ItemCatalogue.ParseCategory(m_sCategory);
	}

	//------------------------------------------------------------------------------------------------
	LF_EItemLegality GetLegality()
	{
		return LF_ItemCatalogue.ParseLegality(m_sLegality);
	}
}

//------------------------------------------------------------------------------------------------
//! One line of an outlet: an item a counter sells and the gate in front of it. A shop names its
//! outlet (LF_ShopComponent.m_sOutletId); the outlet is every offer with that outlet id. Every gate
//! that is set must pass; the refusal names the first one that does not.
[BaseContainerProps()]
class LF_OutletOffer : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Outlet id a shop counter names (gun_counter, clothing_rack, police_armory)")]
	string m_sOutlet;

	[Attribute("", UIWidgets.EditBox, "Catalogue item id this outlet sells")]
	string m_sItem;

	[Attribute("", UIWidgets.EditBox, "Gate: licence the buyer must hold (LF_ELicenseType name). Empty is no licence gate")]
	string m_sLicence;

	[Attribute("", UIWidgets.EditBox, "Gate: job the buyer must be in (POLICE is an on-duty officer and the rank is the police rank; any other id is an LF_EJobType name the buyer works now and the rank is the job level). Empty is no job gate")]
	string m_sJob;

	[Attribute("0", UIWidgets.Auto, "Gate with the job: the lowest rank (police rank index, or job level) that may buy", params: "0 100")]
	int m_iMinRank;

	[Attribute("", UIWidgets.EditBox, "Gate: organisation right the buyer's rank must carry (LF_EOrganizationPermission name). Empty is no right gate")]
	string m_sOrgRight;

	[Attribute("0", UIWidgets.CheckBox, "Hidden: the outlet no longer sells the item; a later layer can set it back. The remove verb takes the offer out of the list instead")]
	bool m_bRemoved;

	//------------------------------------------------------------------------------------------------
	static LF_OutletOffer Make(string outlet, string item)
	{
		LF_OutletOffer offer = new LF_OutletOffer();
		offer.m_sOutlet = outlet;
		offer.m_sItem = item;
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	//! The offer's stable key: "outlet/item".
	string Key()
	{
		return LF_ItemCatalogueConfig.OfferKey(m_sOutlet, m_sItem);
	}
}

//------------------------------------------------------------------------------------------------
//! The item catalogue and its outlets (#988): Configs/Catalogue/LF_ItemCatalogue.conf, then each
//! pack overlay of this class (D4), then the owner's `catalogue` section, then (on a client) the
//! server's streamed values. Every layer adds, patches or removes by id; a list is never replaced
//! whole. The core ships one example per outlet type so the core runs alone.
[BaseContainerProps(configRoot: true)]
class LF_ItemCatalogueConfig
{
	static const ResourceName CONFIG_PATH = "{5D1D3DEDC5B4BFA2}Configs/Catalogue/LF_ItemCatalogue.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "catalogue";

	static const int PRICE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int PRICE_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RANK_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RANK_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("", UIWidgets.Object, "Catalogue items, keyed by id")]
	ref array<ref LF_CatalogueItem> m_aItems;

	[Attribute("", UIWidgets.Object, "Outlet offers, keyed by outlet/item: which counter sells which item behind which gate")]
	ref array<ref LF_OutletOffer> m_aOffers;

	protected static ref LF_ItemCatalogueConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered catalogue, or null (logged once) when the core file does not resolve.
	static LF_ItemCatalogueConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_ItemCatalogue.FEATURE, string.Format("item catalogue does not resolve, no outlet sells anything: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ItemCatalogueConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(LF_ItemCatalogue.FEATURE, string.Format("item catalogue has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers(CONFIG_PATH);
		s_Config.Check(true);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the core file again (/configreload, a stream).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_ItemCatalogueConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	static string OfferKey(string outlet, string item)
	{
		return outlet + "/" + item;
	}

	//------------------------------------------------------------------------------------------------
	//! The item with this id, removed or not; null when there is none.
	LF_CatalogueItem FindItem(string id)
	{
		if (!m_aItems)
			return null;

		foreach (LF_CatalogueItem item : m_aItems)
		{
			if (item && item.m_sId == id)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The offer with this outlet and item, removed or not; null when there is none.
	LF_OutletOffer FindOffer(string outlet, string item)
	{
		if (!m_aOffers)
			return null;

		foreach (LF_OutletOffer offer : m_aOffers)
		{
			if (offer && offer.m_sOutlet == outlet && offer.m_sItem == item)
				return offer;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers every overlay over this instance, which holds the core `.conf`: pack overlays in list
	//! order, the owner section, and on a client the server's streamed section. Every layer adds,
	//! replaces, patches or removes by id through the shared merge (LF_ConfigOverlay, #962).
	//! \param[in] coreConf The core file, to ask the pack source for its overlays.
	void ApplyLayers(ResourceName coreConf)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, LF_ItemCatalogueConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("catalogue: pack overlay %1 changed %2 entries", names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			ApplyOwner(reader, LF_ConfigService.GetOwnerSource());
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("catalogue: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		// Client: the server's effective lists (WriteStream) replace the client's, so an entry the
		// server removed is gone here too. Null on the server.
		LF_ConfigReader streamed = LF_ConfigStream.OpenReceivedSection(SECTION);
		if (streamed)
		{
			m_aItems = new array<ref LF_CatalogueItem>();
			m_aOffers = new array<ref LF_OutletOffer>();
			ApplyOwner(streamed, "the server's stream");
			LF_Debug.Info(LF_ConfigService.FEATURE, "catalogue: using the server's values");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Merges one pack overlay's items and offers by id (LF_ConfigOverlay).
	//! \param[in] source The overlay file, for log lines and the two-pack warning.
	//! \return Entries changed.
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		EnsureLists();
		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_CatalogueItemList(this), container, "m_aItems", source, claims);
		changed += LF_ConfigOverlay.ApplyContainerList(new LF_OutletOfferList(this), container, "m_aOffers", source, claims);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "catalogue" section (or the server's stream, which has the same shape).
	//! `items` is keyed by item id and `offers` by "outlet/item", each record with an optional "op"
	//! (add, replace, patch, remove). With no op an existing key changes only the fields given and a
	//! new key adds a record; a new or replaced item must set a prefab that loads.
	//! \param[in] reader A reader on the section.
	//! \param[in] source The file, for log lines.
	void ApplyOwner(notnull LF_ConfigReader reader, string source = "config.json")
	{
		array<string> ownerKeys = {"items", "offers"};
		reader.WarnUnknownKeys(ownerKeys);
		EnsureLists();
		LF_ConfigOverlay.ApplyRecords(reader, "items", new LF_CatalogueItemList(this), source);
		LF_ConfigOverlay.ApplyRecords(reader, "offers", new LF_OutletOfferList(this), source);
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureLists()
	{
		if (!m_aItems)
			m_aItems = new array<ref LF_CatalogueItem>();
		if (!m_aOffers)
			m_aOffers = new array<ref LF_OutletOffer>();
	}

	//------------------------------------------------------------------------------------------------
	//! Reads an item's owner fields (LF_CatalogueItemList).
	void ReadItem(notnull LF_CatalogueItem item, notnull LF_ConfigReader reader)
	{
		array<string> itemKeys = {"op", "prefab", "category", "price", "sellPrice", "legality", "licence", "concealsFace", "concealsBuild", "removed"};
		reader.WarnUnknownKeys(itemKeys);
		reader.ReadResource("prefab", item.m_Prefab);
		string category = item.m_sCategory;
		if (reader.ReadString("category", category))
			item.m_sCategory = KnownWord(reader, "category", category, item.m_sCategory, true);
		reader.ReadInt("price", item.m_iPrice, PRICE_MIN, PRICE_MAX);
		reader.ReadInt("sellPrice", item.m_iSellPrice, PRICE_MIN, PRICE_MAX);
		string legality = item.m_sLegality;
		if (reader.ReadString("legality", legality))
			item.m_sLegality = KnownWord(reader, "legality", legality, item.m_sLegality, false);
		reader.ReadString("licence", item.m_sLicence);
		reader.ReadBool("concealsFace", item.m_bConcealsFace);
		reader.ReadBool("concealsBuild", item.m_bConcealsBuild);
		reader.ReadBool("removed", item.m_bRemoved);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads an offer's owner fields (LF_OutletOfferList).
	void ReadOffer(notnull LF_OutletOffer offer, notnull LF_ConfigReader reader)
	{
		array<string> offerKeys = {"op", "licence", "job", "minRank", "orgRight", "removed"};
		reader.WarnUnknownKeys(offerKeys);
		reader.ReadString("licence", offer.m_sLicence);
		reader.ReadString("job", offer.m_sJob);
		reader.ReadInt("minRank", offer.m_iMinRank, RANK_MIN, RANK_MAX);
		reader.ReadString("orgRight", offer.m_sOrgRight);
		reader.ReadBool("removed", offer.m_bRemoved);
	}

	//------------------------------------------------------------------------------------------------
	//! A category or legality word the reader gave: the word when the catalogue knows it, else the
	//! kept value (logged).
	protected static string KnownWord(notnull LF_ConfigReader reader, string key, string word, string kept, bool category)
	{
		bool known;
		if (category)
			known = LF_ItemCatalogue.ParseCategory(word) != LF_EItemCategory.UNKNOWN;
		else
			known = LF_ItemCatalogue.ParseLegality(word) != LF_EItemLegality.UNKNOWN;

		if (!known)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1.%2 = %3 is not a %4 the catalogue knows; keeping %5", reader.GetPath(), key, word, key, kept));
			return kept;
		}

		return word;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the effective catalogue into the config stream, in the owner's shape, so a client's
	//! price list shows what the server charges. Removed records are written with removed: true, so
	//! the client drops what the server dropped.
	//! \param[in] ctx A save context with this section's object open.
	void WriteStream(notnull SCR_JsonSaveContext ctx)
	{
		ctx.StartObject("items");
		if (m_aItems)
		{
			foreach (LF_CatalogueItem item : m_aItems)
			{
				if (!item || item.m_sId.IsEmpty())
					continue;

				ctx.StartObject(item.m_sId);
				string prefab = item.m_Prefab;
				ctx.WriteValue("prefab", prefab);
				ctx.WriteValue("category", item.m_sCategory);
				ctx.WriteValue("price", item.m_iPrice);
				ctx.WriteValue("sellPrice", item.m_iSellPrice);
				ctx.WriteValue("legality", item.m_sLegality);
				ctx.WriteValue("licence", item.m_sLicence);
				ctx.WriteValue("concealsFace", item.m_bConcealsFace);
				ctx.WriteValue("concealsBuild", item.m_bConcealsBuild);
				ctx.WriteValue("removed", item.m_bRemoved);
				ctx.EndObject();
			}
		}
		ctx.EndObject();

		ctx.StartObject("offers");
		if (m_aOffers)
		{
			foreach (LF_OutletOffer offer : m_aOffers)
			{
				if (!offer || offer.m_sOutlet.IsEmpty() || offer.m_sItem.IsEmpty())
					continue;

				ctx.StartObject(offer.Key());
				ctx.WriteValue("licence", offer.m_sLicence);
				ctx.WriteValue("job", offer.m_sJob);
				ctx.WriteValue("minRank", offer.m_iMinRank);
				ctx.WriteValue("orgRight", offer.m_sOrgRight);
				ctx.WriteValue("removed", offer.m_bRemoved);
				ctx.EndObject();
			}
		}
		ctx.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! The load-time checks, after every layer. A broken record degrades that record only:
	//! - an item with no prefab, a prefab that does not resolve (when checkResources), or a DLC-owned
	//!   prefab is removed (logged);
	//! - a sell price above the price is lowered to the price (logged), so no outlet is a profit loop;
	//! - an offer naming an item the catalogue lacks is removed (logged).
	//! \param[in] checkResources Whether to load each prefab (false in LOGIC tests: no resources).
	//! \return Records changed.
	int Check(bool checkResources)
	{
		int changed;
		if (m_aItems)
		{
			foreach (LF_CatalogueItem item : m_aItems)
			{
				if (!item || item.m_bRemoved)
					continue;

				if (item.m_Prefab.IsEmpty())
				{
					LF_Debug.Error(LF_ItemCatalogue.FEATURE, string.Format("item %1 has no prefab; removed", item.m_sId));
					item.m_bRemoved = true;
					changed++;
					continue;
				}

				if (LF_ItemCatalogue.IsDlcOwned(item.m_Prefab))
				{
					LF_Debug.Error(LF_ItemCatalogue.FEATURE, string.Format("item %1 is a DLC-owned prefab, which a player without the DLC cannot use; removed: %2", item.m_sId, item.m_Prefab));
					item.m_bRemoved = true;
					changed++;
					continue;
				}

				if (checkResources)
				{
					Resource resource = Resource.Load(item.m_Prefab);
					if (!resource || !resource.IsValid())
					{
						LF_Debug.Error(LF_ItemCatalogue.FEATURE, string.Format("item %1 prefab does not resolve; removed: %2", item.m_sId, item.m_Prefab));
						item.m_bRemoved = true;
						changed++;
						continue;
					}
				}

				if (item.m_iSellPrice > item.m_iPrice)
				{
					LF_Debug.Error(LF_ItemCatalogue.FEATURE, string.Format("item %1 sells back for %2, above its price %3 (a profit loop); sell price lowered to %3", item.m_sId, item.m_iSellPrice, item.m_iPrice));
					item.m_iSellPrice = item.m_iPrice;
					changed++;
				}
			}
		}

		if (m_aOffers)
		{
			foreach (LF_OutletOffer offer : m_aOffers)
			{
				if (!offer || offer.m_bRemoved)
					continue;

				if (!FindItem(offer.m_sItem))
				{
					LF_Debug.Error(LF_ItemCatalogue.FEATURE, string.Format("outlet %1 offers %2, which the catalogue does not list; removed", offer.m_sOutlet, offer.m_sItem));
					offer.m_bRemoved = true;
					changed++;
				}
			}
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! One pack container field into a record field, only when the overlay writes it. \return 1 when set.
class LF_CatalogueFields
{
	static int Copy(BaseContainer from, string field, inout string value)
	{
		if (from.IsVariableSetDirectly(field) && from.Get(field, value))
			return 1;
		return 0;
	}

	static int CopyInt(BaseContainer from, string field, inout int value)
	{
		if (from.IsVariableSetDirectly(field) && from.Get(field, value))
			return 1;
		return 0;
	}

	static int CopyBool(BaseContainer from, string field, inout bool value)
	{
		if (from.IsVariableSetDirectly(field) && from.Get(field, value))
			return 1;
		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! The catalogue's `items` for the shared merge (LF_ConfigOverlay), keyed by item id.
class LF_CatalogueItemList : LF_ConfigList
{
	protected LF_ItemCatalogueConfig m_Config;

	void LF_CatalogueItemList(LF_ItemCatalogueConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "catalogue.items";
	}

	override int Count()
	{
		return m_Config.m_aItems.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aItems[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_CatalogueItem.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "an item id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_CatalogueItem.Make(id, ResourceName.Empty, "equipment", 100, 50, "legal");
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_CatalogueItem item = LF_CatalogueItem.Cast(record);
		if (index < m_Config.m_aItems.Count())
			m_Config.m_aItems.Set(index, item);
		else
			m_Config.m_aItems.Insert(item);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aItems.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_CatalogueItem item = LF_CatalogueItem.Cast(record);
		string prefab = item.m_Prefab;
		int changed = LF_CatalogueFields.Copy(container, "m_Prefab", prefab);
		item.m_Prefab = prefab;
		changed += LF_CatalogueFields.Copy(container, "m_sCategory", item.m_sCategory);
		changed += LF_CatalogueFields.CopyInt(container, "m_iPrice", item.m_iPrice);
		changed += LF_CatalogueFields.CopyInt(container, "m_iSellPrice", item.m_iSellPrice);
		changed += LF_CatalogueFields.Copy(container, "m_sLegality", item.m_sLegality);
		changed += LF_CatalogueFields.Copy(container, "m_sLicence", item.m_sLicence);
		changed += LF_CatalogueFields.CopyBool(container, "m_bConcealsFace", item.m_bConcealsFace);
		changed += LF_CatalogueFields.CopyBool(container, "m_bConcealsBuild", item.m_bConcealsBuild);
		changed += LF_CatalogueFields.CopyBool(container, "m_bRemoved", item.m_bRemoved);
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		m_Config.ReadItem(LF_CatalogueItem.Cast(record), reader);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (LF_CatalogueItem.Cast(record).m_Prefab.IsEmpty())
			return "a new or replaced item needs a prefab that loads";
		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The catalogue's `offers` for the shared merge (LF_ConfigOverlay), keyed by "outlet/item".
class LF_OutletOfferList : LF_ConfigList
{
	protected LF_ItemCatalogueConfig m_Config;

	void LF_OutletOfferList(LF_ItemCatalogueConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "catalogue.offers";
	}

	override int Count()
	{
		return m_Config.m_aOffers.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aOffers[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_OutletOffer.Cast(record).Key();
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string outlet;
		string item;
		container.Get("m_sOutlet", outlet);
		container.Get("m_sItem", item);
		if (outlet.IsEmpty() || item.IsEmpty())
			return string.Empty;
		return LF_ItemCatalogueConfig.OfferKey(outlet, item);
	}

	override string CanonicalId(string key)
	{
		int slash = key.IndexOf("/");
		if (slash <= 0 || slash >= key.Length() - 1)
			return string.Empty;
		return key;
	}

	override string IdHint()
	{
		return "outlet/item";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		int slash = id.IndexOf("/");
		return LF_OutletOffer.Make(id.Substring(0, slash), id.Substring(slash + 1, id.Length() - slash - 1));
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_OutletOffer offer = LF_OutletOffer.Cast(record);
		if (index < m_Config.m_aOffers.Count())
			m_Config.m_aOffers.Set(index, offer);
		else
			m_Config.m_aOffers.Insert(offer);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aOffers.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_OutletOffer offer = LF_OutletOffer.Cast(record);
		int changed = LF_CatalogueFields.Copy(container, "m_sLicence", offer.m_sLicence);
		changed += LF_CatalogueFields.Copy(container, "m_sJob", offer.m_sJob);
		changed += LF_CatalogueFields.CopyInt(container, "m_iMinRank", offer.m_iMinRank);
		changed += LF_CatalogueFields.Copy(container, "m_sOrgRight", offer.m_sOrgRight);
		changed += LF_CatalogueFields.CopyBool(container, "m_bRemoved", offer.m_bRemoved);
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		m_Config.ReadOffer(LF_OutletOffer.Cast(record), reader);
	}
}
