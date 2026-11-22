//------------------------------------------------------------------------------------------------
//! A dressed mannequin that sells what it wears. The pieces are catalogue ids the mannequin's own
//! LF_ShopComponent outlet offers; LF_MannequinBuyAction slots read them as "Buy <piece>: $<price>"
//! and "Buy whole outfit: $<total>". The server sells through LF_ShopComponent.BuyDropOverflow (the
//! shop's price, gate, cash and stock checks; full pockets drop the piece at the buyer's feet) and
//! keeps a replicated sold-out bit per slot so a client greys a piece the outlet has run out of.
[ComponentEditorProps(category: "LifeFramework/Shop", description: "A dressed mannequin: sells each worn piece and the whole outfit through the LF_ShopComponent on the same entity")]
class LF_MannequinOutfitComponentClass : ScriptComponentClass
{
}

class LF_MannequinOutfitComponent : ScriptComponent
{
	//! How often the server re-reads the outlet's stock for the sold-out bits (a counter sale or a restock).
	static const int STOCK_CHECK_MS = 20000; // lf-fixed: a sold-out mark may lag a counter sale by this much

	[Attribute("", UIWidgets.Auto, "Catalogue item ids the mannequin wears, one per buy slot (at most 8), in the order the action list shows them. Each must be offered by the outlet of the LF_ShopComponent on this entity")]
	protected ref array<string> m_aItemIds;

	//! Bit i set: slot i is sold out at the outlet. Server state.
	[RplProp()]
	protected int m_iSoldOutMask;

