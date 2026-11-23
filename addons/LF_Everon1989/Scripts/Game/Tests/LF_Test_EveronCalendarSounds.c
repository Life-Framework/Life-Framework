// red-proof: 2026-11-23 (#4477), `tools cli test --no-build --red LF_Test_EveronCalendarSounds`: RedProof() skips spawning each carrier prefab, so the component, no-mesh, sound and operation checks all fail (the operations are registered at game start, so skipping that call proves nothing); green with `--only` once the carrier prefabs, sounds and LF_CalendarSound were in.
//! #4477 (WO-W6.2): the island calendar's bell, Sunday peal, shift siren and ship's horn each name a
//! carrier prefab with a sound. The test reads the shipped calendar rows (core conf) and checks the
//! hours the plan asks for, then for each sound row: the carrier prefab loads, answers to the row's
//! operation id, names a sound project that resolves, and has no mesh (an invisible emitter, never a
//! visible prop); the operation is registered on the event shell and runs for a placed carrier, and a
//! row with no carrier placed is handled and silent, not retried every second.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCalendarSounds : LF_Test
{
	static const ResourceName BELL_HOUR = "{CA01343AF14EA0BA}Prefabs/Atmosphere/LF_BellHour.et";
	static const ResourceName BELL_SERVICE = "{EB41CD31CE7157B2}Prefabs/Atmosphere/LF_BellService.et";
	static const ResourceName SIREN_SHIFT = "{7CEDFBCC105CECFF}Prefabs/Atmosphere/LF_SirenShift.et";
	static const ResourceName SHIP_HORN = "{4A360B5CEDDC0855}Prefabs/Atmosphere/LF_ShipHorn.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/calendar-sounds";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_IslandCalendarRow FindRow(LF_IslandCalendarConfig config, string operationId)
	{
		if (!config || !config.m_aRows)
			return null;

		foreach (LF_IslandCalendarRow row : config.m_aRows)
		{
			if (row && row.m_sOperationId == operationId)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRow(LF_TestContext ctx, LF_IslandCalendarConfig config, string operationId, int weekday, int from, int to, int every)
	{
		LF_IslandCalendarRow row = FindRow(config, operationId);
		ctx.NotNull(row, "the calendar has a row for " + operationId);
		if (!row)
			return;

		ctx.Equal(weekday, row.m_iWeekday, operationId + " weekday");
		ctx.Equal(from, row.m_iFromHour, operationId + " first hour");
		ctx.Equal(to, row.m_iToHour, operationId + " last hour");
		ctx.Equal(every, row.m_iEveryHours, operationId + " step");
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the carrier, checks it, runs the row's operation for it, deletes it.
	protected void CheckCarrier(LF_TestContext ctx, ResourceName prefab, string operationId)
	{
		IEntity carrier;
		if (!RedProof())
			carrier = LF_Utils.SpawnEntityPrefab(prefab, "0 0 0");

		ctx.NotNull(carrier, operationId + ": the carrier prefab spawns");
		if (!carrier)
			return;

		LF_CalendarCarrierComponent component = LF_CalendarCarrierComponent.Cast(carrier.FindComponent(LF_CalendarCarrierComponent));
		ctx.NotNull(component, operationId + ": the carrier has the carrier component");
		ctx.True(carrier.GetVObject() == null, operationId + ": the carrier is invisible (no mesh)");
		if (component)
		{
			ctx.EqualStr(operationId, component.OperationId(),operationId + ": the carrier answers to the row's operation");
			Resource sound = Resource.Load(component.SoundProject());
			ctx.True(!component.SoundProject().IsEmpty() && sound && sound.IsValid(), operationId + ": the sound project resolves");
			ctx.True(component.Seconds() > 0, operationId + ": the sample length is set");
		}

		ctx.True(LF_CalendarCarrierComponent.CountFor(operationId) >= 1, operationId + ": the carrier is counted");

		LF_EventOperation operation = LF_EventShell.FindOperation(operationId);
		ctx.NotNull(operation, operationId + ": the operation is registered on the event shell");
		if (operation)
			ctx.True(operation.Run(LF_IslandCalendar.EVENT_ID, "test_row", null), operationId + ": the operation rings for a placed carrier");

		SCR_EntityHelper.DeleteEntityAndChildren(carrier);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_IslandCalendarConfig config = LF_IslandCalendarLoad.Get();
		ctx.NotNull(config, "the core calendar conf loads");

		// The plan's hours: bells 07 to 21 every hour, the Sunday service, the siren at 06, 14, 22, the horn at 07 and 19.
		CheckRow(ctx, config, LF_CalendarSound.OP_BELL, 0, 7, 21, 1);
		CheckRow(ctx, config, LF_CalendarSound.OP_SERVICE, 7, 9, 9, 1);
		CheckRow(ctx, config, LF_CalendarSound.OP_SIREN, 0, 6, 22, 8);
		CheckRow(ctx, config, LF_CalendarSound.OP_HORN, 0, 7, 19, 12);

		// The game mode registered the operations at game start; this makes a repeat harmless.
		LF_CalendarSound.RegisterOperations();

		CheckCarrier(ctx, BELL_HOUR, LF_CalendarSound.OP_BELL);
		CheckCarrier(ctx, BELL_SERVICE, LF_CalendarSound.OP_SERVICE);
		CheckCarrier(ctx, SIREN_SHIFT, LF_CalendarSound.OP_SIREN);
		CheckCarrier(ctx, SHIP_HORN, LF_CalendarSound.OP_HORN);

		// No carrier placed: the row is handled (so the calendar does not retry it every second) and says so once.
		LF_EventOperation bell = LF_EventShell.FindOperation(LF_CalendarSound.OP_BELL);
		ctx.True(LF_CalendarCarrierComponent.CountFor(LF_CalendarSound.OP_BELL) == 0, "the test's carriers are gone");
		if (bell)
			ctx.True(bell.Run(LF_IslandCalendar.EVENT_ID, "test_row", null), "a due row with no carrier is handled, not retried");
		else
			ctx.Fail("the bell operation is registered");
	}
}
