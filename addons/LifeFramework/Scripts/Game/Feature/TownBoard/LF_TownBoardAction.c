// LF_TownBoardAction.c — "Read: <card title>" over one card of the town board. The prefab has one
// context per card slot, each with this action and its slot number. Local effect only: the cards are
// config, so reading one opens the detail sheet on the reader's client with no RPC. It also reports
// the board as read (sign.read:<board sign id>), which the arrival quest's "read the town board" step
// waits for.

class LF_TownBoardCardAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Card slot on the board (0 .. 7: left to right, top row first)")]
	protected int m_iSlot;

	protected LF_SignComponent m_Sign;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Sign = LF_Component<LF_SignComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	int GetSlot()
	{
		return m_iSlot;
	}

	//------------------------------------------------------------------------------------------------
	//! An empty slot (fewer cards than slots, or no pack text) offers nothing.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_TownBoard.CardAt(m_iSlot) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_TownBoardCard card = LF_TownBoard.CardAt(m_iSlot);
		if (!card)
			return false;

		outName = WidgetManager.Translate(LF_SignCatalog.KEY_READ, LF_SignCatalog.ResolveTitle(card.m_sTitle));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_TownBoard.FEATURE, string.Format("card slot %1 read with no board or no reader", m_iSlot));
			return;
		}

		// Local effect: the sheet opens on the reader's own client. Anywhere else (a server asked to
		// perform it) there is nothing to show.
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			LF_Debug.Warn(LF_TownBoard.FEATURE, string.Format("card slot %1 read off the reader's client; no sheet here", m_iSlot));
			return;
		}

		LF_TownBoardCard card = LF_TownBoard.CardAt(m_iSlot);
		if (!card)
		{
			LF_Debug.Error(LF_TownBoard.FEATURE, string.Format("card slot %1 read with no card; sheet not opened", m_iSlot));
			return;
		}

		LF_TownBoardSheet.Open(card, pOwnerEntity);
		ReportRead(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! The board counts as read for the quests: the server checks the board stands beside the reader.
	protected void ReportRead(IEntity user)
	{
		if (!m_Sign || m_Sign.GetSignId().IsEmpty())
			return;

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(user.FindComponent(LF_QuestTrackerComponent));
		if (!tracker)
		{
			LF_Debug.Error(LF_TownBoard.FEATURE, "board read not reported: the reader has no quest tracker");
			return;
		}

		tracker.AskSignRead(m_Sign.GetSignId());
	}
}
