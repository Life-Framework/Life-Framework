// red-proof: 2026-11-23 (#4227), --only LF_Test_EveronAmbientBeds before the conf existed: FAIL everon/ambient-beds (the beds conf loads under the pack); green once the conf, its meta and the three sound projects were in. RedProof() skips the conf load.
//! #4227 (WO-W1.1): the harbor, the village square and the night each have an ambience bed row with hours.
//! A bed is a one-track LF_RadioPlaylist (slot, hours, the looped sound project) in
//! `Configs/Atmosphere/LF_AmbientBeds_Everon1989.conf`. The test loads that file itself, so it holds
//! whether or not the mission header already names it: each row has a slot, resolves its sound project,
//! has valid hours (0 to 24, not always-on), the harbor day and night rows leave no hour of the day silent
//! at the quay, and the village night bed plays at night and not at noon.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronAmbientBeds : LF_Test
{
	static const ResourceName BEDS_CONF = "{C05EF16F26511B12}Configs/Atmosphere/LF_AmbientBeds_Everon1989.conf";
	static const string SLOT_HARBOR_DAY = "atmo_bed_harbor_day";
	static const string SLOT_HARBOR_NIGHT = "atmo_bed_harbor_night";
	static const string SLOT_SQUARE = "atmo_bed_square";
	static const string SLOT_VILLAGE_NIGHT = "atmo_bed_village_night";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/ambient-beds";
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
	protected bool PlaysAround(LF_RadioPlaylist list, float hour)
	{
		return list && list.InHours(hour) && list.InHours(hour + 0.5);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBed(LF_TestContext ctx, LF_RadioPlaylist bed, string slot)
	{
		ctx.NotNull(bed, "bed row " + slot + " exists");
		if (!bed)
			return;

		ctx.True(bed.m_aTracks && bed.m_aTracks.Count() == 1, slot + " has exactly one looped track");
		ctx.True(bed.m_fHourFrom >= 0 && bed.m_fHourFrom < 24, slot + " starts at an hour from 0 to 24");
		ctx.True(bed.m_fHourTo >= 0 && bed.m_fHourTo <= 24, slot + " ends at an hour from 0 to 24");
		ctx.True(bed.m_fHourFrom != bed.m_fHourTo, slot + " has real hours, not always on");
		ctx.True(bed.PlayableCount() == 1, slot + " resolves its sound project");
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
		LF_RadioPlaylist harborDay = FindBed(config, SLOT_HARBOR_DAY);
		LF_RadioPlaylist harborNight = FindBed(config, SLOT_HARBOR_NIGHT);
		LF_RadioPlaylist square = FindBed(config, SLOT_SQUARE);
		LF_RadioPlaylist villageNight = FindBed(config, SLOT_VILLAGE_NIGHT);
		CheckBed(ctx, harborDay, SLOT_HARBOR_DAY);
		CheckBed(ctx, harborNight, SLOT_HARBOR_NIGHT);
		CheckBed(ctx, square, SLOT_SQUARE);
		CheckBed(ctx, villageNight, SLOT_VILLAGE_NIGHT);

		// The quay is never silent: the day bed or the water-only night bed covers every half hour.
		for (int hour = 0; hour < 24; hour++)
		{
			bool quay = PlaysAround(harborDay, hour) || PlaysAround(harborNight, hour);
			ctx.True(quay, string.Format("a harbor bed plays at %1:00 and %1:30", hour));
		}

		// Gulls only by day; the night bed is the quiet one.
		ctx.True(harborDay && harborDay.InHours(12), "the harbor day bed plays at noon");
		ctx.True(harborDay && !harborDay.InHours(2), "the harbor day bed is off at 02:00");
		ctx.True(harborNight && harborNight.InHours(2), "the harbor night bed plays at 02:00");
		ctx.True(harborNight && !harborNight.InHours(12), "the harbor night bed is off at noon");

		// The village: the square by day, crickets and a far dog at night, never both.
		ctx.True(square && square.InHours(12), "the square bed plays at noon");
		ctx.True(square && !square.InHours(2), "the square bed is off at 02:00");
		ctx.True(villageNight && villageNight.InHours(23), "the village night bed plays at 23:00");
		ctx.True(villageNight && villageNight.InHours(3), "the village night bed plays at 03:00");
		ctx.True(villageNight && !villageNight.InHours(12), "the village night bed is off at noon");
	}
}
