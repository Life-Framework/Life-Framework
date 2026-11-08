// LF_SignReadAction.c — "Read: <title>" on a sign prop. Opens the paper card on
// the reading client only. Content is static config, so there is no RPC.

class LF_SignReadAction : ScriptedUserAction
{
	protected LF_SignComponent m_SignComponent;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_SignComponent = EL_Component<LF_SignComponent>.Find(pOwnerEntity);
		if (!m_SignComponent)
			EL_Debug.Error("Signs", "Read action on an entity with no LF_SignComponent; Read hidden");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_SignDef GetSign()
	{
		if (!m_SignComponent)
			return null;

		return m_SignComponent.GetSign();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return GetSign() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_SignDef sign = GetSign();
		if (!sign)
			return false;

		outName = LF_SignCatalog.ReadName(sign);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || pUserEntity != EL_Utils.GetLocalCharacter())
			return;

		LF_SignDef sign = GetSign();
		if (!sign)
		{
			EL_Debug.Error("Signs", "Read performed on a sign with no entry; card not opened");
			return;
		}

		LF_SignCard.Open(sign, pOwnerEntity.GetID());
	}
}
