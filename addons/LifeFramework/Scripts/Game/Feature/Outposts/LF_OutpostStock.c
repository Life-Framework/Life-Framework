//------------------------------------------------------------------------------------------------
//! The quartermaster's crate (0.3.0 gangs O2, decisions N1 and N29). Each stocked item is one row:
//! a prefab (or a prefab slot a pack fills), a price and the lowest underworld tier the
//! quartermaster sells it at. The crate carries one Buy action per row; the server decides every
//! sale (LF_OutpostStock.Buy): the outpost's officer refusal (#4217), the tier gate, the cash, the
//! item. A refusal is said in words, never with a tier or a points number (N1).
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_OutpostStockItem
{
	[Attribute("", UIWidgets.EditBox, "Stable row id, for example pistol")]
	string m_sId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item prefab. Empty uses the prefab slot", params: "et")]
	ResourceName m_Prefab;

	[Attribute("", UIWidgets.EditBox, "Prefab slot id (LF_PrefabSlots) used when the prefab is empty, so a pack can supply its own item")]
	string m_sPrefabSlot;

	[Attribute("", UIWidgets.EditBox, "# key of the item's name on the Buy action")]
	string m_sNameKey;

	[Attribute("0", UIWidgets.EditBox, "Price in dollars", params: "0 1000000")]
	int m_iPrice;

	[Attribute("0", UIWidgets.EditBox, "Lowest underworld tier that may buy it (-2..3 with the shipped ladder)", params: "-10 10")]
	int m_iMinTier;

	//------------------------------------------------------------------------------------------------
	static LF_OutpostStockItem Make(string id, ResourceName prefab, string prefabSlot, string nameKey, int price, int minTier)
	{
		LF_OutpostStockItem item = new LF_OutpostStockItem();
		item.m_sId = id;
		item.m_Prefab = prefab;
		item.m_sPrefabSlot = prefabSlot;
		item.m_sNameKey = nameKey;
		item.m_iPrice = price;
		item.m_iMinTier = minTier;
		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The prefab to hand over: the row's own, else its slot's, else empty (logged by the slot).
	ResourceName ResolvePrefab()
	{
		if (!m_Prefab.IsEmpty())
			return m_Prefab;

		if (m_sPrefabSlot.IsEmpty())
			return ResourceName.Empty;

		return LF_PrefabSlots.Get(m_sPrefabSlot);
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EOutpostBuy
{
	OK,
	BAD_INPUT,
	REFUSED_OFFICER,
	NOT_KNOWN,
	CASH_SHORT,
	NO_PREFAB,
	NO_ROOM
}

//------------------------------------------------------------------------------------------------
class LF_OutpostStock
{
	static const string FEATURE = "Outposts";
	static const string REASON_BUY = "outpost-buy";
	static const string TITLE_KEY = "#LF-OutpostStock_Title";

	static const ResourceName PISTOL_PREFAB = "{C0F7DD85A86B2900}Prefabs/Weapons/Handguns/PM/Handgun_PM.et";
	static const ResourceName RIFLE_PREFAB = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
	static const string GANG_RADIO_SLOT = "gang_radio";

	//------------------------------------------------------------------------------------------------
	//! The north crate's stock when the crate's own list is empty (N29: pistol $400 to $600 at tier 1,
	//! rifle $1,500 to $2,500 at tier 2; the gang radio at $60 for anyone, #4221). The pack's stock
	//! tables (O6) replace it on the placed crate.
	static void DefaultStock(notnull array<ref LF_OutpostStockItem> outStock)
	{
		outStock.Clear();
		outStock.Insert(LF_OutpostStockItem.Make("gang_radio", ResourceName.Empty, GANG_RADIO_SLOT, "#LF-OutpostStock_GangRadio", 60, 0));
		outStock.Insert(LF_OutpostStockItem.Make("pistol", PISTOL_PREFAB, "", "#LF-OutpostStock_Pistol", 500, 1));
		outStock.Insert(LF_OutpostStockItem.Make("rifle", RIFLE_PREFAB, "", "#LF-OutpostStock_Rifle", 2000, 2));
	}

	//------------------------------------------------------------------------------------------------
	//! The pure gate: OK when a buyer at `tier` may take `item`, else why not.
	static LF_EOutpostBuy Gate(LF_OutpostStockItem item, int tier)
	{
		if (!item || item.m_iPrice < 0)
			return LF_EOutpostBuy.BAD_INPUT;

		if (tier < item.m_iMinTier)
			return LF_EOutpostBuy.NOT_KNOWN;

		return LF_EOutpostBuy.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The buyer's underworld tier (server state; 0 for a body with no uid).
	static int TierOf(IEntity buyer)
	{
		return LF_Reputation.Tier(LF_Utils.GetPlayerUid(buyer), LF_Reputation.FACTION_UNDERWORLD);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells `item` to `buyer` at the crate at `traderPos`. Order: the outpost's officer refusal
	//! (told by LF_Outpost.NotifyRefusal), the tier gate, the cash, the item; a failed hand-over
	//! refunds the cash. The caller tells the buyer every other result (RefusalKey).
	//! \param[in] tier The buyer's underworld tier (TierOf in play).
	//! \param[out] paid Dollars taken; 0 unless OK.
	static LF_EOutpostBuy Buy(IEntity buyer, LF_OutpostStockItem item, vector traderPos, int tier, out int paid)
	{
		paid = 0;
		if (Replication.IsRunning() && !Replication.IsServer())
			return LF_EOutpostBuy.BAD_INPUT;

		if (!buyer || !item)
		{
			LF_Debug.Error(FEATURE, string.Format("outpost buy refused: buyer %1 item %2", buyer != null, item != null));
			return LF_EOutpostBuy.BAD_INPUT;
		}

		if (LF_Outpost.RefusesService(buyer, traderPos))
		{
			LF_Outpost.NotifyRefusal(buyer);
			return LF_EOutpostBuy.REFUSED_OFFICER;
		}

		LF_EOutpostBuy gate = Gate(item, tier);
		if (gate != LF_EOutpostBuy.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("outpost buy of %1 refused: tier below the row's (%2)", item.m_sId, typename.EnumToString(LF_EOutpostBuy, gate)));
			return gate;
		}

		ResourceName prefab = item.ResolvePrefab();
		if (prefab.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("outpost stock row %1 has no prefab (slot '%2'); not sold", item.m_sId, item.m_sPrefabSlot));
			return LF_EOutpostBuy.NO_PREFAB;
		}

		string uid = LF_Utils.GetPlayerUid(buyer);
		int taken;
		if (item.m_iPrice > 0)
		{
			taken = LF_MoneyUtils.RemoveAmount(buyer, item.m_iPrice, REASON_BUY, item.m_sId);
			if (taken != item.m_iPrice)
			{
				if (taken > 0)
					LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, item.m_sId, true);

				LF_Debug.Info(FEATURE, string.Format("outpost buy of %1 refused: %2 had %3 of %4, refunded", item.m_sId, uid, taken, item.m_iPrice));
				return LF_EOutpostBuy.CASH_SHORT;
			}
		}

		if (LF_InventoryUtils.AddAmount(buyer, prefab, 1, true) < 1)
		{
			if (taken > 0)
				LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, item.m_sId, true);

			LF_Debug.Warn(FEATURE, string.Format("outpost buy of %1: %2 did not spawn for %3, refunded", item.m_sId, prefab, uid));
			return LF_EOutpostBuy.NO_ROOM;
		}

		paid = taken;
		LF_Debug.Info(FEATURE, string.Format("%1 bought %2 at the outpost for %3", uid, item.m_sId, paid));
		return LF_EOutpostBuy.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The `#` key that tells the buyer the result; never a number (N1).
	static string RefusalKey(LF_EOutpostBuy result)
	{
		switch (result)
		{
			case LF_EOutpostBuy.OK:
				return "#LF-OutpostStock_Bought";
			case LF_EOutpostBuy.NOT_KNOWN:
				return "#LF-OutpostStock_NotKnown";
			case LF_EOutpostBuy.CASH_SHORT:
				return "#LF-OutpostStock_NoCash";
		}

		return "#LF-OutpostStock_Unavailable";
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Outposts", description: "A quartermaster's crate: stock rows sold by LF_OutpostStockBuyAction, gated by underworld tier")]
class LF_OutpostStockComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_OutpostStockComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object, "Stock rows; empty uses LF_OutpostStock.DefaultStock")]
	protected ref array<ref LF_OutpostStockItem> m_aStock;

	protected ref array<ref LF_OutpostStockItem> m_aDefault;

	//------------------------------------------------------------------------------------------------
	//! \return The crate's rows (the default stock when the prefab lists none).
	array<ref LF_OutpostStockItem> GetStock()
	{
		if (m_aStock && !m_aStock.IsEmpty())
			return m_aStock;

		if (!m_aDefault)
		{
			m_aDefault = new array<ref LF_OutpostStockItem>();
			LF_OutpostStock.DefaultStock(m_aDefault);
		}

		return m_aDefault;
	}

	//------------------------------------------------------------------------------------------------
	LF_OutpostStockItem GetItem(int index)
	{
		array<ref LF_OutpostStockItem> stock = GetStock();
		if (index < 0 || index >= stock.Count())
			return null;

		return stock[index];
	}
}

//------------------------------------------------------------------------------------------------
//! One Buy row on the crate: the prefab lists one action per stock index (0, 1, 2, ...); an index past
//! the stock is hidden. The client only names the row; the server re-checks everything in Buy.
class LF_OutpostStockBuyAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Stock row index this action buys", params: "0 31")]
	protected int m_iIndex;

	protected LF_OutpostStockComponent m_Crate;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		m_Crate = LF_Component<LF_OutpostStockComponent>.Find(pOwnerEntity);
		if (!m_Crate)
			LF_Debug.Error(LF_OutpostStock.FEATURE, "outpost Buy action on an entity with no LF_OutpostStockComponent; action hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Crate && m_Crate.GetItem(m_iIndex);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Crate)
			return false;

		LF_OutpostStockItem item = m_Crate.GetItem(m_iIndex);
		if (!item)
			return false;

		outName = WidgetManager.Translate("#LF-OutpostStock_Buy", WidgetManager.Translate(item.m_sNameKey), LF_MoneyFormat.FormatMoney(item.m_iPrice));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !m_Crate || !pUserEntity)
			return;

		LF_OutpostStockItem item = m_Crate.GetItem(m_iIndex);
		int paid;
		LF_EOutpostBuy result = LF_OutpostStock.Buy(pUserEntity, item, pOwnerEntity.GetOrigin(), LF_OutpostStock.TierOf(pUserEntity), paid);
		if (result == LF_EOutpostBuy.REFUSED_OFFICER)
			return;

		string arg;
		if (result == LF_EOutpostBuy.OK)
			arg = LF_MoneyFormat.FormatMoney(paid);

		LF_Utils.NotifyEntity(pUserEntity, LF_OutpostStock.RefusalKey(result), LF_OutpostStock.TITLE_KEY, 5.0, arg);
	}
}
