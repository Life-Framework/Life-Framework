// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_ConfZeroConfigs`: with `bool m_bDownedEnabled = true;` and `int m_iFoundingFee = 250;` put back, `conf 0: the downed state is off` and `conf 0: founding is free` failed (the fields without an initializer passed); removed; green.
//------------------------------------------------------------------------------------------------
//! config/conf-zero (#1077): a `.conf` that sets a config class's field to 0 reads as 0 (an
//! `[Attribute]` member initializer beats it), and a field the `.conf` leaves out reads the attribute
//! default, which is what CreateDefault() hands script. Covers the classes that used to carry
//! initializers: LF_MedicConfig (flags, seconds, an amount) and LF_OrgConfig (the founding fee).
// tier: WORLD
class LF_Test_ConfZeroConfigs : LF_Test
{
	static const ResourceName MEDIC_CONF = "{AC89FEF3BD409CE4}Configs/Tests/LF_Test_MedicOff.conf";
	static const ResourceName ORG_CONF = "{2ABC36B260DF9AA2}Configs/Tests/LF_Test_OrgFree.conf";

	override string GetName()
	{
		return "config/conf-zero";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource medicRes = BaseContainerTools.LoadContainer(MEDIC_CONF);
		ctx.True(medicRes && medicRes.IsValid() && medicRes.GetResource(), "the medic test conf loads");
		if (medicRes && medicRes.IsValid() && medicRes.GetResource())
		{
			LF_MedicConfig medic = LF_MedicConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(medicRes.GetResource().ToBaseContainer()));
			ctx.True(medic != null, "the medic test conf is an LF_MedicConfig");
			if (medic)
			{
				ctx.False(medic.m_bDownedEnabled, "conf 0: the downed state is off");
				ctx.True(medic.m_fBleedOutSeconds == 0, "conf 0: bleed out reads 0 seconds");
				ctx.True(medic.m_iReviveBillAmount == 0, "conf 0: a free revive reads 0");
				ctx.False(medic.m_bNewLifeEnabled, "conf 0: the new-life rule is off");

				// A field the conf leaves out keeps the attribute default, and CreateDefault matches it.
				LF_MedicConfig defaults = LF_MedicConfig.CreateDefault();
				ctx.True(medic.m_fCarryRange == defaults.m_fCarryRange, "unset medic float equals CreateDefault");
				ctx.True(medic.m_bCarryAnyone == defaults.m_bCarryAnyone, "unset medic flag equals CreateDefault");
				ctx.True(medic.m_iBillDeadlineHours == defaults.m_iBillDeadlineHours, "unset medic int equals CreateDefault");
				ctx.True(medic.m_fBedRange == defaults.m_fBedRange, "unset medic bed range equals CreateDefault");
			}
		}

		Resource orgRes = BaseContainerTools.LoadContainer(ORG_CONF);
		ctx.True(orgRes && orgRes.IsValid() && orgRes.GetResource(), "the org test conf loads");
		if (orgRes && orgRes.IsValid() && orgRes.GetResource())
		{
			LF_OrgConfig org = LF_OrgConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(orgRes.GetResource().ToBaseContainer()));
			ctx.True(org != null, "the org test conf is an LF_OrgConfig");
			if (org)
			{
				ctx.True(org.m_iFoundingFee == 0, "conf 0: founding is free");
				ctx.True(org.m_iNameMin == LF_OrgConfig.DEFAULT_NAME_MIN, "unset org int keeps its attribute default");
			}
		}
	}
}
