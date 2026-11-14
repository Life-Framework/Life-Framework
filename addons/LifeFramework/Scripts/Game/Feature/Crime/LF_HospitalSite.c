//------------------------------------------------------------------------------------------------
//! Placed hospital till. Registers the cash gates with LF_Hospital on the server.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Hospital till: civilian cash robbery.")]
class LF_HospitalTillComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HospitalTillComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable world target id. Empty uses the entity name.")]
	protected string m_sTargetId;

	[Attribute("200", UIWidgets.EditBox, desc: "MoneyStack cash paid on a successful rob.")]
	protected int m_iPayout;

	[Attribute("60", UIWidgets.EditBox, desc: "Cooldown after a successful rob. Zero allows another rob immediately.")]
	protected float m_fCooldown;

	[Attribute("0", UIWidgets.EditBox, desc: "Minimum police on duty. Zero matches the DebugWorld robbery points.")]
	protected int m_iMinPolice;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		string targetId = GetTargetId();
		if (!targetId.IsEmpty())
			LF_Places.Register(LF_Hospital.PLACE_KIND, targetId, owner);

		if (!Replication.IsServer())
			return;

		if (targetId.IsEmpty())
		{
			LF_Debug.Error("Hospital", "till component has no id and the entity is unnamed");
			return;
		}

		LF_Hospital.ConfigureTill(targetId, m_iPayout, m_fCooldown, m_iMinPolice);
	}

	//------------------------------------------------------------------------------------------------
	string GetTargetId()
	{
		if (!m_sTargetId.IsEmpty())
			return m_sTargetId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Placed hospital cabinet. The loot list is spawned by LF_Hospital after LF_Breach opens it.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Hospital cabinet loot list.")]
class LF_HospitalCabinetComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HospitalCabinetComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable world target id. Empty uses the entity name.")]
	protected string m_sTargetId;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Medicine prefabs spawned when the cabinet opens.", "et")]
	protected ref array<ResourceName> m_aLootPrefabs;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		string targetId = GetTargetId();
		if (!targetId.IsEmpty())
			LF_Places.Register(LF_Hospital.PLACE_KIND, targetId, owner);

		if (!Replication.IsServer())
			return;

		if (targetId.IsEmpty())
		{
			LF_Debug.Error("Hospital", "cabinet component has no id and the entity is unnamed");
			return;
		}

		LF_LockComponent cabinetLock = LF_Component<LF_LockComponent>.Find(owner);
		if (!cabinetLock)
		{
			LF_Debug.Error("Hospital", string.Format("cabinet %1 has no LF_LockComponent", targetId));
			return;
		}

		// A lock added on the layer does not run LF_LockComponent.EOnInit, so the
		// debug flag never reaches the replicated locked bit. Lock here.
		if (!cabinetLock.IsLocked())
			cabinetLock.Lock();

		array<ResourceName> loot = {};
		if (!m_aLootPrefabs || m_aLootPrefabs.Count() == 0)
			LF_Hospital.FillDefaultLoot(loot);
		else
		{
			foreach (ResourceName prefab : m_aLootPrefabs)
				loot.Insert(prefab);
		}

		LF_Hospital.ConfigureCabinet(targetId, loot);
	}

	//------------------------------------------------------------------------------------------------
	string GetTargetId()
	{
		if (!m_sTargetId.IsEmpty())
			return m_sTargetId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Cabinet action. Calls LF_Breach.Arm through LF_Hospital.BreachCabinet.
//------------------------------------------------------------------------------------------------
class LF_HospitalCabinetAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return Cabinet() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_HospitalCabinetComponent cabinet = Cabinet();
		if (!cabinet)
		{
			SetCannotPerformReason("#EL-Breach_NotConfigured");
			return false;
		}

		if (!LF_Hospital.CanBreach(cabinet.GetTargetId()))
		{
			SetCannotPerformReason("#EL-Breach_AlreadyOpen");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_HospitalCabinetComponent cabinet = LF_Component<LF_HospitalCabinetComponent>.Find(pOwnerEntity);
		if (!cabinet)
		{
			LF_Debug.Error("Hospital", "cabinet action: owner has no LF_HospitalCabinetComponent");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Hospital", "cabinet action refused: player has no account id");
			return;
		}

		LF_Hospital.BreachCabinet(cabinet.GetTargetId(), playerUid, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Hospital_BreachCabinet";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_HospitalCabinetComponent Cabinet()
	{
		if (!GetOwner())
			return null;

		return LF_Component<LF_HospitalCabinetComponent>.Find(GetOwner());
	}
}
