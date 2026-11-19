// LF_HarvestBoard.c: the farm contract board and its four prompts (#3656, the board half of #3297).
// The board is a cork notice board (Prefabs/Harvest/LF_HarvestBoard.et) that stands at a wheat field's gate. Its
// prompts: Post contract, Take with own reaper, Take with rental kit, Return kit. The contract rules live in
// LF_HarvestContract; this file finds the board's field, checks the user stands at the board and toasts the outcome.
//
// The server decides everything except one fact it cannot know: whether the taker's client owns the Rally DLC for the
// Z7211 tractor. DlcManager answers for the machine it runs on, so the rental prompt runs on the acting client
// (HasLocalEffectOnlyScript), asks LF_VehicleDlcGate.LocalOwns there and sends the answer with one RPC
// (SCR_PlayerController.LF_AskHarvestRental). The answer is a hint, like the vehicle lot's: a client that lies gets a
// tractor whose driver seat vanilla refuses it, at the price of its own $300 deposit. The other three prompts run
// on the server and send nothing.

enum LF_EHarvestBoardMode
{
	POST,
	TAKE_OWN,
	TAKE_RENTAL,
	RETURN_KIT
}

//! A sphere query that keeps the wheat fields it meets.
class LF_HarvestBoardFieldQuery
{
	ref array<LF_WheatField> m_aFields = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		LF_WheatField field = LF_WheatField.Cast(entity);
		if (field)
			m_aFields.Insert(field);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HarvestBoard
{
	static const string FEATURE = "Harvest";

	//! How far the user may stand from the board and still use it.
	static const float USE_REACH_M = 6; // lf-fixed: the board prompts' reach (#3656)

	//! How far from the board its field's centre may lie: a board belongs to the nearest field inside this.
	static const float FIELD_SEARCH_M = 90; // lf-fixed: a 60 x 40 m field centred a few dozen metres from its gate board (#3656)

	//! How far from the board the rental kit stands, on the side away from the field.
	static const float KIT_SPOT_M = 5; // lf-fixed: inside the 8 m return reach, clear of the board (#3656)

	//------------------------------------------------------------------------------------------------
	//! The nearest wheat field to the board inside FIELD_SEARCH_M, or null.
	static LF_WheatField FieldOf(IEntity board)
	{
		if (!board)
			return null;

		LF_HarvestBoardFieldQuery query = new LF_HarvestBoardFieldQuery();
		vector at = board.GetOrigin();
		GetGame().GetWorld().QueryEntitiesBySphere(at, FIELD_SEARCH_M, query.Collect, null, EQueryEntitiesFlags.ALL);
		LF_WheatField best;
		float bestDistance = FIELD_SEARCH_M * 2;
		foreach (LF_WheatField candidate : query.m_aFields)
		{
			float distance = vector.Distance(candidate.GetOrigin(), at);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = candidate;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the rental kit stands: KIT_SPOT_M from the board along its depth axis, on the side away from the field,
	//! on the ground, turned the way the board is. `angles` is the kit's yaw/pitch/roll.
	static vector KitSpot(IEntity board, LF_WheatField field, out vector angles)
	{
		vector origin = board.GetOrigin();
		vector depth = board.GetTransformAxis(2);
		depth[1] = 0;
		depth.Normalize();
		vector away = field.GetOrigin() - origin;
		away[1] = 0;
		float side = 1;
		if (vector.Dot(depth, away) > 0)
			side = -1;

		vector spot = origin + depth * (KIT_SPOT_M * side);
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + 0.3;
		vector yawPitchRoll = board.GetYawPitchRoll();
		angles = Vector(yawPitchRoll[0], 0, 0);
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one prompt of the board. Checks the user stands at the board, finds the board's field, runs the contract
	//! call, toasts the outcome. `dlcOwned` is the taker's client answer for the Rally tractor (rental only).
	static LF_EHarvestContract Use(IEntity board, IEntity user, LF_EHarvestBoardMode mode, bool dlcOwned)
	{
		if (!Replication.IsServer())
			return LF_EHarvestContract.NOT_SERVER;

		if (!board || !user)
			return LF_EHarvestContract.NO_USER;

		if (vector.Distance(board.GetOrigin(), user.GetOrigin()) > USE_REACH_M)
		{
			LF_Debug.Warn(FEATURE, "harvest board refused: the player is not at the board");
			return LF_EHarvestContract.KIT_FAR;
		}

		LF_WheatField field = FieldOf(board);
		if (!field)
		{
			LF_Debug.Warn(FEATURE, string.Format("harvest board refused: no wheat field within %1 m of the board", FIELD_SEARCH_M));
			LF_Utils.NotifyEntity(user, "#LF-Harvest_BoardNoField", TitleOf(mode), 4.0);
			return LF_EHarvestContract.NO_FIELD;
		}

		LF_EHarvestContract result;
		int refunded;
		IEntity kit;
		vector kitAngles;
		vector kitAt;
		switch (mode)
		{
			case LF_EHarvestBoardMode.POST:
				result = LF_HarvestContract.Post(field);
				break;
			case LF_EHarvestBoardMode.TAKE_OWN:
				result = LF_HarvestContract.Take(field, user, false, dlcOwned, board.GetOrigin(), vector.Zero, kit);
				break;
			case LF_EHarvestBoardMode.TAKE_RENTAL:
				kitAt = KitSpot(board, field, kitAngles);
				result = LF_HarvestContract.Take(field, user, true, dlcOwned, kitAt, kitAngles, kit);
				break;
			case LF_EHarvestBoardMode.RETURN_KIT:
				result = LF_HarvestContract.ReturnKit(field, user, board.GetOrigin(), refunded);
				break;
		}

		string key;
		if (result == LF_EHarvestContract.OK)
			key = SuccessKey(mode);
		else
			key = RefusalKey(result);

		// A refunded return already toasted its own line from the contract.
		if (!key.IsEmpty() && !(mode == LF_EHarvestBoardMode.RETURN_KIT && result == LF_EHarvestContract.OK))
			LF_Utils.NotifyEntity(user, key, TitleOf(mode), 4.0);

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the entity carries a board prompt: the RPC takes an id from a client, so it must name a board.
	static bool IsBoard(IEntity entity)
	{
		if (!entity)
			return false;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (LF_HarvestBoardAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static string TitleOf(LF_EHarvestBoardMode mode)
	{
		switch (mode)
		{
			case LF_EHarvestBoardMode.POST: return "#LF-Harvest_BoardPost";
			case LF_EHarvestBoardMode.TAKE_OWN: return "#LF-Harvest_BoardTakeOwn";
			case LF_EHarvestBoardMode.TAKE_RENTAL: return "#LF-Harvest_BoardTakeRental";
		}

		return "#LF-Harvest_BoardReturn";
	}

	//------------------------------------------------------------------------------------------------
	static string SuccessKey(LF_EHarvestBoardMode mode)
	{
		switch (mode)
		{
			case LF_EHarvestBoardMode.POST: return "#LF-Harvest_BoardPosted";
			case LF_EHarvestBoardMode.TAKE_OWN: return "#LF-Harvest_BoardTakenOwn";
			case LF_EHarvestBoardMode.TAKE_RENTAL: return "#LF-Harvest_BoardTakenRental";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The toast line for a refusal: the contract's own lines where it has one, the board's for the rest.
	static string RefusalKey(LF_EHarvestContract why)
	{
		string key = LF_HarvestContract.RefusalKey(why);
		if (!key.IsEmpty())
			return key;

		switch (why)
		{
			case LF_EHarvestContract.NOT_POSTED: return "#LF-Harvest_BoardNotPosted";
			case LF_EHarvestContract.NOT_RIPE: return "#LF-Harvest_BoardNotRipe";
			case LF_EHarvestContract.ALREADY_POSTED: return "#LF-Harvest_BoardAlreadyPosted";
			case LF_EHarvestContract.NOT_TAKER: return "#LF-Harvest_BoardNotTaker";
			case LF_EHarvestContract.NO_KIT: return "#LF-Harvest_BoardNoKit";
			case LF_EHarvestContract.KIT_FAILED: return "#LF-Harvest_BoardKitFailed";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! One prompt on the farm board. Post, Take own and Return run on the server; Take rental runs on the acting client,
//! which adds its Rally DLC answer and asks the server through the player controller.
class LF_HarvestBoardAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this prompt does at the board", "", ParamEnumArray.FromEnum(LF_EHarvestBoardMode))]
	protected LF_EHarvestBoardMode m_eMode;

	//------------------------------------------------------------------------------------------------
	//! The rental prompt needs the client's DLC answer, so it runs on the acting player's machine only.
	override bool HasLocalEffectOnlyScript()
	{
		return m_eMode == LF_EHarvestBoardMode.TAKE_RENTAL;
	}

	//------------------------------------------------------------------------------------------------
	//! The contract state lives on the server, so every prompt shows; the server answers with a toast.
	override bool CanBeShownScript(IEntity user)
	{
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = LF_HarvestBoard.TitleOf(m_eMode);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;

		if (m_eMode == LF_EHarvestBoardMode.TAKE_RENTAL)
		{
			PerformRental(pOwnerEntity, pUserEntity);
			return;
		}

		if (!Replication.IsServer())
			return;

		if (!LF_RateLimit.AllowCharacter(pUserEntity, LF_ERequestKind.PURCHASE, "harvest board"))
			return;

		LF_HarvestBoard.Use(pOwnerEntity, pUserEntity, m_eMode, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: reads the Rally licence of this machine and asks the server for the rental.
	protected void PerformRental(IEntity board, IEntity user)
	{
		if (!LF_CounterAccess.IsLocalActor(user, LF_Utils.GetLocalCharacter()))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_HarvestBoard.FEATURE, "rental dropped: no local player controller");
			return;
		}

		bool dlcOwned = LF_VehicleDlcGate.LocalOwns(LF_VehicleDlcGate.Z7211);
		controller.LF_AskHarvestRental(board, dlcOwned);
	}
}

//------------------------------------------------------------------------------------------------
//! The rental request: client -> server, with the client's DLC answer. The server re-checks every other rule.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server for a rental kit at `board`. The listen host handles it directly.
	void LF_AskHarvestRental(IEntity board, bool dlcOwned)
	{
		RplComponent rpl = LF_Component<RplComponent>.Find(board);
		if (!rpl)
		{
			LF_Debug.Error(LF_HarvestBoard.FEATURE, "rental dropped: the board has no RplComponent");
			return;
		}

		if (Replication.IsServer())
			RpcAsk_LF_HarvestRental(rpl.Id(), dlcOwned);
		else
			Rpc(RpcAsk_LF_HarvestRental, rpl.Id(), dlcOwned);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarvestRental(RplId boardId, bool dlcOwned)
	{
		RplComponent boardRpl = RplComponent.Cast(Replication.FindItem(boardId));
		IEntity board;
		if (boardRpl)
			board = boardRpl.GetEntity();

		IEntity user = GetControlledEntity();
		if (!board || !user)
		{
			LF_Debug.Warn(LF_HarvestBoard.FEATURE, string.Format("rental refused for player %1: no board or no character", GetPlayerId()));
			return;
		}

		if (!LF_HarvestBoard.IsBoard(board))
		{
			LF_Debug.Warn(LF_HarvestBoard.FEATURE, string.Format("rental refused for player %1: the entity is not a harvest board", GetPlayerId()));
			return;
		}

		if (!LF_RateLimit.AllowCharacter(user, LF_ERequestKind.PURCHASE, "harvest rental"))
			return;

		LF_HarvestBoard.Use(board, user, LF_EHarvestBoardMode.TAKE_RENTAL, dlcOwned);
	}
}
