// red-proof: 2026-11-24 r7-small-items: with the first Watch skipped the second one subscribes and returns true, so "a second Watch is a no-op" fails (see the --red run below). Earlier: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_GangRadioTune`: RED-OK, 24 assertions; with the member-left watch skipped, "a member leaving redraws the band key" and "the leaver's radio is not on the new band" fail; `--only` then passed.
//------------------------------------------------------------------------------------------------
//! comms/gang-radio-tune (GA-C8, #4221): a member's radio tuned to the gang band sits on its
//! frequency and key; an outsider who dials the frequency by hand is not on the band (no key); a
//! radio left on a stale band (before a restart or a rekey) retunes cleanly; when a member leaves
//! the band is redrawn with a new key, radios on the old band follow, radios elsewhere are left
//! alone, and the leaver's radio comes off; the last member leaving releases the band.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GangRadioTune : LF_Test
{
	protected static const ResourceName RADIO = "{E1A5D4B878AA8980}Prefabs/Items/Equipment/Radios/Radio_R148.et";
	protected static const string FOUNDER = "gangtune-founder";
	protected static const string MEMBER = "gangtune-member";
	protected static const string THIRD = "gangtune-third";

	protected ref array<IEntity> m_aSpawned = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "comms/gang-radio-tune";
	}

	//------------------------------------------------------------------------------------------------
	protected BaseRadioComponent SpawnRadio(int index)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(RADIO, Vector(420 + index, 2, 120));
		if (!entity)
			return null;

		m_aSpawned.Insert(entity);
		return BaseRadioComponent.Cast(entity.FindComponent(BaseRadioComponent));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_GangRadio.ResetStatics();
		LF_GangRadioTune.ResetStatics();
		bool firstWatch = false;
		if (!RedProof())
			firstWatch = LF_GangRadioTune.Watch();

		// Every tune calls Watch(); a second call must not subscribe again (it logged a bus warning each time).
		ctx.True(firstWatch, "the first Watch subscribes the member-left listener");
		ctx.False(LF_GangRadioTune.Watch(), "a second Watch is a no-op: the listener is already on the bus");

		string orgName = "Tune Crew " + Math.RandomInt(1000, 9999).ToString();
		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, orgName);
		ctx.NotNull(org, "the gang is founded");
		if (!org)
			return;

		string orgId = org.GetId();
		ctx.True(LF_OrganizationManager.JoinDirect(orgId, MEMBER, LF_OrganizationConfig.RANK_MEMBER), "the member joins");
		ctx.True(LF_OrganizationManager.JoinDirect(orgId, THIRD, LF_OrganizationConfig.RANK_MEMBER), "a third member joins");

		BaseRadioComponent founderRadio = SpawnRadio(0);
		BaseRadioComponent memberRadio = SpawnRadio(1);
		BaseRadioComponent outsiderRadio = SpawnRadio(2);
		BaseRadioComponent leaverRadio = SpawnRadio(3);
		ctx.True(founderRadio && memberRadio && outsiderRadio && leaverRadio, "four R148 radios spawn with a radio component");
		if (!founderRadio || !memberRadio || !outsiderRadio || !leaverRadio)
		{
			Cleanup(orgId);
			return;
		}

		LF_GangRadioBand band = LF_GangRadio.BandFor(orgId);
		ctx.NotNull(band, "the gang gets a band");
		if (!band)
		{
			Cleanup(orgId);
			return;
		}

		// Two members tuned hear each other: same frequency and key.
		ctx.True(LF_GangRadioTune.TuneTo(founderRadio, band), "the founder's radio tunes to the band");
		ctx.True(LF_GangRadioTune.TuneTo(memberRadio, band), "the member's radio tunes to the band");
		ctx.True(LF_GangRadioTune.IsOnBand(founderRadio, band) && LF_GangRadioTune.IsOnBand(memberRadio, band), "both members' radios sit on the band");
		ctx.True(founderRadio.IsPowered(), "a tuned radio is switched on");

		// An outsider dialing the frequency by hand has no key: not on the band.
		outsiderRadio.GetTransceiver(0).SetFrequency(band.m_iFrequency);
		ctx.False(LF_GangRadioTune.IsOnBand(outsiderRadio, band), "an outsider on the gang frequency without the key is not on the band");

		// A radio from before a restart: stale frequency and key, retunes cleanly.
		leaverRadio.GetTransceiver(0).SetFrequency(38000);
		leaverRadio.SetEncryptionKey("LF_gang_000001_000001");
		ctx.True(LF_GangRadioTune.TuneTo(leaverRadio, band), "a radio on a stale band retunes");
		ctx.True(LF_GangRadioTune.IsOnBand(leaverRadio, band), "the retuned radio sits on the current band");

		int oldFrequency = band.m_iFrequency;
		string oldKey = band.m_sKey;

		// The third member leaves: the band is redrawn with a new key.
		ctx.True(LF_OrganizationManager.LeaveDirect(THIRD), "the third member leaves");
		LF_GangRadioBand fresh = LF_GangRadio.Find(orgId);
		ctx.NotNull(fresh, "the gang still holds a band after a member left");
		if (fresh)
		{
			ctx.True(fresh.m_sKey != oldKey, "a member leaving redraws the band key");

			// Online members' radios follow the band (here handed over directly: no characters).
			array<BaseRadioComponent> radios = {founderRadio, memberRadio, outsiderRadio};
			ctx.Equal(2, LF_GangRadioTune.Retune(radios, oldFrequency, oldKey, fresh), "the two radios on the old band move, the outsider's stays");
			ctx.True(LF_GangRadioTune.IsOnBand(founderRadio, fresh) && LF_GangRadioTune.IsOnBand(memberRadio, fresh), "the members' radios sit on the new band");
			ctx.False(LF_GangRadioTune.IsOnBand(leaverRadio, fresh), "the leaver's radio is not on the new band");
			ctx.False(LF_GangRadioTune.IsOnBand(outsiderRadio, fresh), "the outsider's radio is not on the new band");

			// The leaver's radio comes off the old band.
			array<BaseRadioComponent> leaver = {leaverRadio};
			ctx.Equal(1, LF_GangRadioTune.Retune(leaver, oldFrequency, oldKey, null), "the leaver's radio is taken off the old band");
			ctx.True(leaverRadio.GetEncryptionKey().IsEmpty(), "the leaver's radio holds no gang key");
		}

		// The last members leave: the band is released.
		LF_OrganizationManager.LeaveDirect(MEMBER);
		LF_OrganizationManager.LeaveDirect(FOUNDER);
		ctx.True(LF_GangRadio.Find(orgId) == null, "an emptied gang holds no band");

		Cleanup("");
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(string orgId)
	{
		if (!orgId.IsEmpty())
		{
			LF_OrganizationManager.LeaveDirect(THIRD);
			LF_OrganizationManager.LeaveDirect(MEMBER);
			LF_OrganizationManager.LeaveDirect(FOUNDER);
		}

		foreach (IEntity entity : m_aSpawned)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}

		m_aSpawned.Clear();
		LF_GangRadioTune.ResetStatics();
		LF_GangRadio.ResetStatics();
	}
}
