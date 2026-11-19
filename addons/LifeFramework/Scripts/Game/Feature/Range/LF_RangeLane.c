//------------------------------------------------------------------------------------------------
//! A range target's lane (#3184): the lane id string that groups the targets one round raises and
//! counts. A lane is every target carrying the same id; no coordinates, no entity names. The
//! targets register here so the round can find them without a world query.
[ComponentEditorProps(category: "LifeFramework/Feature/Range", description: "Lane id of a range target: the targets sharing an id are one lane.")]
class LF_RangeLaneComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_RangeLaneComponent : ScriptComponent
{
	[Attribute("range", UIWidgets.EditBox, "Targets with the same lane id are one lane: a round raises and counts all of them")]
	protected string m_sLaneId;

	protected static ref array<EntityID> s_aTargets;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!s_aTargets)
			s_aTargets = new array<EntityID>();

		if (s_aTargets.Find(owner.GetID()) == -1)
			s_aTargets.Insert(owner.GetID());
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aTargets)
		{
			int index = s_aTargets.Find(owner.GetID());
			if (index != -1)
				s_aTargets.Remove(index);
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetLaneId()
	{
		return m_sLaneId;
	}

	//------------------------------------------------------------------------------------------------
	void SetLaneId(string laneId)
	{
		m_sLaneId = laneId;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills `outTargets` with the live targets of this lane.
	static void TargetsOf(string laneId, notnull array<SCR_FiringRangeTarget> outTargets)
	{
		outTargets.Clear();
		if (!s_aTargets || !GetGame() || !GetGame().GetWorld())
			return;

		foreach (EntityID id : s_aTargets)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (!entity)
				continue;

			SCR_FiringRangeTarget target = SCR_FiringRangeTarget.Cast(entity);
			LF_RangeLaneComponent lane = LF_RangeLaneComponent.Cast(entity.FindComponent(LF_RangeLaneComponent));
			if (target && lane && lane.GetLaneId() == laneId)
				outTargets.Insert(target);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aTargets = null;
	}
}

//------------------------------------------------------------------------------------------------
//! "Start a round" on a lane target. The server starts the round for the user; the rules (one
//! round per lane, 60 s, the starter's hits only) are in LF_RangeRun. A refusal is a toast.
class LF_RangeStartAction : ScriptedUserAction
{
	protected LF_RangeLaneComponent m_Lane;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Lane = LF_RangeLaneComponent.Cast(pOwnerEntity.FindComponent(LF_RangeLaneComponent));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Lane != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return m_Lane != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Range_StartRound";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!m_Lane)
		{
			LF_Debug.Error(LF_RangeRun.FEATURE, "start action on an entity with no LF_RangeLaneComponent");
			return;
		}

		LF_ERangeStart result = LF_RangeRun.Start(pUserEntity, m_Lane.GetLaneId());
		string key = RefusalKey(result);
		if (key.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		LF_RangeRun.Toast(playerId, LF_Terms.Text(key));
	}

	//------------------------------------------------------------------------------------------------
	protected static string RefusalKey(LF_ERangeStart result)
	{
		switch (result)
		{
			case LF_ERangeStart.RUNNING:
				return "#LF-Range_Busy";
			case LF_ERangeStart.NO_TARGETS:
				return "#LF-Range_NoTargets";
			case LF_ERangeStart.NO_SHOOTER:
				return "#LF-Range_NoShooter";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Tells the range which player hit which target. The vanilla target folds itself in OnDamage; the
//! shooter is only known here (the instigator), so the hook compares the state around the call.
modded class SCR_FiringRangeTarget
{
	//------------------------------------------------------------------------------------------------
	override void OnDamage(float damage, EDamageType type, IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, int colliderID, float speed)
	{
		int before = GetState();
		super.OnDamage(damage, type, pHitEntity, outMat, damageSource, instigator, colliderID, speed);

		if (before != ETargetState.TARGET_UP || GetState() != ETargetState.TARGET_DOWN)
			return;

		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld() || !GetGame().GetPlayerManager())
			return;

		int playerId = instigator.GetInstigatorPlayerID();
		if (playerId <= 0)
			return;

		string key = LF_Utils.GetPlayerUID(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (key.IsEmpty())
			return;

		LF_RangeRun.OnHit(this, key, GetGame().GetWorld().GetWorldTime());
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the lane registry.
class LF_RangeLane_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_RangeLaneComponent";
	}

	override void Apply()
	{
		LF_RangeLaneComponent.ResetStatics();
	}
}
