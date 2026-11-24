// red-proof: `tools\cli test --no-build --red LF_Test_HoursPlateFace` (2026-11-24, #4834): the hook leaves the live hours rows off the face plan, so "the plate's face carries the live hours row" and every check after it fail.

//! signs/hours-plate-face (#4834): the opening-hours plate prints on a styled face, not the legacy paint face (which
//! prints the title only). The `enamel` style and `plate` size resolve from Configs/Signs/LF_SignStyles_Plate.conf, the
//! style's layout and row layout load, `LF_Sign_HoursPlate` names that style and size and the `hours_plate` row source,
//! the labour and harbour plates (`LF_Sign_PlateSmall`) keep the legacy face, and the plan the face is painted from
//! carries the live row, "OPEN" and the keeper's hours, inside the shown area at a readable size, for a one-span shift
//! and a split shift alike.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HoursPlateFace : LF_Test
{
	static const ResourceName HOURS_PLATE_PREFAB = "{379F4E8692F36269}Prefabs/Props/Signs/LF_Sign_HoursPlate.et";
	static const ResourceName PLATE_SMALL_PREFAB = "{9182BD264965878C}Prefabs/Props/Signs/LF_Sign_PlateSmall.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "signs/hours-plate-face";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Loads(ResourceName name)
	{
		if (name.IsEmpty())
			return false;

		Resource resource = Resource.Load(name);
		return resource && resource.IsValid();
	}

	//------------------------------------------------------------------------------------------------
	//! The plan the face would be painted from, for a keeper's day: the sign's live rows, resolved, composed on the size.
	protected LF_SignFacePlan PlanFor(LF_ResidentTimetable day, LF_SignStyleDef style, LF_SignSizeDef size)
	{
		LF_SignDef sign = new LF_SignDef();
		sign.m_sTitle = "Opening Hours";
		sign.m_aRows = {};
		if (!RedProof())
			LF_HoursPlate.BuildRows(day, sign.m_aRows);

		array<string> labels = {};
		array<string> values = {};
		LF_SignStyledFace.ResolveRows(sign, labels, values);
		return LF_SignFacePlan.Compose(size.m_vShown, style, sign.m_sTitle, labels, values);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPlan(LF_TestContext ctx, string where, LF_SignFacePlan plan, LF_SignSizeDef size, string hours)
	{
		ctx.EqualStr("OPENING HOURS", plan.m_sTitle, where + ": the title is in capitals");
		ctx.True(plan.m_aRows.Count() == 1, where + ": the face carries the live hours row");
		if (plan.m_aRows.Count() != 1)
			return;

		LF_SignFaceRowPlan row = plan.m_aRows[0];
		ctx.EqualStr("OPEN", row.m_sLabel, where + ": the row label is OPEN");
		ctx.EqualStr(hours, row.m_sValue, where + ": the row value is the keeper's hours");
		ctx.True(row.m_iFont >= LF_SignFacePlan.ROW_FONT_MIN, where + ": the hours read at a legible size");
		ctx.True(row.m_vPos[0] >= 0 && row.m_vPos[1] >= 0, where + ": the row starts on the plate");
		ctx.True(row.m_vPos[0] + row.m_vSize[0] <= size.m_vShown[0] + 0.5, where + ": the row is inside the shown width");
		ctx.True(row.m_vPos[1] + row.m_vSize[1] <= size.m_vShown[1] + 0.5, where + ": the row is inside the shown height");
		ctx.True(plan.m_vTitlePos[1] + plan.m_vTitleSize[1] * 0.5 <= row.m_vPos[1], where + ": the title line sits above the row");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResidentTimetable.ResetStatics();

		LF_SignStyleConfig config = LF_SignStyles.LoadWithFamilies();
		ctx.NotNull(config, "the sign style config loads with its family files");
		if (!config)
			return;

		LF_SignStyleDef enamel = LF_SignStyles.FindStyleIn(config, "enamel");
		ctx.NotNull(enamel, "the enamel style resolves from LF_SignStyles_Plate.conf");
		LF_SignSizeDef size = LF_SignStyles.FindSizeIn(config, "plate");
		ctx.NotNull(size, "the plate size resolves from LF_SignStyles_Plate.conf");
		if (!enamel || !size)
			return;

		ctx.True(Loads(enamel.m_sLayout), "the enamel face layout resolves");
		ctx.True(Loads(enamel.m_sRowLayout), "the enamel row layout resolves");
		ctx.True(enamel.m_bPlacementColors, "an enamel plate takes the placed plate's paint and lettering colours");
		ctx.True(enamel.m_bUpperTitle, "enamel lettering is capitals");
		ctx.True(size.m_sCarrier.IsEmpty(), "the plate's vanilla body is the face: no carrier");
		ctx.EqualFloat(1024, size.m_vShown[0], 0.01, "the plate shows 1024 units across");
		ctx.EqualFloat(512, size.m_vShown[1], 0.01, "and 512 down, the 90 x 45 body's shape");

		// The prefab: styled, sized, reading its hours from the keeper; the other plates keep the legacy face.
		ExpectPlate(ctx, HOURS_PLATE_PREFAB, "enamel", "plate");
		ExpectPlate(ctx, PLATE_SMALL_PREFAB, "", "");

		// A one-span and a split shift: the plan holds the live row.
		LF_ResidentDef baker = LF_ResidentDef.Make("t_baker", "t_home", "t_shop");
		baker.Add("00:00", "home", "bench").Add("06:00", "work", "counter", "serve").Add("14:00", "home", "bench");
		LF_SignFacePlan plan = PlanFor(LF_ResidentTimetable.Build(baker, new LF_Test_AnyKinds()), enamel, size);
		CheckPlan(ctx, "baker", plan, size, "6 AM - 2 PM");

		LF_ResidentDef barber = LF_ResidentDef.Make("t_barber", "t_home", "t_shop");
		barber.Add("00:00", "home", "bench").Add("09:00", "work", "counter", "serve").Add("12:30", "home", "bench").Add("13:30", "work", "counter", "serve").Add("18:00", "home", "bench");
		plan = PlanFor(LF_ResidentTimetable.Build(barber, new LF_Test_AnyKinds()), enamel, size);
		CheckPlan(ctx, "barber", plan, size, "9 AM - 12:30 PM, 1:30 PM - 6 PM");

		LF_ResidentTimetable.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectPlate(LF_TestContext ctx, ResourceName prefab, string style, string size)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), prefab.GetPath() + " loads");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity prop = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(prop, prefab.GetPath() + " spawns");
		if (!prop)
			return;

		LF_SignComponent sign = LF_Component<LF_SignComponent>.Find(prop);
		ctx.NotNull(sign, prefab.GetPath() + " carries LF_SignComponent");
		if (sign)
		{
			ctx.EqualStr(style, sign.GetStyleId(), prefab.GetPath() + ": style id");
			ctx.EqualStr(size, sign.GetSizeId(), prefab.GetPath() + ": size id");
			ctx.True(sign.HasPaintedFace(), prefab.GetPath() + " keeps a painted face, so placement colours apply");
			if (style == "enamel")
			{
				ctx.EqualStr("hours_plate", sign.GetSignId(), "the hours plate names the hours_plate sign");
				ctx.True(sign.HasRowSource(), "the hours plate reads live rows");
			}
		}

		SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}
}
