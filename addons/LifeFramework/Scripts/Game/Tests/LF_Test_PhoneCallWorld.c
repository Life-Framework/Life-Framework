// red-proof: Observed 2026-11-12 with `tools/cli test --no-build --only LF_Test_PhoneCallWorld`: letting anyone answer (`mayAnswer = true`) in LF_PhoneCalls.AnswerAt failed it with `- an off-duty player cannot pick up the station phone`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/phone-call-world (#147): the shipped payphone and station telephone prefabs, end to end
//! on the server. Dialling takes the coin once and still reaches the radio dispatch (the existing
//! payphone path); the station phone's line spawns and shows RINGING; only the on-duty officer
//! picks up; both lines show who talks; a talker who walks away ends it; the bus hears every
//! transition; nobody on duty takes no coin and rings nothing.
// tier: WORLD
class LF_Test_PhoneCallWorld : LF_Test
{
	static const ResourceName PAYPHONE = "{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et";
	static const ResourceName STATION = "{9BDAE42023AB3357}Prefabs/Comms/LF_DispatchPhone.et";

	override string GetName()
	{
		return "comms/phone-call-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_Payphone.ResetStatics();
		LF_PhoneCalls.ResetStatics();
		LF_ServiceCallDispatch.ResetStatics();
		LF_ServiceCallDispatch.Install();
		LF_PhoneTestEventCounter counter = new LF_PhoneTestEventCounter();
		LF_EventBus.Subscribe(LF_OnPhoneCall, counter);
		LF_PhoneTestPresence presence = new LF_PhoneTestPresence();
		LF_PhoneCalls.SetPresence(presence);

		IEntity caller = LF_TestCharacterFixture.Spawn(ctx);
		if (!caller)
			return;

		vector at = caller.GetOrigin();
		IEntity payphoneEntity = LF_Utils.SpawnEntityPrefab(PAYPHONE, at + "3 0 0");
		IEntity stationEntity = LF_Utils.SpawnEntityPrefab(STATION, at + "9 0 0");
		LF_PhoneComponent payphone = LF_PhoneComponent.Of(payphoneEntity);
		LF_PhoneComponent station = LF_PhoneComponent.Of(stationEntity);
		ctx.NotNull(payphone, "the payphone prefab is a telephone");
		ctx.NotNull(station, "the station telephone prefab is a telephone");
		if (!payphone || !station)
		{
			Cleanup(caller, payphoneEntity, stationEntity);
			return;
		}

		ctx.True(!payphone.IsDispatchPhone(), "a payphone answers no service");
		ctx.EqualStr("police", station.DispatchService(), "the station telephone answers the police");

		int cost = LF_ServiceCallsConfig.PayphoneCost();
		LF_MoneyUtils.AddCash(caller, 5);
		int cash = LF_MoneyUtils.GetCash(caller);
		array<ref LF_ServiceCandidate> duty = {};
		duty.Insert(LF_ServiceCandidate.Make(2, true, LF_EJobType.UNEMPLOYED));
		duty.Insert(LF_ServiceCandidate.Make(3, false, LF_EJobType.UNEMPLOYED));
		array<string> board = {};

		// Dial: one coin, one dispatch, the station rings, the payphone is busy with the caller.
		ctx.True(LF_PhoneCalls.DialAt(100, 1, "phone-caller", caller, payphone, "police", duty) == LF_EPhoneRefusal.OK, "the dial rings the station");
		ctx.Equal(cash - cost, LF_MoneyUtils.GetCash(caller), "the dial took the coin once");
		LF_Dispatch.BuildBoardEntries(10, board);
		ctx.Equal(1, board.Count(), "the call still reached the radio dispatch once");
		LF_PhoneLineComponent stationLine = station.Line();
		LF_PhoneLineComponent payLine = payphone.Line();
		ctx.NotNull(stationLine, "the station phone got its line");
		ctx.NotNull(payLine, "the payphone got its line");
		if (!stationLine || !payLine)
		{
			Cleanup(caller, payphoneEntity, stationEntity);
			return;
		}

		ctx.True(stationLine.GetState() == LF_EPhoneLineState.RINGING, "the station line shows RINGING");
		ctx.True(payLine.GetState() == LF_EPhoneLineState.BUSY && payLine.GetTalkerId() == 1, "the payphone line shows the caller on it");
		ctx.Equal(1, counter.Count(LF_EPhoneCallState.RINGING), "RINGING went out on the bus");

		// Pick-up: the off-duty player is refused, the officer connects.
		ctx.True(LF_PhoneCalls.AnswerAt(105, 3, "phone-civilian", caller, station, duty) == LF_EPhoneRefusal.NOT_ON_DUTY, "an off-duty player cannot pick up the station phone");
		ctx.True(LF_PhoneCalls.AnswerAt(106, 2, "phone-officer", caller, station, duty) == LF_EPhoneRefusal.OK, "the on-duty officer picks up");
		ctx.True(stationLine.GetState() == LF_EPhoneLineState.BUSY && stationLine.GetTalkerId() == 2, "the station line shows the officer on it");
		ctx.Equal(1, counter.Count(LF_EPhoneCallState.CONNECTED), "CONNECTED went out on the bus");

		// The cord: the officer walks away, the call ends, both lines go idle.
		presence.m_eVerdict = LF_EPhoneEnd.WALKED_AWAY;
		LF_PhoneCalls.TickAt(110);
		presence.m_eVerdict = LF_EPhoneEnd.NONE;
		ctx.True(stationLine.GetState() == LF_EPhoneLineState.IDLE && payLine.GetState() == LF_EPhoneLineState.IDLE, "both lines are idle after the call");
		ctx.Equal(1, counter.Count(LF_EPhoneCallState.ENDED), "ENDED went out on the bus");
		ctx.True(counter.m_eLastEnd == LF_EPhoneEnd.WALKED_AWAY, "the end reason is WALKED_AWAY");

		// Nobody on duty: no coin, nothing rings, MISSED on the bus.
		array<ref LF_ServiceCandidate> nobody = {};
		cash = LF_MoneyUtils.GetCash(caller);
		ctx.True(LF_PhoneCalls.DialAt(1000, 1, "phone-caller-2", caller, payphone, "police", nobody) == LF_EPhoneRefusal.NOBODY_ON_DUTY, "nobody on duty: the call is missed");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(caller), "a call nobody can take takes no coin");
		ctx.True(stationLine.GetState() == LF_EPhoneLineState.IDLE, "nothing rings when nobody is on duty");
		ctx.Equal(1, counter.Count(LF_EPhoneCallState.MISSED), "MISSED went out on the bus");

