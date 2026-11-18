// red-proof: 2026-11-18 on the unfixed SyncOrgBase (`m_sBaseOrgId == org` early return) `tools/cli test --no-build --only LF_Test_SiteCaptureBaseReload` failed "the base door holds the organization's base place after the load" and "a second tenancy apply keeps the base place on the base door"; green with the registry check.
//! sites/capture-base-reload (#2816 review): an organization with a base door and a captured door keeps
//! its base place after a load. The "hideout" load registers every lease as a base in record order, so
//! the captured door takes the place over from the base; the "tenancy" load must hand it back to the
//! base door, not leave the organization with no base place at all.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SiteCaptureBaseReload : LF_Test
{
	//! A door spawned for this test is the base; the annex door is the captured site.
	protected static const string BASE_SITE = "capture-reload-base";
	protected static const ResourceName DOOR_PREFAB = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "sites/capture-base-reload";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		IEntity capturedDoor = LF_Places.Find(LF_Hideout.PLACE_KIND_DOOR, LF_TestAnnex.HIDEOUT_ID);
		ctx.NotNull(capturedDoor, "the annex hideout door is bound");
		if (!capturedDoor)
			return;

		IEntity baseDoor = LF_Utils.SpawnEntityPrefab(DOOR_PREFAB, capturedDoor.GetOrigin() + "0 0 6");
		ctx.NotNull(baseDoor, "a base door spawns");
		if (baseDoor)
		{
			LF_Hideout.BindDoor(BASE_SITE, baseDoor);
			RunSeeded(ctx, baseDoor);
			LF_Places.Unregister(LF_Hideout.PLACE_KIND_DOOR, BASE_SITE);
			SCR_EntityHelper.DeleteEntityAndChildren(baseDoor);
		}

		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSeeded(LF_TestContext ctx, IEntity baseDoor)
	{
		string captured = LF_TestAnnex.HIDEOUT_ID;
		string baseKind = LF_PlacementZones.ORG_BASE_KIND;
		ctx.True(LF_Places.Find(LF_Hideout.PLACE_KIND_DOOR, BASE_SITE) == baseDoor, "the base door is bound");

		LF_Organization org = LF_OrganizationManager.Create("capture-reload-founder", "Capture Reload Crew");
		ctx.NotNull(org, "the organization is founded");
		if (!org)
			return;

		string orgId = org.GetId();

		// The save as a capture leaves it: the base first, the captured site after it.
		array<ref LF_HideoutRecord> records = {};
		records.Insert(LF_HideoutRecord.Create(BASE_SITE, orgId, true));
		records.Insert(LF_HideoutRecord.Create(captured, orgId, true));
		array<ref LF_HideoutTenancyRow> tenancy = {};
		tenancy.Insert(LF_HideoutTenancyRow.Create(captured, LF_EHideoutTenancy.CAPTURED));

		LF_Hideout.ApplyAll(records);
		LF_Hideout.ApplyTenancy(tenancy);

		ctx.True(LF_Hideout.IsCaptured(captured), "the annex site loads as captured ground");
		ctx.False(LF_Hideout.IsCaptured(BASE_SITE), "the spawned site loads as the base");
		ctx.True(LF_Places.Find(baseKind, orgId) == baseDoor, "the base door holds the organization's base place after the load");

		// Idempotent: a second apply leaves it where it is.
		LF_Hideout.ApplyTenancy(tenancy);
		ctx.True(LF_Places.Find(baseKind, orgId) == baseDoor, "a second tenancy apply keeps the base place on the base door");

		LF_Hideout.Release(captured);
		LF_Hideout.Release(BASE_SITE);
		ctx.True(LF_Places.Find(baseKind, orgId) == null, "releasing both sites drops the base place");
	}
}
