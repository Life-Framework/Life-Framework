// red-proof: RED-OK 2026-11-23 `test --no-build --red LF_Test_OutpostOfficerVoice` together with the pack leg (everon/outpost-officer): RedProof() leaves the OutpostOfficer condition off the reply and withholds the stand-in conversation, so a civilian and an off-duty officer are offered the officer reply and the refused officer hears nothing ("the refusal speaks once (expected 1, got 0)"); green without the hook.
//! outposts/officer-voice (#4640, follow-up of #4459): the outpost people's officer reply is offered to an on-duty
//! officer only (the `OutpostOfficer` dialogue condition, the same on-duty rule the traders use), and a trader's
//! refusal of an on-duty officer (the real LF_OutpostBuyer.Sell path) speaks the voiced `for_key` refusal line to
//! that officer only. A civilian and an off-duty officer are never shown the reply and are never refused. The
//! conversation is a hand-built stand-in so the core leg needs no pack recording; the pack's own confs are checked in
//! the pack leg (everon/outpost-officer).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OutpostOfficerVoice : LF_Test
{
	protected static const string OFFICER_UID = "outpost-officer-a";
	protected static const string CIVILIAN_UID = "outpost-officer-b";
	protected static const string OUTPOST_ID = "officer-voice-test";
	protected static const string LINE_ID = "refuse";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "outposts/officer-voice";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_DialogueOption Reply(string next, string condition)
	{
		LF_DialogueOption option = new LF_DialogueOption();
		option.m_sTextKey = "#LF-Dialogue_OutpostQuartermaster_SayBadge";
		option.m_sNext = next;
		option.m_sCondition = condition;
		return option;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetDuty(string uid, LF_Faction faction, bool onDuty)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(faction);
		account.SetOnDuty(onDuty);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity civilian = LF_TestCharacterFixture.Spawn(ctx);
		IEntity trader = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !civilian || !trader)
		{
			LF_TestCharacterFixture.Delete(officer);
			LF_TestCharacterFixture.Delete(civilian);
			LF_TestCharacterFixture.Delete(trader);
			return;
		}

		string zone = LF_Outpost.ZONE_PREFIX + OUTPOST_ID;
		vector table = trader.GetOrigin();
		LF_Zones.RegisterCircle(zone, LF_ZoneRule.SAFE, table, 20);
		SetDuty(OFFICER_UID, LF_Faction.POLICE, true);
		SetDuty(CIVILIAN_UID, LF_Faction.CIVILIAN, false);

		// The conversation a trader speaks from: one voiced refusal line, as the pack's quartermaster has.
		LF_DialogueLine line = new LF_DialogueLine();
		line.m_sId = LINE_ID;
		line.m_sTextKey = "#LF-Dialogue_OutpostQuartermaster_Refuse";
		line.m_sForKey = LF_Outpost.REFUSE_KEY;
		line.m_sEvent = "SOUND_TEST_REFUSE";
		line.m_fSeconds = 3;
		LF_DialogueConfig voice = new LF_DialogueConfig();
		voice.m_sId = "outpost_officer_voice_test";
		voice.m_aLines = {line};

		// A trader whose conversation holds no refusal line stays quiet; the refusal itself stands.
		LF_OutpostVoice.SetConfigForTests(new LF_DialogueConfig());
		LF_Utils.SetUidForTests(officer, OFFICER_UID);
		LF_MoneyUtils.SetUidForTests(officer, OFFICER_UID);
		int now = System.GetUnixTime();
		int sold;
		int paid;
		int silent = LF_OutpostVoice.SpokenCount();
		ctx.True(LF_OutpostBuyer.Sell(officer, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.REFUSED_OFFICER, "an on-duty officer is refused by a trader with no recording");
		ctx.Equal(silent, LF_OutpostVoice.SpokenCount(), "no recorded refusal: the trader stays quiet");
		if (!RedProof())
			LF_OutpostVoice.SetConfigForTests(voice);

		// The officer reply: offered to an on-duty officer, hidden from everyone else.
		string condition = "OutpostOfficer";
		if (RedProof())
			condition = "";

		LF_DialogueOption badge = Reply("refuse", condition);
		LF_DialogueOption bye = Reply("bye", "");
		ctx.True(LF_DialogueHook.Offers(badge, officer, trader), "an on-duty officer is offered the officer reply");
		ctx.True(LF_DialogueHook.Offers(bye, officer, trader), "a reply with no condition is offered to the officer too");
		ctx.False(LF_DialogueHook.Offers(badge, null, trader), "nobody at all is offered the officer reply");

		// The refusal: the real sale path says no, and the clip goes to the refused officer only.
		int spoken = LF_OutpostVoice.SpokenCount();
		ctx.True(LF_OutpostBuyer.Sell(officer, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.REFUSED_OFFICER, "an on-duty officer is refused at the buyer's table");
		ctx.Equal(spoken + 1, LF_OutpostVoice.SpokenCount(), "the refusal speaks once");
		ctx.EqualStr(LINE_ID, LF_OutpostVoice.LastLine(), "the line is the voiced refusal");
		ctx.EqualStr(OFFICER_UID, LF_OutpostVoice.LastListener(), "the refused officer is the one who hears it");

		// A civilian: never offered the officer reply, never refused, never spoken to.
		LF_Utils.SetUidForTests(civilian, CIVILIAN_UID);
		LF_MoneyUtils.SetUidForTests(civilian, CIVILIAN_UID);
		ctx.False(LF_DialogueHook.Offers(badge, civilian, trader), "a civilian is never offered the officer reply");
		ctx.True(LF_DialogueHook.Offers(bye, civilian, trader), "a civilian is offered the plain replies");
		ctx.False(LF_Outpost.RefusesService(civilian, table), "a civilian is not refused at the outpost");
		spoken = LF_OutpostVoice.SpokenCount();
		LF_EOutpostSell civilianSale = LF_OutpostBuyer.Sell(civilian, OUTPOST_ID, table, now, sold, paid);
		ctx.True(civilianSale != LF_EOutpostSell.REFUSED_OFFICER, "a civilian is not turned away as an officer");
		ctx.Equal(spoken, LF_OutpostVoice.SpokenCount(), "nothing is spoken to a civilian");

		// Off duty, the same officer is a civilian to the outpost.
		SetDuty(OFFICER_UID, LF_Faction.POLICE, false);
		LF_Utils.SetUidForTests(officer, OFFICER_UID);
		ctx.False(LF_DialogueHook.Offers(badge, officer, trader), "an off-duty officer is not offered the officer reply");

		LF_Zones.Unregister(zone);
		LF_Utils.SetUidForTests(null, "");
		LF_MoneyUtils.SetUidForTests(null, "");
		LF_OutpostVoice.ResetStatics();
		SetDuty(OFFICER_UID, LF_Faction.CIVILIAN, false);
		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(civilian);
		LF_TestCharacterFixture.Delete(trader);
	}
}