	protected LF_ShopComponent m_Shop;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_aItemIds)
			m_aItemIds = {};

		m_Shop = LF_Component<LF_ShopComponent>.Find(owner);
		if (!m_Shop || m_Shop.GetOutletId().IsEmpty())
		{
			LF_Debug.Error(LF_MannequinOutfit.FEATURE, "mannequin has no LF_ShopComponent with an outlet; its buy actions stay hidden");
			return;
		}

		if (m_aItemIds.Count() > LF_MannequinOutfit.MAX_PIECES)
			LF_Debug.Warn(LF_MannequinOutfit.FEATURE, string.Format("mannequin lists %1 pieces; only the first %2 are sold", m_aItemIds.Count(), LF_MannequinOutfit.MAX_PIECES));

		if (Replication.IsServer() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(RefreshSoldOut, STOCK_CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RefreshSoldOut);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The outlet the mannequin sells under; empty when it has no shop.
	string GetOutletId()
	{
		if (!m_Shop)
			return string.Empty;

		return m_Shop.GetOutletId();
	}

	//------------------------------------------------------------------------------------------------
	//! The piece in this slot, or null (empty slot, unknown id, not sold at the outlet).
	LF_MannequinPiece GetPiece(int slot)
	{
		if (!m_Shop)
			return null;

		return LF_MannequinOutfit.Piece(LF_ItemCatalogueConfig.Get(), GetOutletId(), m_aItemIds, slot, m_iSoldOutMask);
	}

	//------------------------------------------------------------------------------------------------
	//! The display name of a piece (a #key or a plain name).
	string GetPieceName(LF_MannequinPiece piece)
	{
		if (!m_Shop || !piece)
			return string.Empty;

		return m_Shop.GetItemDisplayName(piece.m_Prefab);
	}

	//------------------------------------------------------------------------------------------------
	bool HasOutfit()
	{
		return m_Shop && LF_MannequinOutfit.HasOutfit(LF_ItemCatalogueConfig.Get(), GetOutletId(), m_aItemIds);
	}

	//------------------------------------------------------------------------------------------------
	int GetOutfitTotal()
	{
		if (!m_Shop)
			return 0;

		return LF_MannequinOutfit.OutfitTotal(LF_ItemCatalogueConfig.Get(), GetOutletId(), m_aItemIds);
	}

	//------------------------------------------------------------------------------------------------
	bool IsOutfitSoldOut()
	{
		return LF_MannequinOutfit.OutfitSoldOut(LF_ItemCatalogueConfig.Get(), GetOutletId(), m_aItemIds, m_iSoldOutMask);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: re-reads the outlet's stock; replicates only a change.
	void RefreshSoldOut()
	{
		if (!Replication.IsServer() || !m_Shop)
			return;

		int mask = LF_MannequinOutfit.SoldOutMask(LF_ItemCatalogueConfig.Get(), GetOutletId(), m_aItemIds, GetGame().GetWorld().GetWorldTime());
		if (mask == m_iSoldOutMask)
			return;

		m_iSoldOutMask = mask;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells the piece in this slot to the buyer through the outlet's till.
	//! \param[out] charged Cash taken. \param[out] dropped Units left at the buyer's feet.
	LF_EShopRefusal BuyPiece(int slot, IEntity buyer, out int charged, out int dropped)
	{
		charged = 0;
		dropped = 0;
		if (!Replication.IsServer() || !buyer || !m_Shop)
			return LF_EShopRefusal.NO_SHOP;

		LF_MannequinPiece piece = GetPiece(slot);
		if (!piece)
		{
			LF_Debug.Info(LF_MannequinOutfit.FEATURE, string.Format("buy refused: slot %1 sells nothing at %2", slot, GetOutletId()));
			return LF_EShopRefusal.NOT_SOLD_HERE;
		}

		string missing;
		LF_EShopRefusal result = m_Shop.BuyDropOverflow(piece.m_Prefab, 1, buyer, charged, missing, dropped);
		LF_Debug.Info(LF_MannequinOutfit.FEATURE, string.Format("piece %1 for %2: %3 charged=%4 dropped=%5", piece.m_sId, LF_Utils.GetPlayerUid(buyer), typename.EnumToString(LF_EShopRefusal, result), charged, dropped));
		RefreshSoldOut();
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells every piece. Nothing is bought when a piece is sold out or the buyer's cash does
	//! not cover the total; a refusal part way (a gate on one piece) keeps what was already paid for.
	//! \param[out] charged Cash taken. \param[out] bought Pieces sold. \param[out] dropped Units at the buyer's feet.
	LF_EShopRefusal BuyOutfit(IEntity buyer, out int charged, out int bought, out int dropped)
	{
		charged = 0;
		bought = 0;
		dropped = 0;
		if (!Replication.IsServer() || !buyer || !m_Shop)
			return LF_EShopRefusal.NO_SHOP;

		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		string outlet = GetOutletId();
		if (!LF_MannequinOutfit.HasOutfit(config, outlet, m_aItemIds))
			return LF_EShopRefusal.NOT_SOLD_HERE;

		int nowMs = GetGame().GetWorld().GetWorldTime();
		int mask = LF_MannequinOutfit.SoldOutMask(config, outlet, m_aItemIds, nowMs);
		if (LF_MannequinOutfit.OutfitSoldOut(config, outlet, m_aItemIds, mask))
		{
			RefreshSoldOut();
			LF_Debug.Info(LF_MannequinOutfit.FEATURE, string.Format("outfit refused at %1: a piece is sold out", outlet));
			return LF_EShopRefusal.OUT_OF_STOCK;
		}

		int total = LF_MannequinOutfit.OutfitTotal(config, outlet, m_aItemIds);
		if (LF_MoneyUtils.GetCash(buyer) < total)
		{
			LF_Debug.Info(LF_MannequinOutfit.FEATURE, string.Format("outfit refused at %1: %2 carries less than %3", outlet, LF_Utils.GetPlayerUid(buyer), total));
			return LF_EShopRefusal.NOT_ENOUGH_CASH;
		}

		LF_EShopRefusal result = LF_EShopRefusal.NONE;
		array<ref LF_MannequinPiece> pieces = {};
		LF_MannequinOutfit.Pieces(config, outlet, m_aItemIds, 0, pieces);
		foreach (LF_MannequinPiece piece : pieces)
		{
			int paid;
			int left;
			string missing;
			result = m_Shop.BuyDropOverflow(piece.m_Prefab, 1, buyer, paid, missing, left);
			if (result != LF_EShopRefusal.NONE)
				break;

			charged += paid;
			dropped += left;
			bought++;
		}

		LF_Debug.Info(LF_MannequinOutfit.FEATURE, string.Format("outfit at %1 for %2: %3 pieces of %4, charged=%5 dropped=%6 last=%7", outlet, LF_Utils.GetPlayerUid(buyer), bought, pieces.Count(), charged, dropped, typename.EnumToString(LF_EShopRefusal, result)));
		RefreshSoldOut();
		return result;
	}
}
