// LF_TownBoardMapKnown.c: reading a town board card marks its place "heard of" on the reader's map
// (#2242, card M14; docs/architecture/everon-map.md sections 4 and 6). The card is config and the sheet is
// local, so the read itself sends nothing; this file adds one small ask beside it. The server checks that
// the reader stands at a board and that the card's place is a map POI, then records it through
// LF_MapKnown.Hear (the M13 store, the join sync and the live message to the player's map).

//------------------------------------------------------------------------------------------------
//! What a card read means for the map, and the server's checks on the ask. The rules are pure.
class LF_TownBoardMapKnown
{
	//! The server accepts a read when a board stands this near the reader's character (the action's own
	//! reach is a few metres; this carries network slack).
	static const float BOARD_REACH = 12; // lf-fixed: anti-cheat slack on the card action's reach, not a gameplay knob
	//! Least time between two asks from one player.
	static const int MIN_GAP_MS = 500; // lf-fixed: anti-flood floor; a card is read once and the sheet takes longer

	//! Player id -> tick of the last accepted ask.
	protected static ref map<int, int> s_mLastMs;
	protected static bool s_bBoardFound;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLastMs = null;
		s_bBoardFound = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The map place a card points at, when that place is a map POI kind. The 1989 cards point at sign
	//! ids by default (kind Sign, not a POI), so those mark nothing; a card of a POI kind does.
	//! \param[out] placeId The LF_Places id to mark.
	//! \return True when reading the card can mark a place.
	static bool PoiPlaceOf(notnull LF_MapPoiConfig config, LF_TownBoardCard card, out string placeId)
	{
		if (!card || card.m_sPlaceId.IsEmpty())
			return false;

		string kind = card.m_sPlaceKind;
		if (kind.IsEmpty())
			kind = LF_SignComponent.PLACE_KIND;

		if (!config.Find(kind))
			return false;

		placeId = card.m_sPlaceId;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server store only: reading the card in `slot` makes the character hear of its place. Pure apart from
	//! the store, so a LOGIC test drives it with a fixture config.
	//! \return The M13 outcome: HEARD or KEPT when the card marks a place, REFUSED_BAD when it marks none.
	static LF_EMapKnownOutcome HearCard(notnull LF_MapPoiConfig config, string characterId, int slot)
	{
		string placeId;
		if (!PoiPlaceOf(config, LF_TownBoard.CardAt(slot), placeId))
			return LF_EMapKnownOutcome.REFUSED_BAD;

		return LF_MapKnown.HearCharacter(characterId, placeId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: counts one ask against the player. False inside the gap since the last accepted one.
	static bool Admit(int playerId, int nowMs)
	{
		if (!s_mLastMs)
			s_mLastMs = new map<int, int>();

		int last;
		if (s_mLastMs.Find(playerId, last) && nowMs >= last && nowMs - last < MIN_GAP_MS)
			return false;

		s_mLastMs.Set(playerId, nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player left the server. Player ids are reused by the next arrival, so the gap
	//! must not outlive the connection, and the table must not grow with every player who passed.
	static void Forget(int playerId)
	{
		if (s_mLastMs)
			s_mLastMs.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! How many players the gap holds a tick for.
	static int TrackedCount()
	{
		if (!s_mLastMs)
			return 0;

		return s_mLastMs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether a town board stands within BOARD_REACH of the character.
	static bool BoardNear(IEntity character)
	{
		if (!character || !GetGame() || !GetGame().GetWorld())
			return false;

		s_bBoardFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(character.GetOrigin(), BOARD_REACH, CollectBoard, null, EQueryEntitiesFlags.ALL);
		return s_bBoardFound;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectBoard(IEntity entity)
	{
		if (entity && entity.FindComponent(LF_TownBoardComponent))
		{
			s_bBoardFound = true;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: tell the server the reader read the card in `slot`. Nothing is sent for a card that marks no
	//! map place, so the 1989 sign cards cost no traffic.
	static void Report(int slot)
	{
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		string placeId;
		if (!config || !PoiPlaceOf(config, LF_TownBoard.CardAt(slot), placeId))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskTownCardHeard(slot);
	}
}

//------------------------------------------------------------------------------------------------
//! Reading a card also tells the map. The sheet and the quest signal stay as they were.
modded class LF_TownBoardCardAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		super.PerformAction(pOwnerEntity, pUserEntity);
		if (pOwnerEntity && pUserEntity && pUserEntity == LF_Utils.GetLocalCharacter())
			LF_TownBoardMapKnown.Report(m_iSlot);
	}
}

//------------------------------------------------------------------------------------------------
//! The one ask on the player's own controller (the client owns it).
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: the reader read the board card in `slot`.
	void LF_AskTownCardHeard(int slot)
	{
		if (Replication.IsServer())
			RpcAsk_LF_TownCardHeard(slot);
		else
			Rpc(RpcAsk_LF_TownCardHeard, slot);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: checked here. A board must stand beside the character, the slot must hold a card that points
	//! at a map POI, and a player may ask only so often.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_TownCardHeard(int slot)
	{
		int playerId = GetPlayerId();
		if (!LF_TownBoardMapKnown.Admit(playerId, System.GetTickCount()))
			return;

		IEntity character = GetControlledEntity();
		if (!character || !LF_TownBoardMapKnown.BoardNear(character))
		{
			LF_Debug.Warn(LF_MapKnown.FEATURE, string.Format("card read refused: player %1 is not at a town board", playerId));
			return;
		}

		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		string placeId;
		if (!config || !LF_TownBoardMapKnown.PoiPlaceOf(config, LF_TownBoard.CardAt(slot), placeId))
		{
			LF_Debug.Warn(LF_MapKnown.FEATURE, string.Format("card read refused: slot %1 marks no map place", slot));
			return;
		}

		LF_MapKnown.Hear(playerId, placeId);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the ask gate (registered at script load; Core/LF_StaticReset.c).
class LF_TownBoardMapKnown_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_TownBoardMapKnown_StaticResetHook());

	override string Owner()
	{
		return "LF_TownBoardMapKnown";
	}

	override void Apply()
	{
		LF_TownBoardMapKnown.ResetStatics();
	}
}