		Cleanup(caller, payphoneEntity, stationEntity);
		LF_EventBus.Unsubscribe(LF_OnPhoneCall, counter);
		LF_PhoneCalls.ResetStatics();
	}

	protected void Cleanup(IEntity caller, IEntity payphone, IEntity station)
	{
		array<IEntity> phones = {payphone, station};
		foreach (IEntity phone : phones)
		{
			LF_PhoneComponent component = LF_PhoneComponent.Of(phone);
			if (component && component.ServerLineEntity())
				SCR_EntityHelper.DeleteEntityAndChildren(component.ServerLineEntity());
			if (phone)
				SCR_EntityHelper.DeleteEntityAndChildren(phone);
		}

		LF_TestCharacterFixture.Delete(caller);
	}
}

//------------------------------------------------------------------------------------------------
//! Every test talker is at their phone unless the test says otherwise.
class LF_PhoneTestPresence : LF_PhonePresence
{
	LF_EPhoneEnd m_eVerdict = LF_EPhoneEnd.NONE;

	override bool IsAt(IEntity user, IEntity phone, float cordMeters)
	{
		return true;
	}

	override LF_EPhoneEnd Check(int playerId, string phoneKey, float cordMeters)
	{
		return m_eVerdict;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PhoneTestEventCounter : LF_EventSubscriber
{
	ref array<int> m_aStates = {};
	LF_EPhoneEnd m_eLastEnd;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPhoneCall call = LF_OnPhoneCall.Cast(busEvent);
		if (!call)
			return false;

		m_aStates.Insert(call.m_eState);
		m_eLastEnd = call.m_eEnd;
		return true;
	}

	int Count(LF_EPhoneCallState state)
	{
		int n;
		foreach (int s : m_aStates)
		{
			if (s == state)
				n++;
		}
		return n;
	}
}
