// red-proof: observed 2026-11-22 with `tools/cli test --no-build --red LF_Test_GangRadioBand`: with the disband event skipped, RED-OK on "the disbanded gang holds no band" and "after the release a new gang gets a band".
//------------------------------------------------------------------------------------------------
//! comms/gang-radio-band (GA-C7, #4215): every gang band sits on the R148 grid inside the shipped
//! range and off the service frequencies; two gangs never share a band; a gang asking again keeps
//! its band; with every channel held a new gang is refused; the disband event (an organization's
//! last member left) frees exactly that band, and only then does the next gang get it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GangRadioBand : LF_Test
{
	protected static const string ORG_PREFIX = "test-gang-band-";

	override string GetName()
	{
		return "comms/gang-radio-band";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_GangRadio.ResetStatics();

		LF_GangRadioBands bands = LF_GangRadio.Bands();
		array<int> channels = {};
		bands.Channels(channels);
		ctx.True(channels.Count() >= 20, string.Format("the shipped config gives at least 20 gang channels (got %1)", channels.Count()));

		int policeFreq = LF_ServiceCallsConfig.Frequency("police");
		int medicFreq = LF_ServiceCallsConfig.Frequency("medic");
		bool onGrid = true;
		foreach (int channel : channels)
		{
			if (channel < 38000 || channel > 54000 || (channel - 38000) % 200 != 0 || channel == 38000)
				onGrid = false;
		}

		ctx.True(onGrid, "every gang channel is an R148 channel (38000 to 54000 kHz, 200 kHz steps) and not its factory channel");
		ctx.True(channels.Find(policeFreq) < 0 && channels.Find(medicFreq) < 0, "no gang channel is a service radio frequency");

		// Fill every channel: no two gangs share one.
		map<int, string> seen = new map<int, string>();
		bool shared = false;
		int assigned = 0;
		for (int i = 0; i < channels.Count(); i++)
		{
			string orgId = ORG_PREFIX + i.ToString();
			LF_GangRadioBand band = LF_GangRadio.BandFor(orgId);
			if (!band)
				continue;

			assigned++;
			if (seen.Contains(band.m_iFrequency))
				shared = true;

			seen.Set(band.m_iFrequency, orgId);
		}

		ctx.Equal(channels.Count(), assigned, "every channel can be given to a gang");
		ctx.False(shared, "two gangs never hold the same band");

		LF_GangRadioBand first = LF_GangRadio.BandFor(ORG_PREFIX + "0");
		ctx.NotNull(first, "the first gang still has its band");
		if (!first)
		{
			LF_GangRadio.ResetStatics();
			return;
		}

		ctx.Equal(0, bands.FreeCount(), "with every channel held none is free");
		LF_GangRadioBand again = LF_GangRadio.BandFor(ORG_PREFIX + "0");
		ctx.True(again == first, "a gang asking again keeps its band");
		ctx.True(!first.m_sKey.IsEmpty(), "a band carries an encryption key");

		string lateId = ORG_PREFIX + "late";
		ctx.True(LF_GangRadio.BandFor(lateId) == null, "with every channel held a new gang is refused");

		int freed = first.m_iFrequency;
		string oldKey = first.m_sKey;
		if (!RedProof())
			LF_EventBus.Publish(LF_OnOrganizationEmptied.Create(ORG_PREFIX + "0", "test disband"));

		ctx.True(LF_GangRadio.Find(ORG_PREFIX + "0") == null, "the disbanded gang holds no band");
		LF_GangRadioBand late = LF_GangRadio.BandFor(lateId);
		ctx.NotNull(late, "after the release a new gang gets a band");
		if (late)
		{
			ctx.Equal(freed, late.m_iFrequency, "the new gang gets the released band, the only free one");
			ctx.True(late.m_sKey != oldKey, "the reused band has a fresh key, so the old gang's radios do not hear it");
		}

		LF_GangRadioBand second = LF_GangRadio.Find(ORG_PREFIX + "1");
		ctx.True(second && bands.HolderOf(second.m_iFrequency) == ORG_PREFIX + "1", "the other gangs keep their bands");

		LF_GangRadio.ResetStatics();
	}
}
