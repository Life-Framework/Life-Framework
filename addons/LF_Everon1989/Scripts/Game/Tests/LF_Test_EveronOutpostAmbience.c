// red-proof: 2026-11-24 (#4460), --only LF_Test_EveronOutpostAmbience before the outpost rows existed: FAIL everon/outpost-ambience (no atmo_bed_outpost_north_* row); green once the four rows and their sound projects were in. RedProof() skips the conf load.
//! #4460 (GA-O12): the north outpost has its camp ambience as rows in the ambience bed table.
//! Four one-track beds in `Configs/Atmosphere/LF_AmbientBeds_Everon1989.conf`, slot `atmo_bed_outpost_<id>_<voice>`:
//! a generator hum by day, a crackling fire in the evening, a radio murmuring in a tent and wind through the nets
//! at night. The test loads the file itself: each row has hours, resolves its sound project, the slot carries the
//! outpost id and that id is a real outpost in the pack's outpost stock table, and the camp is never silent.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronOutpostAmbience : LF_Test
{
	static const ResourceName BEDS_CONF = "{C05EF16F26511B12}Configs/Atmosphere/LF_AmbientBeds_Everon1989.conf";
	static const ResourceName STOCK_CONF = "{7E21C5A93D40B8F6}Configs/Outposts/LF_OutpostStock_Everon1989.conf";
	static const string OUTPOST_ID = "north";
	static const string SLOT_PREFIX = "atmo_bed_outpost_";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/outpost-ambience";
	}

	//------------------------------------------------------------------------------------------------
	protected static string SlotFor(string voice)
	{
		return SLOT_PREFIX + OUTPOST_ID + "_" + voice;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RadioPlaylist FindBed(LF_RadioPlaylistConfig config, string slot)
	{
		if (!config || !config.m_aPlaylists)
			return null;

		foreach (LF_RadioPlaylist list : config.m_aPlaylists)
		{
			if (list && list.m_sSlot == slot)
				return list;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBed(LF_TestContext ctx, LF_RadioPlaylist bed, string slot)
	{
		ctx.NotNull(bed, "outpost bed row " + slot + " exists");
		if (!bed)
			return;

		ctx.True(bed.m_aTracks && bed.m_aTracks.Count() == 1, slot + " has exactly one looped track");
		ctx.True(bed.m_fHourFrom >= 0 && bed.m_fHourFrom < 24, slot + " starts at an hour from 0 to 24");
		ctx.True(bed.m_fHourTo >= 0 && bed.m_fHourTo <= 24, slot + " ends at an hour from 0 to 24");
		ctx.True(bed.m_fHourFrom != bed.m_fHourTo, slot + " has real hours, not always on");
		ctx.True(bed.PlayableCount() == 1, slot + " resolves its sound project");
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlaysAround(LF_RadioPlaylist list, float hour)
	{
		return list && list.InHours(hour) && list.InHours(hour + 0.5);
	}

	//------------------------------------------------------------------------------------------------
	protected bool OutpostIsInStockTable()
	{
		Resource resource = Resource.Load(STOCK_CONF);
		if (!resource || !resource.IsValid())
			return false;

		LF_OutpostStockTableConfig stock = LF_OutpostStockTableConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		if (!stock || !stock.m_aTables)
			return false;

		foreach (LF_OutpostStockTable table : stock.m_aTables)
		{
			if (table && table.m_sOutpostId == OUTPOST_ID)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_RadioPlaylistConfig config;
		if (!RedProof())
		{
			Resource resource = Resource.Load(BEDS_CONF);
			ctx.True(resource && resource.IsValid(), "the beds conf loads under the pack");
			if (resource && resource.IsValid())
				config = LF_RadioPlaylistConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		}

		ctx.NotNull(config, "the beds conf is a radio playlist config");
		LF_RadioPlaylist generator = FindBed(config, SlotFor("generator"));
		LF_RadioPlaylist fire = FindBed(config, SlotFor("fire"));
		LF_RadioPlaylist radio = FindBed(config, SlotFor("radio"));
		LF_RadioPlaylist wind = FindBed(config, SlotFor("wind"));
		CheckBed(ctx, generator, SlotFor("generator"));
		CheckBed(ctx, fire, SlotFor("fire"));
		CheckBed(ctx, radio, SlotFor("radio"));
		CheckBed(ctx, wind, SlotFor("wind"));

		// The rows are tied to the outpost place: the slot names the outpost id, and that id is a real outpost.
		ctx.True(OutpostIsInStockTable(), "the outpost id " + OUTPOST_ID + " is an outpost in the pack's stock table");

		// The hours: hum and radio by day, the fire from evening, the wind through the nets at night, never both ends.
		ctx.True(generator && generator.InHours(12), "the generator hums at noon");
		ctx.True(generator && !generator.InHours(2), "the generator is off at 02:00");
		ctx.True(radio && radio.InHours(14), "the tent radio murmurs at 14:00");
		ctx.True(radio && !radio.InHours(3), "the tent radio is off at 03:00");
		ctx.True(fire && fire.InHours(20), "the fire crackles at 20:00");
		ctx.True(fire && fire.InHours(1), "the fire crackles at 01:00");
		ctx.True(fire && !fire.InHours(12), "the fire is out at noon");
		ctx.True(wind && wind.InHours(23), "the wind in the nets blows at 23:00");
		ctx.True(wind && wind.InHours(3), "the wind in the nets blows at 03:00");
		ctx.True(wind && !wind.InHours(12), "the wind in the nets is off at noon");

		// The camp is never silent: some outpost bed plays every half hour.
		for (int hour = 0; hour < 24; hour++)
		{
			bool camp = PlaysAround(generator, hour) || PlaysAround(fire, hour) || PlaysAround(radio, hour) || PlaysAround(wind, hour);
			ctx.True(camp, string.Format("an outpost bed plays at %1:00 and %1:30", hour));
		}
	}
}
