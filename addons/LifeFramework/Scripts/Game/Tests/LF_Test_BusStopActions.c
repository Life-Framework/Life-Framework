// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_BusStop,LF_Test_BusStopWorld,LF_Test_BusStopActions`: RED-OK travel/bus-stop-actions (fails with the hook on, 6 assertions)
//! Bus stop actions and the printed board, on three real stop prefabs: the prefab carries one
//! destination action per slot and the "Cancel trip" action; a stop offers every other stop nearest
//! first with the fare and wait the server charges, never itself; the action reads "Bus to <stop>";
//! the board prints this stop first marked "you are here", then the others in the same order.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BusStopActions : LF_Test
{
	static const ResourceName STOP_PREFAB = "{D3B1C7A4E2F05986}Prefabs/Travel/LF_BusStop.et";

	override string GetName()
	{
		return "travel/bus-stop-actions";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		vector origin = character.GetOrigin();
		IEntity here = LF_Utils.SpawnEntityPrefab(STOP_PREFAB, origin + "3 0 0");
		IEntity far = LF_Utils.SpawnEntityPrefab(STOP_PREFAB, origin + "3 0 90");
		IEntity near = LF_Utils.SpawnEntityPrefab(STOP_PREFAB, origin + "30 0 0");
		array<IEntity> spawned = {here, far, near};
		if (!here || !far || !near)
		{
			ctx.Fail("bus stop prefab did not spawn: " + STOP_PREFAB);
			Cleanup(character, spawned);
			return;
		}

		// The prefab: one destination action per slot, then the cancel.
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(here.FindComponent(ActionsManagerComponent));
		int destinations = 0;
		int cancels = 0;
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction action : list)
			{
				if (LF_BusStopAction.Cast(action))
					destinations++;
				else if (LF_BusCancelAction.Cast(action))
					cancels++;
			}
		}

		ctx.Equal(LF_BusTicket.MAX_SLOTS, destinations, "the stop prefab carries one destination action per slot");
		ctx.Equal(1, cancels, "the stop prefab carries one Cancel trip action");

		LF_BusStopConfig config = LF_BusStopConfig.CreateDefault();
		array<IEntity> stops = {far, here, near};
		array<ref LF_BusOffer> offers = {};
		array<ref LF_SignRow> rows = {};
		if (!RedProof())
		{
			LF_BusTicket.Offers(here, stops, config, offers);
			LF_BusTimetableSource.BuildRows(here, stops, config, rows);
		}

		LF_BusStopComponent nearStop = LF_BusStopComponent.Of(near);
		LF_BusStopComponent farStop = LF_BusStopComponent.Of(far);
		LF_BusStopComponent hereStop = LF_BusStopComponent.Of(here);
		ctx.Equal(2, offers.Count(), "a stop offers every other stop and never itself");
		if (offers.Count() == 2 && nearStop && farStop)
		{
			ctx.EqualStr(nearStop.GetStopId(), offers[0].m_sToId, "the nearest stop is the first action");
			ctx.EqualStr(farStop.GetStopId(), offers[1].m_sToId, "the far stop comes after it");
			ctx.Equal(LF_BusStop.Fare(config, vector.Distance(here.GetOrigin(), near.GetOrigin())), offers[0].m_iFare, "the action shows the fare the server charges");
			ctx.Equal(Math.Round(LF_BusStop.WaitSeconds(config, vector.Distance(here.GetOrigin(), far.GetOrigin()))), offers[1].m_iWait, "the action shows the wait the server keeps");
			string name = LF_BusTicket.ActionName(offers[0]);
			ctx.True(name.Contains(WidgetManager.Translate(nearStop.GetLabel())), "the action names its stop: " + name);
		}

		ctx.Equal(3, rows.Count(), "the board lists this stop and both others");
		if (rows.Count() == 3 && hereStop && nearStop)
		{
			ctx.EqualStr(hereStop.GetLabel(), rows[0].m_sLabel, "the board's first row is this stop");
			ctx.EqualStr("#LF-BusStop_BoardHere", rows[0].m_sValue, "this stop is marked you are here");
			ctx.EqualStr(nearStop.GetLabel(), rows[1].m_sLabel, "the board lists the nearest stop next");
			ctx.False(rows[1].m_sValue.IsEmpty(), "every other stop has its fare on the board");
		}

		Cleanup(character, spawned);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, array<IEntity> spawned)
	{
		foreach (IEntity stop : spawned)
		{
			LF_BusStopComponent component = LF_BusStopComponent.Of(stop);
			if (component)
				LF_Places.Unregister(LF_BusStop.PLACE_KIND, component.GetStopId());

			LF_TestCharacterFixture.Delete(stop);
		}

		LF_TestCharacterFixture.Delete(character);
	}
}
