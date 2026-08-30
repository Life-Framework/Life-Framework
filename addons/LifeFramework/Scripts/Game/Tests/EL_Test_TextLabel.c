// red-proof: when m_sText was an RplProp-only field (not prefab-serialized)
// the label assertion failed with "expected 'MINING AREA', got ''" on
// `tools\cli test --tier all`. Making it a prefab-settable [Attribute] field
// fixed the red.

// tier: WORLD
class EL_Test_TextLabel : EL_Test
{
	protected static const ResourceName SIGN = "{D8D55097BA6DB899}Prefabs/Props/Signs/EL_MiningSign.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "signs/text-label-wiring";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		ctx.True(Resource.Load(SIGN).IsValid(), "mining sign prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity sign = GetGame().SpawnEntityPrefab(Resource.Load(SIGN), GetGame().GetWorld(), params);
		ctx.True(sign != null, "mining sign spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_TextLabelComponent label = EL_Component<EL_TextLabelComponent>.Find(sign);
		ctx.True(label != null, "mining sign carries EL_TextLabelComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr("MINING AREA", label.m_sText, "mining sign labels itself MINING AREA");
	}
}