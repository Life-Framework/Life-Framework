//------------------------------------------------------------------------------------------------
//! One item of the catalogue (#988): what it is, what it costs, and what the law says about it.
//! The id is the stable key packs and the owner patch by; never rename one.
[BaseContainerProps()]
class LF_CatalogueItem
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

	[Attribute("0", UIWidgets.CheckBox, "Removed by a pack or the owner: no outlet sells it")]
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
class LF_OutletOffer
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

	[Attribute("0", UIWidgets.CheckBox, "Removed by a pack or the owner: the outlet no longer sells the item")]
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
	//! order, the owner section, and on a client the server's streamed section.
	//! \param[in] coreConf The core file, to ask the pack source for its overlays.
	void ApplyLayers(ResourceName coreConf)
	{
		array<ResourceName> overlays = {};
		LF_ConfigService.GetPackOverlays(coreConf, overlays);
		foreach (ResourceName overlay : overlays)
		{
			Resource resource = BaseContainerTools.LoadContainer(overlay);
			if (!resource || !resource.IsValid() || !resource.GetResource())
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("catalogue: pack overlay does not resolve, skipped: %1", overlay));
				continue;
			}

			BaseContainer container = resource.GetResource().ToBaseContainer();
			typename overlayType;
			if (container)
				overlayType = container.GetClassName().ToType();
			if (!overlayType || !overlayType.IsInherited(LF_ItemCatalogueConfig))
				continue;

			if (overlay == coreConf)
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("catalogue: the pack lists the core file as an overlay, skipped: %1", overlay));
				continue;
			}

			int changed = ApplyContainer(container);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("catalogue: pack overlay %1 set %2 fields", overlay, changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			ApplyOwner(reader);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("catalogue: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		// Client: the server's effective values, written in the owner's shape (WriteStream). Null on the server.
		LF_ConfigReader streamed = LF_ConfigStream.OpenReceivedSection(SECTION);
		if (streamed)
		{
			ApplyOwner(streamed);
			LF_Debug.Info(LF_ConfigService.FEATURE, "catalogue: using the server's values");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Merges one pack overlay: an item or offer whose key the catalogue has changes only the fields
	//! the overlay writes; a new key is added whole.
	//! \return Fields set and records added.
	int ApplyContainer(notnull BaseContainer container)
	{
		int changed;
		if (!m_aItems)
			m_aItems = new array<ref LF_CatalogueItem>();
		if (!m_aOffers)
			m_aOffers = new array<ref LF_OutletOffer>();

		BaseContainerList items = container.GetObjectArray("m_aItems");
		if (items)
		{
			for (int i = 0; i < items.Count(); i++)
			{
				BaseContainer itemContainer = items.Get(i);
				if (!itemContainer)
					continue;

				string id;
				itemContainer.Get("m_sId", id);
				if (id.IsEmpty())
				{
					LF_Debug.Error(LF_ConfigService.FEATURE, "catalogue: a pack item has no id; skipped");
					continue;
				}

				LF_CatalogueItem item = FindItem(id);
				if (!item)
				{
					item = LF_CatalogueItem.Cast(BaseContainerTools.CreateInstanceFromContainer(itemContainer));
					if (!item)
						continue;
					m_aItems.Insert(item);
					changed++;
					continue;
				}

				changed += SetIfWritten(itemContainer, "m_Prefab", item);
				changed += SetIfWritten(itemContainer, "m_sCategory", item);
				changed += SetIfWritten(itemContainer, "m_iPrice", item);
				changed += SetIfWritten(itemContainer, "m_iSellPrice", item);
				changed += SetIfWritten(itemContainer, "m_sLegality", item);
				changed += SetIfWritten(itemContainer, "m_sLicence", item);
				changed += SetIfWritten(itemContainer, "m_bConcealsFace", item);
				changed += SetIfWritten(itemContainer, "m_bConcealsBuild", item);
				changed += SetIfWritten(itemContainer, "m_bRemoved", item);
			}
		}

		BaseContainerList offers = container.GetObjectArray("m_aOffers");
		if (offers)
		{
			for (int o = 0; o < offers.Count(); o++)
			{
				BaseContainer offerContainer = offers.Get(o);
				if (!offerContainer)
					continue;

				string outlet;
				string itemId;
				offerContainer.Get("m_sOutlet", outlet);
				offerContainer.Get("m_sItem", itemId);
				if (outlet.IsEmpty() || itemId.IsEmpty())
				{
					LF_Debug.Error(LF_ConfigService.FEATURE, "catalogue: a pack offer has no outlet or item; skipped");
					continue;
				}

				LF_OutletOffer offer = FindOffer(outlet, itemId);
				if (!offer)
				{
					offer = LF_OutletOffer.Cast(BaseContainerTools.CreateInstanceFromContainer(offerContainer));
					if (!offer)
						continue;
					m_aOffers.Insert(offer);
					changed++;
					continue;
				}

				changed += SetOfferIfWritten(offerContainer, "m_sLicence", offer);
				changed += SetOfferIfWritten(offerContainer, "m_sJob", offer);
				changed += SetOfferIfWritten(offerContainer, "m_iMinRank", offer);
				changed += SetOfferIfWritten(offerContainer, "m_sOrgRight", offer);
				changed += SetOfferIfWritten(offerContainer, "m_bRemoved", offer);
			}
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! One item field from a pack container, only when the overlay writes it. \return 1 when set.
	protected static int SetIfWritten(BaseContainer from, string field, LF_CatalogueItem item)
	{
		if (!from.IsVariableSetDirectly(field))
			return 0;

		bool ok;
		if (field == "m_Prefab")
			ok = from.Get(field, item.m_Prefab);
		else if (field == "m_sCategory")
			ok = from.Get(field, item.m_sCategory);
		else if (field == "m_iPrice")
			ok = from.Get(field, item.m_iPrice);
		else if (field == "m_iSellPrice")
			ok = from.Get(field, item.m_iSellPrice);
		else if (field == "m_sLegality")
			ok = from.Get(field, item.m_sLegality);
		else if (field == "m_sLicence")
			ok = from.Get(field, item.m_sLicence);
		else if (field == "m_bConcealsFace")
			ok = from.Get(field, item.m_bConcealsFace);
		else if (field == "m_bConcealsBuild")
			ok = from.Get(field, item.m_bConcealsBuild);
		else if (field == "m_bRemoved")
			ok = from.Get(field, item.m_bRemoved);

		if (ok)
			return 1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! One offer field from a pack container, only when the overlay writes it. \return 1 when set.
	protected static int SetOfferIfWritten(BaseContainer from, string field, LF_OutletOffer offer)
	{
		if (!from.IsVariableSetDirectly(field))
			return 0;

		bool ok;
		if (field == "m_sLicence")
			ok = from.Get(field, offer.m_sLicence);
		else if (field == "m_sJob")
			ok = from.Get(field, offer.m_sJob);
		else if (field == "m_iMinRank")
			ok = from.Get(field, offer.m_iMinRank);
		else if (field == "m_sOrgRight")
			ok = from.Get(field, offer.m_sOrgRight);
		else if (field == "m_bRemoved")
			ok = from.Get(field, offer.m_bRemoved);

		if (ok)
			return 1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "catalogue" section (or the server's stream, which has the same shape).
	//! `items` is keyed by item id and `offers` by "outlet/item". An existing key changes only the
	//! fields given; a new key adds a record, and a new item must set prefab; `removed: true` takes a
	//! record out without deleting it, so a later layer can put it back.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"items", "offers"};
		reader.WarnUnknownKeys(ownerKeys);

		if (!m_aItems)
			m_aItems = new array<ref LF_CatalogueItem>();
		if (!m_aOffers)
			m_aOffers = new array<ref LF_OutletOffer>();

		int count = reader.BeginRecords("items");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			array<string> itemKeys = {"prefab", "category", "price", "sellPrice", "legality", "licence", "concealsFace", "concealsBuild", "removed"};
			reader.WarnUnknownKeys(itemKeys);

			LF_CatalogueItem item = FindItem(key);
			bool added;
			if (!item)
			{
				if (key.IsEmpty() || !reader.Has("prefab"))
				{
					reader.RejectRecord("a new item needs an id and a prefab");
					reader.EndRecord();
					continue;
				}

				item = LF_CatalogueItem.Make(key, ResourceName.Empty, "equipment", 100, 50, "legal");
				added = true;
			}

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
			if (added && item.m_Prefab.IsEmpty())
			{
				reader.RejectRecord("a new item's prefab does not load");
				reader.EndRecord();
				continue;
			}

			if (added)
				m_aItems.Insert(item);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();

		count = reader.BeginRecords("offers");
		for (int o = 0; o < count; o++)
		{
			string offerKey = reader.GetRecordKey(o);
			if (!reader.BeginRecord(offerKey))
				continue;

			array<string> offerKeys = {"licence", "job", "minRank", "orgRight", "removed"};
			reader.WarnUnknownKeys(offerKeys);

			int slash = offerKey.IndexOf("/");
			if (slash <= 0 || slash >= offerKey.Length() - 1)
			{
				reader.RejectRecord("the key is not outlet/item");
				reader.EndRecord();
				continue;
			}

			string outlet = offerKey.Substring(0, slash);
			string itemId = offerKey.Substring(slash + 1, offerKey.Length() - slash - 1);
			LF_OutletOffer offer = FindOffer(outlet, itemId);
			bool addedOffer;
			if (!offer)
			{
				offer = LF_OutletOffer.Make(outlet, itemId);
				addedOffer = true;
			}

			reader.ReadString("licence", offer.m_sLicence);
			reader.ReadString("job", offer.m_sJob);
			reader.ReadInt("minRank", offer.m_iMinRank, RANK_MIN, RANK_MAX);
			reader.ReadString("orgRight", offer.m_sOrgRight);
			reader.ReadBool("removed", offer.m_bRemoved);
			if (addedOffer)
				m_aOffers.Insert(offer);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
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
