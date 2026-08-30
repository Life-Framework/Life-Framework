[ComponentEditorProps(category: "EveronLife/DebugWorld", description: "Repairs traceability and physics for DebugWorld mesh entities")]
class EL_DebugWorldCollisionRepairComponentClass : ScriptComponentClass
{
}

class EL_DebugWorldCollisionRepairComponent : ScriptComponent
{
	protected float m_fElapsed;
	protected int m_iPasses;

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnFrame(IEntity owner, float timeSlice)
	{
		m_fElapsed += timeSlice;
		if (m_fElapsed < 0.25)
			return;

		m_fElapsed = 0;
		RepairWorld();
		m_iPasses++;
		if (m_iPasses >= 40)
			ClearEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	protected void RepairWorld()
	{
		World world = GetGame().GetWorld();
		if (!world)
		{
			EL_Debug.Error("DebugWorld", "collision repair skipped: world is unavailable");
			return;
		}

		#ifdef WORKBENCH
		GenericWorldEntity worldEntity = GetGame().GetWorldEntity();
		if (worldEntity)
		{
			ScanEntityTree(worldEntity.GetChildren());
			return;
		}
		#endif

		vector center = "256 0 256";
		world.QueryEntitiesBySphere(center, 500, RepairEntity, null, EQueryEntitiesFlags.ALL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScanEntityTree(IEntity entity)
	{
		while (entity)
		{
			RepairEntity(entity);
			ScanEntityTree(entity.GetChildren());
			entity = entity.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool RepairEntity(IEntity entity)
	{
		if (!entity)
			return true;

		typename type = entity.Type();
		if (type.IsInherited(GenericTerrainEntity) || type.IsInherited(GenericWorldEntity) || type.IsInherited(SCR_ChimeraCharacter))
			return true;

		if (!(entity.GetFlags() & EntityFlags.TRACEABLE))
		{
			entity.SetFlags(EntityFlags.TRACEABLE, true);
			EL_Debug.Warn("DebugWorld", "collision repair enabled traceability for " + GetEntityLabel(entity));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetEntityLabel(IEntity entity)
	{
		string name = entity.GetName();
		if (!name.IsEmpty())
			return name;

		return entity.Type().ToString() + " at " + entity.GetOrigin().ToString();
	}
}
