//------------------------------------------------------------------------------------------------
//! One line of a dressed mannequin's action list: "Buy <piece>: $<price>" for a worn piece, or "Buy
//! whole outfit: $<total>" (slot -1). A slot the mannequin has no piece for is hidden; a sold-out
//! piece is greyed with the reason. The client only shows; the server decides and sells through
//! LF_MannequinOutfitComponent (the outlet's till), then answers with a toast. No window opens.
class LF_MannequinBuyAction : ScriptedUserAction
{
	//! Slot for "Buy whole outfit".
	static const int OUTFIT_SLOT = -1;

	[Attribute("0", UIWidgets.EditBox, "Worn piece this line sells: the index into the mannequin's item ids; -1 is the whole outfit")]
	protected int m_iSlot;

	protected LF_MannequinOutfitComponent m_Mannequin;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		m_Mannequin = LF_Component<LF_MannequinOutfitComponent>.Find(pOwnerEntity);
		if (!m_Mannequin)
			LF_Debug.Error(LF_MannequinOutfit.FEATURE, "buy action on an entity with no LF_MannequinOutfitComponent; action hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Mannequin)
			return false;

		if (m_iSlot == OUTFIT_SLOT)
			return m_Mannequin.HasOutfit();

		return m_Mannequin.GetPiece(m_iSlot) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Mannequin)
			return false;

		int price;
		if (m_iSlot == OUTFIT_SLOT)
		{
			if (m_Mannequin.IsOutfitSoldOut())
			{
				SetCannotPerformReason("#LF-Mannequin_OutfitSoldOut");
				return false;
			}

			price = m_Mannequin.GetOutfitTotal();
		}
		else
		{
			LF_MannequinPiece piece = m_Mannequin.GetPiece(m_iSlot);
			if (!piece)
				return false;

			if (piece.m_bSoldOut)
			{
				SetCannotPerformReason("#LF-Mannequin_SoldOut");
				return false;
			}

			price = piece.m_iPrice;
		}

		// A hint only: the server checks the cash again when it sells.
		if (user && LF_MoneyUtils.GetCash(user) < price)
		{
			SetCannotPerformReason("#LF-Mannequin_NoCash");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Mannequin)
			return false;

		if (m_iSlot == OUTFIT_SLOT)
		{
			outName = LF_MannequinOutfit.OutfitLabel(m_Mannequin.GetOutfitTotal());
			return true;
		}

		LF_MannequinPiece piece = m_Mannequin.GetPiece(m_iSlot);
		if (!piece)
			return false;

		outName = LF_MannequinOutfit.PieceLabel(m_Mannequin.GetPieceName(piece), piece.m_iPrice);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells the piece or the outfit and tells the buyer how it went.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !m_Mannequin || !pUserEntity)
			return;

		int charged;
		int dropped;
		int bought;
		LF_EShopRefusal result;
		string message;
		if (m_iSlot == OUTFIT_SLOT)
		{
			result = m_Mannequin.BuyOutfit(pUserEntity, charged, bought, dropped);
			message = OutfitMessage(result, bought, dropped);
		}
		else
		{
			result = m_Mannequin.BuyPiece(m_iSlot, pUserEntity, charged, dropped);
			message = PieceMessage(result, dropped);
		}

		LF_ENotificationType type = LF_ENotificationType.INFO;
		if (result != LF_EShopRefusal.NONE)
			type = LF_ENotificationType.WARNING;

		int buyerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (buyerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(buyerId, "#LF-Mannequin_Title", message, 6.0, type, LF_MoneyFormat.FormatMoney(charged));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The toast line for one piece.
	static string PieceMessage(LF_EShopRefusal result, int dropped)
	{
		if (result != LF_EShopRefusal.NONE)
			return LF_ShopRefusal.Key(result);

		if (dropped > 0)
			return "#LF-Mannequin_BoughtDropped";

		return "#LF-Mannequin_Bought";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The toast line for the whole outfit: refused outright, part bought, or all of it.
	static string OutfitMessage(LF_EShopRefusal result, int bought, int dropped)
	{
		if (result != LF_EShopRefusal.NONE)
		{
			if (bought > 0)
				return "#LF-Mannequin_BoughtPart";

			return LF_ShopRefusal.Key(result);
		}

		if (dropped > 0)
			return "#LF-Mannequin_BoughtOutfitDropped";

		return "#LF-Mannequin_BoughtOutfit";
	}
}
