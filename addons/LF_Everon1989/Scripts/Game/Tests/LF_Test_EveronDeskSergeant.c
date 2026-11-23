// red-proof: to be recorded from `tools\cli test --no-build --red LF_Test_EveronDeskSergeant` (RedProof() skips loading the dialogue and the booking announcement, so every assertion on them fails).
//! The desk sergeant's booking lines and the booking desk's sounds (#4431, police L9), under the Everon
//! 1989 header (the pack leg of `tools\cli test`). Every line the sergeant says for a ticket, a booking,
//! paying now and the cell resolves to a voiced clip that loads; no other NPC dialogue uses his voice
//! (offline: `validate-dialogue-voices`); and a booking announces the typewriter and the stamp, both
//! filled by the pack. The sergeant's stand-in behaviour is WO-W4.8; this card writes his lines only.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronDeskSergeant : LF_Test
{
	static const ResourceName DIALOGUE = "{E61BB56805BBC506}Configs/Dialogue/LF_Dialogue_DeskSergeant.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/desk-sergeant";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLine(LF_TestContext ctx, LF_DialogueConfig config, string lineId, ResourceName clip)
	{
		LF_DialogueLine line = LF_DialogueFlow.FindLine(config, lineId);
		ctx.NotNull(line, lineId + ": the sergeant has the line");
		if (!line)
			return;

		ctx.True(!line.m_sEvent.IsEmpty(), lineId + ": it has a sound event");
		ctx.True(line.m_fSeconds > 0, lineId + ": it has a length");
		ctx.True(!line.m_sMouth.IsEmpty(), lineId + ": it has a mouth envelope");
		ctx.True(Resource.Load(clip).IsValid(), lineId + ": its voiced file loads");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_MissionHeader.GetCurrent(), "the pack leg runs under an LF mission header");

		LF_DialogueConfig config;
		if (!RedProof())
			config = LF_DialogueConfig.Load(DIALOGUE);

		ctx.NotNull(config, "the desk sergeant's dialogue loads");
		if (config)
		{
			ctx.EqualStr("desk_sergeant", config.m_sId, "the dialogue is the desk sergeant's");
			ctx.True(Resource.Load(config.m_sSoundProject).IsValid(), "the dialogue's audio project loads");
			CheckLine(ctx, config, "ticket", "{5978C5BF003516D8}Sounds/Dialogue/desk_sergeant/desk_sergeant_ticket.wav");
			CheckLine(ctx, config, "booking", "{C74B1018D5016620}Sounds/Dialogue/desk_sergeant/desk_sergeant_booking.wav");
			CheckLine(ctx, config, "pay_now", "{A663E20DAF08BB67}Sounds/Dialogue/desk_sergeant/desk_sergeant_pay_now.wav");
			CheckLine(ctx, config, "cell", "{59D52FF138AD5E65}Sounds/Dialogue/desk_sergeant/desk_sergeant_cell.wav");
		}

		// The pack fills both booking slots with a project that loads.
		LF_SoundSlots.ResetStatics();
		ctx.True(!LF_SoundSlots.Get(LF_BookingSounds.SLOT_TYPEWRITER).IsEmpty(), "pack: the booking typewriter slot has a sound");
		ctx.True(!LF_SoundSlots.Get(LF_BookingSounds.SLOT_STAMP).IsEmpty(), "pack: the booking stamp slot has a sound");

		// The booking event: both slots are announced (the log line is `[LFDebug:BookingSounds] booking sounds: ...`).
		LF_BookingSounds.ResetStatics();
		int ready;
		if (!RedProof())
			ready = LF_BookingSounds.OnBooked(null);

		ctx.Equal(2, ready, "a booking plays the typewriter and the stamp slots");
		ctx.Equal(1, LF_BookingSounds.Announced(), "a booking is announced once");

		LF_BookingSounds.ResetStatics();
		LF_SoundSlots.ResetStatics();
	}
}
