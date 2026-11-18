// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_BuildSiteKeyHolder` printed RED PROOF OK: with the hook on the key stays unbound, so `- the record counts the key holder's two cement bags (expected 2, got 0)`. On the owner-only #2650 code a key holder is refused the same way.
//! property/build-site-keyholder (#2815): building is cooperative. At plot-a's sign a stranger (no key)
//! is refused with a reason and nothing leaves their inventory; a character carrying a key bound to the
//! plot deposits and works like the owner and the stage advances; once the key is bound elsewhere the
//! same character is refused again (the server re-reads the key on every call); the owner needs no key.
//! A plot sold mid-build passes its stage and deposits to the new owner: the old owner is refused, the
//! new one may continue, and nothing is refunded.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuildSiteKeyHolder : LF_Test
{
	protected static const ResourceName KEY_PREFAB = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string OWNER = "lf-test-build-owner";
	protected static const string FRIEND = "lf-test-build-friend";
	protected static const string STRANGER = "lf-test-build-stranger";
	protected static const string NEW_OWNER = "lf-test-build-new-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "property/build-site-keyholder";
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the character `count` of the item a few at a time and deposits each batch as `actorKey`.
	protected bool DepositAs(LF_TestContext ctx, LF_BuildSiteComponent site, IEntity character, string actorKey, ResourceName prefab, int count)
	{
		while (count > 0)
		{
			int batch = LF_Utils.MinInt(count, 2);
			if (LF_InventoryUtils.AddAmount(character, prefab, batch) != batch)
			{
				ctx.Fail("the character could not pick up " + prefab);
				return false;
			}

			if (site.ServerDeposit(character, actorKey) != LF_EBuildSite.OK)
			{
				ctx.Fail("deposit of " + prefab + " as " + actorKey + " refused");
				return false;
			}

			count -= batch;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		IEntity character;
		IEntity sign;
		LF_BuildSiteComponent site = LF_Test_BuildSite.SetUp(ctx, character, sign);
		string plot = "plot-a";
		string tier = LF_PropertyConfig.PlotTierId(plot);
		LF_PropertyStageDef def = LF_BuildStageRules.Stage(tier, 2);
		LF_PropertyStageReqDef cement = LF_Test_BuildSite.Requirement(def, "Cement");
		ctx.NotNull(def, "the example plot's tier has a stage 2");
		if (!site || !def || !cement || cement.m_iCount < 2)
		{
			Cleanup(character, sign, before);
			return;
		}

		LF_PropertyRecord record = LF_Properties.Get(plot);

		// 1. A stranger with no key: refused, and what they carry stays with them.
		ctx.Equal(2, LF_InventoryUtils.AddAmount(character, cement.m_Prefab, 2), "the character carries two cement bags");
		ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerDeposit(character, STRANGER), "a stranger with no key may not deposit");
		ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerWork(character, STRANGER, 100), "a stranger with no key may not work");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(character, cement.m_Prefab), "a refused deposit takes nothing");
		ctx.EqualStr("", record.m_sDeposited, "the record is untouched");
		ctx.EqualStr("#LF-Property_BuildNotYours", LF_BuildSiteAction.RefusalKey(LF_EBuildSite.REFUSED_NOT_ALLOWED), "the refusal has words");

		// 2. The same character with a key bound to the plot may deposit.
		ctx.Equal(1, LF_InventoryUtils.AddAmount(character, KEY_PREFAB, 1), "the character is handed a house key");
		array<IEntity> keys = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(character), KEY_PREFAB);
		ctx.Equal(1, keys.Count(), "the key is in the inventory");
		if (keys.IsEmpty())
		{
			Cleanup(character, sign, before);
			return;
		}

		LF_KeyComponent key = LF_Component<LF_KeyComponent>.Find(keys[0]);
		ctx.NotNull(key, "the key item carries LF_KeyComponent");
		if (!key)
		{
			Cleanup(character, sign, before);
			return;
		}

		ctx.False(LF_BuildSiteComponent.UserHoldsKey(character, plot), "an unbound key is no key to the plot");
		ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerDeposit(character, FRIEND), "an unbound key opens nothing");
		if (!RedProof())
			key.SetHouseIdentifier(plot);

		ctx.True(LF_BuildSiteComponent.UserHoldsKey(character, plot), "a key bound to the plot is a key to it");
		ctx.Equal(LF_EBuildSite.OK, site.ServerDeposit(character, FRIEND), "a key holder deposits");
		ctx.Equal(2, LF_BuildStageRules.CountOf(record.m_sDeposited, cement.m_Prefab), "the record counts the key holder's two cement bags");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(character, cement.m_Prefab), "the cement left the key holder's inventory");

		// 3. The key holder finishes the stage: the rest of the items, then the work.
		foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
		{
			int missing = LF_BuildStageRules.Missing(def, record.m_sDeposited, req.m_Prefab);
			if (missing > 0 && !DepositAs(ctx, site, character, FRIEND, req.m_Prefab, missing))
			{
				Cleanup(character, sign, before);
				return;
			}
		}

		ctx.True(LF_BuildStageRules.ItemsMet(def, record.m_sDeposited), "every item of stage 2 is in");
		float now = 100;
		for (int unit = 1; unit <= def.m_iWorkUnits; unit++)
		{
			now += 5;
			ctx.Equal(LF_EBuildSite.OK, site.ServerWork(character, FRIEND, now), string.Format("the key holder's work unit %1 lands", unit));
		}

		ctx.Equal(2, record.m_iBuildStage, "the key holder's work advanced the stage");

		// 4. The key is read again on every call: bound elsewhere, the same character is refused.
		LF_PropertyStageDef next = LF_BuildStageRules.Stage(tier, 3);
		ctx.NotNull(next, "the example plot's tier has a stage 3");
		key.SetHouseIdentifier("some-other-plot");
		ctx.False(LF_BuildSiteComponent.UserHoldsKey(character, plot), "a key bound elsewhere is no key to the plot");
		ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerDeposit(character, FRIEND), "a key that stopped fitting is refused at once");
		ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerWork(character, FRIEND, now + 10), "and so is its work");

		// 5. The owner needs no key: past the permission check (the stage still wants items).
		ctx.Equal(LF_EBuildSite.REFUSED_ITEMS_FIRST, site.ServerWork(character, OWNER, now + 20), "the owner works without a key (waits for the items)");

		// 6. A plot sold mid-build passes its stage and deposits on; no refund, the old owner is out.
		if (next && next.m_aRequirements && !next.m_aRequirements.IsEmpty())
		{
			LF_PropertyStageReqDef first = next.m_aRequirements[0];
			ctx.True(DepositAs(ctx, site, character, OWNER, first.m_Prefab, 1), "the owner deposits one item of stage 3");
			string deposited = record.m_sDeposited;
			ctx.True(LF_Properties.SetOwner(plot, NEW_OWNER), "the plot is sold");
			ctx.Equal(2, record.m_iBuildStage, "the new owner inherits the finished stages");
			ctx.EqualStr(deposited, record.m_sDeposited, "the new owner inherits the deposits");
			ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerDeposit(character, OWNER), "the old owner is refused");
			ctx.Equal(LF_EBuildSite.REFUSED_ITEMS_FIRST, site.ServerWork(character, NEW_OWNER, now + 30), "the new owner may continue");
		}

		Cleanup(character, sign, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, IEntity sign, array<ref LF_PropertyRecord> before)
	{
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(character);
		LF_Properties.ApplyAll(before);
	}
}
