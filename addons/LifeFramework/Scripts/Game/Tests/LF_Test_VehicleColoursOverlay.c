// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_VehicleColours,LF_Test_VehicleColoursOverlay` (RED-OK): without the pack source the overlay adds nothing, `- add: the S105 row has two colours (expected 2, got 1)`. Green with the hook off.
// tier: WORLD
//! Epic #1826: a pack `.conf` fills the colours config by catalog prefab: a row is added, a row's
//! colour list is replaced by a patch, a row is removed, and a model with no row is unlisted.
class LF_Test_VehicleColoursOverlaySource : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{F57851C9F6EB28C4}Configs/Tests/LF_Test_VehicleColoursOverlay.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_VehicleColoursConfig.CONFIG_PATH)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "test-vehicle-colours";
	}
}

//------------------------------------------------------------------------------------------------
class LF_Test_VehicleColoursOverlay : LF_Test
{
	override string GetName()
	{
		return "vehicles/colours-overlay";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");
		Resource resource = BaseContainerTools.LoadContainer(LF_VehicleColoursConfig.CONFIG_PATH);
		ctx.True(resource && resource.IsValid() && resource.GetResource(), "the core colours config loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return;

		LF_VehicleColoursConfig core = LF_VehicleColoursConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		ctx.NotNull(core, "the core colours config has the right root class");
		if (!core)
			return;

		ctx.Equal(0, core.m_aModels.Count(), "core ships no colours: a pack fills them");

		// The overlay. The red proof leaves the pack source out, so no row arrives.
		if (!RedProof())
			LF_ConfigService.SetPackSource(new LF_Test_VehicleColoursOverlaySource());
		core.ApplyLayers();

		ctx.Equal(2, core.CountFor(LF_VehicleCatalog.S105), "add: the S105 row has two colours");
		LF_VehicleColour red = core.ColourAt(LF_VehicleCatalog.S105, 1);
		ctx.NotNull(red, "add: the S105's second colour is there");
		if (red)
		{
			ctx.EqualStr("ColourTwo", red.m_sNameKey, "add: the colour keeps its name key");
			ctx.Equal(9109504, red.m_iSwatch, "add: the colour keeps its swatch");
			ctx.Equal(50, red.m_iPriceDelta, "add: the colour keeps its price delta");
			ctx.EqualStr("{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et", red.m_SpawnPrefab, "add: the colour keeps its spawn prefab");
		}

		ctx.Equal(1, core.CountFor(LF_VehicleCatalog.UAZ469), "patch: the UAZ's three colours are replaced by the patch's one");
		ctx.Equal(1, core.CountFor(LF_VehicleCatalog.DANA2101), "remove: the V2101 row is gone, the model sells one colour");
		ctx.True(core.SetFor(LF_VehicleCatalog.DANA2101) == null, "remove: no row is left for the V2101");
		ctx.True(core.SetFor(LF_VehicleCatalog.M923) == null, "a model no layer lists has no row");

		LF_ConfigService.Reset();
	}
}
