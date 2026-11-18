// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_HandsetCall` (RedProof() skips the carried call): RED PROOF OK, `the fee came from carried cash once (expected 2, got 7)` and `the taxi board got the call` failed. Green with the call: `--only LF_Test_HandsetCall` passed.
//------------------------------------------------------------------------------------------------
//! comms/handset-call (#3155): a character carrying a radio set calls the taxi from where they
//! stand. The call reaches the bus (and the service board) at the caller's position, the fee
//! comes from carried cash once, a short purse refuses and posts nothing, and a radio that is not
//! carried (lying on the ground) refuses.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HandsetCall : LF_Test
{
	//! A vanilla handheld radio: the pack's radio set inherits one (core tests name no pack file).
	static const ResourceName RADIO = "{E1A5D4B878AA8980}Prefabs/Items/Equipment/Radios/Radio_R148.et";

	override string GetName()
	{
		return "comms/handset-call";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_ServiceBoard.ResetStatics();
		LF_ServiceBoardSubscriber.ResetStatics();
		LF_ServiceBoardSubscriber.Install();
		LF_HandsetCall.ResetStatics();
		LF_HandsetCallTestCapture capture = new LF_HandsetCallTestCapture();
		LF_EventBus.Subscribe(LF_OnServiceCall, capture);

		IEntity caller = LF_TestCharacterFixture.Spawn(ctx);
		if (!caller)
			return;

		IEntity radio = LF_Utils.SpawnEntityPrefab(RADIO, caller.GetOrigin() + "2 0 0");
		ctx.NotNull(radio, "the radio prefab spawns");
		if (!radio)
		{
			LF_TestCharacterFixture.Delete(caller);
			return;
		}

		array<ref LF_ServiceCandidate> driver = {};
		driver.Insert(LF_ServiceCandidate.Make(7, false, LF_EJobType.TAXI));
		int fee = LF_HandsetCall.DEFAULT_FEE;
		LF_MoneyUtils.AddCash(caller, fee + 2);
		int cash = LF_MoneyUtils.GetCash(caller);

		// Not carried: the radio lies on the ground beside the caller.
		ctx.True(LF_HandsetCall.CallAt(100, "handset-a", caller, radio, "taxi", fee, driver) == LF_EHandsetResult.NO_HANDSET, "a radio on the ground is refused");
		ctx.Equal(0, capture.m_iCalls, "a refused call posts nothing");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(caller), "a refused call takes no fee");

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(caller);
		bool carried = storage && storage.TryInsertItem(radio);
		ctx.True(carried, "the radio goes into the caller's inventory");
		if (!carried)
		{
			LF_TestCharacterFixture.Delete(radio);
			LF_TestCharacterFixture.Delete(caller);
			return;
		}

		ctx.True(LF_HandsetCall.IsCarriedBy(radio, caller), "the caller carries the radio");
		vector at = caller.GetOrigin();
		if (!RedProof())
			ctx.True(LF_HandsetCall.CallAt(100, "handset-a", caller, radio, "taxi", fee, driver) == LF_EHandsetResult.CONNECTED, "the carried radio calls the taxi");
		ctx.Equal(1, capture.m_iCalls, "the call went out once");
		ctx.EqualStr("taxi", capture.m_sService, "the call is for the taxi");
		ctx.True(vector.Distance(capture.m_vPosition, at) < 0.5, "the call is at the caller's position");
		ctx.Equal(cash - fee, LF_MoneyUtils.GetCash(caller), "the fee came from carried cash once");
		array<string> board = {};
		LF_ServiceBoard.BuildEntries("taxi", System.GetTickCount(), 10, board);
		ctx.Equal(1, board.Count(), "the taxi board got the call");

		// Short purse: 2 left, the fee is 5.
		ctx.True(LF_HandsetCall.CallAt(1000, "handset-a", caller, radio, "taxi", fee, driver) == LF_EHandsetResult.NO_CASH, "a short purse is refused");
		ctx.Equal(1, capture.m_iCalls, "a short purse posts nothing");
		ctx.Equal(cash - fee, LF_MoneyUtils.GetCash(caller), "a short purse keeps its cash");

		// Nobody on duty: no fee, nothing posted.
		LF_MoneyUtils.AddCash(caller, fee);
		array<ref LF_ServiceCandidate> nobody = {};
		ctx.True(LF_HandsetCall.CallAt(2000, "handset-a", caller, radio, "medic", fee, nobody) == LF_EHandsetResult.NO_ANSWER, "nobody on duty gives no answer");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(caller), "an unanswered call takes no fee");
		ctx.Equal(1, capture.m_iCalls, "an unanswered call posts nothing");

		LF_EventBus.Unsubscribe(LF_OnServiceCall, capture);
		LF_HandsetCall.ResetStatics();
		LF_ServiceBoard.ResetStatics();
		LF_TestCharacterFixture.Delete(caller);
	}
}

//------------------------------------------------------------------------------------------------
//! Counts LF_OnServiceCall and keeps the last one's service and position.
class LF_HandsetCallTestCapture : LF_EventSubscriber
{
	int m_iCalls;
	string m_sService;
	vector m_vPosition;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (!call)
			return false;

		m_iCalls++;
		m_sService = call.m_sService;
		m_vPosition = call.m_vPosition;
		return true;
	}
}
