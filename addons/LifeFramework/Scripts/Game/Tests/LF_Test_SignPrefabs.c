// red-proof (A-frame back face, #3547): RedProof() reports no back face, so "the A-frame prints the same face on its far slate too" fails.
// red-proof (A-frame, #3305): RedProof() blanks the A-frame's style id, so "the A-frame names the chalk style" fails.
// red-proof: expected 2 Read actions; `tools\cli test --no-build --tier all`
// (2026-11-08) failed signs/prefabs with "sign carries one Read action:
// ...LF_Sign_Board.et (expected 2, got 1)" for every sign prefab.

[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SignPrefabs : LF_Test
{
	static const ResourceName PUB_PREFAB = "{7B452ADB636331A9}Prefabs/Props/Signs/LF_Sign_Pub.et";

	override string GetName()
	{
		return "signs/prefabs";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			"{220A96D43DC3C3AE}Prefabs/Props/Signs/LF_Sign_Board.et",
			"{7B452ADB636331A9}Prefabs/Props/Signs/LF_Sign_Pub.et",
			"{70924D289627911B}Prefabs/Props/Signs/LF_Sign_Notice.et",
			"{F82F6440A45EE705}Prefabs/Props/Signs/LF_Sign_Chalkboard.et",
			"{C6940227F886B8EF}Prefabs/Props/Signs/EL_WhitelistSign.et",
			"{884DF4029C8307DB}Prefabs/Props/Signs/LF_Sign_Plate.et",
			"{9182BD264965878C}Prefabs/Props/Signs/LF_Sign_PlateSmall.et",
			"{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et",
			"{7863BDC6872929FF}Prefabs/Props/Signs/LF_Sign_Posts.et",
			"{B7B489CA36A0E9F8}Prefabs/Props/Signs/LF_Sign_PolePlate.et"
		};

		Resource paintLayout = Resource.Load("{5783648244305C88}UI/Layouts/Signs/LF_SignPaintFace.layout");
		ctx.True(paintLayout && paintLayout.IsValid(), "the painted face layout loads");
		Resource boardLayout = Resource.Load("{FE97102988E1EC48}UI/Layouts/Signs/LF_SignBoardFace.layout");
		ctx.True(boardLayout && boardLayout.IsValid(), "the cork board face layout loads");

		foreach (ResourceName path : prefabs)
		{
			ExpectOneRead(ctx, path);
		}

		ExpectChalkPub(ctx);

		Resource face = Resource.Load(LF_SignComponent.FacePrefab());
		ctx.True(face && face.IsValid(), "the board face sheet prefab loads");
		Resource faceLayout = Resource.Load(LF_SignComponent.FACE_LAYOUT);
		ctx.True(faceLayout && faceLayout.IsValid(), "the board face layout loads");

		LF_SignDef mining = LF_SignCatalog.Find("mining");
		ctx.True(LF_SignCatalog.IsUsable(mining), "LF_Signs.conf has the mining sign");
		ctx.True(LF_SignCatalog.IsUsable(LF_SignCatalog.Find("police_whitelist")), "LF_Signs.conf has the police whitelist sign");
		ctx.True(LF_SignCatalog.Find("no_such_sign") == null, "LF_Signs.conf has no unknown id");
		if (mining)
		{
			// The per-sign text is the pack's (#325); the core alone reads the neutral title.
			string expected = "Read: Notice";
			if (LF_SignCatalog.Resolve(mining.m_sTitle) != "")
				expected = "Read: Mining area";

			ctx.EqualStr(expected, LF_SignCatalog.ReadName(mining), "the mining sign reads Read: Mining area, or the neutral title without the pack");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The A-frame pavement board prints in chalk on the portrait 5:8 quad (#3305, #3547): the prefab names a style and
	//! a size, and both resolve in LF_SignStyles.conf.
	protected void ExpectChalkPub(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(PUB_PREFAB);
		ctx.True(resource && resource.IsValid(), "LF_Sign_Pub loads under its original GUID");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity pub = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(pub, "the A-frame spawns");
		if (!pub)
			return;

		LF_SignComponent sign = LF_Component<LF_SignComponent>.Find(pub);
		ctx.NotNull(sign, "the A-frame carries LF_SignComponent");
		if (sign)
		{
			string styleId = sign.GetStyleId();
			if (RedProof())
				styleId = "";

			ctx.EqualStr("chalk", styleId, "the A-frame names the chalk style");
			ctx.NotNull(LF_SignStyles.FindStyle(styleId), "the chalk style resolves");
			LF_SignSizeDef size = LF_SignStyles.FindSize(sign.GetSizeId());
			ctx.NotNull(size, "the A-frame's size resolves: " + sign.GetSizeId());
			if (size)
			{
				ctx.True(!size.m_sCarrier.IsEmpty(), "and its size draws on a carrier quad");
				// The slate is portrait (about 0.57 x 0.9 m), so a landscape quad shows the vanilla chalk around it (#3547).
				ctx.True(size.m_vShown[1] > size.m_vShown[0], "the A-frame's carrier is portrait, so it covers the slate");
			}

			bool backFace = sign.HasBackFace();
			if (RedProof())
				backFace = false;

			ctx.True(backFace, "the A-frame prints the same face on its far slate too");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(pub);
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectOneRead(LF_TestContext ctx, ResourceName path)
	{
		Resource resource = Resource.Load(path);
		ctx.True(resource && resource.IsValid(), "sign prefab loads: " + path);
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity sign = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(sign, "sign spawns: " + path);
		if (!sign)
			return;

		ctx.NotNull(LF_Component<LF_SignComponent>.Find(sign), "sign carries LF_SignComponent: " + path);

		int reads = 0;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(sign);
		if (actions)
		{
			array<BaseUserAction> actionList = {};
			actions.GetActionsList(actionList);
			foreach (BaseUserAction action : actionList)
			{
				if (LF_SignReadAction.Cast(action))
					reads++;
			}
		}

		ctx.Equal(1, reads, "sign carries one Read action: " + path);
		SCR_EntityHelper.DeleteEntityAndChildren(sign);
	}
}
