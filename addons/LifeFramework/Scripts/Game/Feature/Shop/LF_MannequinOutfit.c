//------------------------------------------------------------------------------------------------
//! One worn piece on a mannequin: the catalogue row it sells as, its price at the mannequin's outlet
//! and whether the outlet has run out of it.
class LF_MannequinPiece
{
	string m_sId;
	ResourceName m_Prefab;
	int m_iPrice;
	bool m_bSoldOut;
}

//------------------------------------------------------------------------------------------------
//! The rules of a dressed mannequin (LF_MannequinOutfitComponent), with no world: which worn piece a
//! slot sells, at what price, the whole outfit's total and which pieces are sold out. A piece is the
//! catalogue item the mannequin's outlet offers under that id; an id the outlet does not sell is no
//! piece (its slot is hidden). The whole outfit is every piece at the sum of their prices.
class LF_MannequinOutfit
{
	static const string FEATURE = "Mannequin";
	//! Slots one mannequin can carry: the sold-out flags travel as bits of one replicated int.
	static const int MAX_PIECES = 8; // lf-fixed: the sold-out flags are bits of one replicated int

	//------------------------------------------------------------------------------------------------
	//! The piece in this slot, or null when the slot is empty or the outlet does not sell that id.
	//! \param soldOutMask Bit i set: slot i is sold out (server state the mannequin replicates).
	static LF_MannequinPiece Piece(LF_ItemCatalogueConfig config, string outlet, array<string> ids, int slot, int soldOutMask)
	{
		if (!config || !ids || slot < 0 || slot >= ids.Count() || slot >= MAX_PIECES)
			return null;

		string id = ids[slot];
		LF_CatalogueItem item = LF_ItemCatalogue.Item(config, id);
		if (!item || item.m_iPrice <= 0)
			return null;

		LF_OutletOffer offer = config.FindOffer(outlet, id);
		if (!offer || offer.m_bRemoved || offer.IsLicenceRow())
			return null;

		LF_MannequinPiece piece = new LF_MannequinPiece();
		piece.m_sId = id;
		piece.m_Prefab = item.m_Prefab;
		piece.m_iPrice = item.m_iPrice;
		piece.m_bSoldOut = (soldOutMask & (1 << slot)) != 0;
		return piece;
	}

	//------------------------------------------------------------------------------------------------
	//! Every sellable piece, in slot order.
	static void Pieces(LF_ItemCatalogueConfig config, string outlet, array<string> ids, int soldOutMask, notnull array<ref LF_MannequinPiece> outPieces)
	{
		outPieces.Clear();
		if (!ids)
			return;

		for (int slot = 0; slot < ids.Count() && slot < MAX_PIECES; slot++)
		{
			LF_MannequinPiece piece = Piece(config, outlet, ids, slot, soldOutMask);
			if (piece)
				outPieces.Insert(piece);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The whole outfit's price: the sum of every sellable piece. 0 when there is no piece.
	static int OutfitTotal(LF_ItemCatalogueConfig config, string outlet, array<string> ids)
	{
		array<ref LF_MannequinPiece> pieces = {};
		Pieces(config, outlet, ids, 0, pieces);
		int total;
		foreach (LF_MannequinPiece piece : pieces)
		{
			total += piece.m_iPrice;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the whole outfit is offered: two pieces or more (one piece is just that piece).
	static bool HasOutfit(LF_ItemCatalogueConfig config, string outlet, array<string> ids)
	{
		array<ref LF_MannequinPiece> pieces = {};
		Pieces(config, outlet, ids, 0, pieces);
		return pieces.Count() >= 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any sellable piece is sold out: then the whole outfit is too.
	static bool OutfitSoldOut(LF_ItemCatalogueConfig config, string outlet, array<string> ids, int soldOutMask)
	{
		array<ref LF_MannequinPiece> pieces = {};
		Pieces(config, outlet, ids, soldOutMask, pieces);
		foreach (LF_MannequinPiece piece : pieces)
		{
			if (piece.m_bSoldOut)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the sold-out bits for these slots from the outlet's live stock (LF_CatalogueStock).
	static int SoldOutMask(LF_ItemCatalogueConfig config, string outlet, array<string> ids, int nowMs)
	{
		if (!config || !ids)
			return 0;

		int mask;
		for (int slot = 0; slot < ids.Count() && slot < MAX_PIECES; slot++)
		{
			LF_OutletOffer offer = config.FindOffer(outlet, ids[slot]);
			if (offer && !LF_CatalogueStock.CanSupply(offer, 1, nowMs))
				mask = mask | (1 << slot);
		}

		return mask;
	}

	//------------------------------------------------------------------------------------------------
	//! The action line for a piece: "Buy <name>: $<price>" (format key, item name, price).
	static string PieceLabel(string itemName, int price)
	{
		return WidgetManager.Translate("#LF-Mannequin_BuyPiece", WidgetManager.Translate(itemName), LF_MoneyFormat.FormatMoney(price));
	}

	//------------------------------------------------------------------------------------------------
	//! The action line for the whole outfit: "Buy whole outfit: $<total>".
	static string OutfitLabel(int total)
	{
		return WidgetManager.Translate("#LF-Mannequin_BuyOutfit", LF_MoneyFormat.FormatMoney(total));
	}
}
