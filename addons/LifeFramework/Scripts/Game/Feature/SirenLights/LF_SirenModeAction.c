class LF_SirenModeAction : ScriptedUserAction
{
	[Attribute(uiwidget: UIWidgets.CheckBox)]
	protected bool m_bInteriorOnly;

	//! Will action be available for entities seated in pilot compartment only?
	[Attribute(uiwidget: UIWidgets.CheckBox, defvalue: "1")]
	protected bool m_bPilotOnly;

	[Attribute(uiwidget: UIWidgets.ComboBox, enums: LF_SirenMode.MODE_NAMES)]
	protected string m_Mode;

	[Attribute(uiwidget: UIWidgets.CheckBox, desc: "Only an on-duty member of the vehicle's service is offered this, and the server enforces it (the ambulance's medics, #923). The action hides while its mode is the one running, so a pair of them reads as one toggle. Leave off for the police knob.")]
	protected bool m_bDutyOnly;

	protected LF_SirenManagerComponent m_Manager;

	protected IEntity m_pOwner;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		// The knob is a child of the vehicle; a menu action sits on the vehicle itself.
		m_pOwner = pOwnerEntity;
		while (m_pOwner && !m_Manager)
		{
			m_Manager = LF_SirenManagerComponent.Cast(m_pOwner.FindComponent(LF_SirenManagerComponent));
			if (!m_Manager)
				m_pOwner = m_pOwner.GetParent();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("SirenLights", "LF_SirenModeAction"))
			return;

		if (!m_Manager)
			return;

		if (m_bDutyOnly)
		{
			// The server decides (duty rule and throttle) and the change replicates from there.
			if (Replication.IsServer())
				m_Manager.RequestModeStr(pUserEntity, m_Mode);
			return;
		}

		m_Manager.SetModeStr(m_Mode);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("SirenLights"))
			return false;

		if (m_bDutyOnly && (!m_Manager || !m_Manager.MayOperate(user) || m_Manager.IsModeName(m_Mode)))
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		// See if character is in vehicle
		if (character && character.IsInVehicle())
		{
			// See if character is in "this" (owner) vehicle
			CompartmentAccessComponent compartmentAccess = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
			if (compartmentAccess)
			{
				// Character is in compartment
				// that belongs to owner of this action
				BaseCompartmentSlot slot = compartmentAccess.GetCompartment();
				if (slot)
				{
					// Check pilot only condition
					if (m_bPilotOnly && !PilotCompartmentSlot.Cast(slot))
						return false;

					// Check interior only condition
					if (m_bInteriorOnly && slot.GetOwner() != m_pOwner)
						return false;

					return true;
				}
			}

			return false;
		}
		// We cannot be pilot nor interior, if we are not seated in vehicle at all.
		else if (m_bInteriorOnly || m_bPilotOnly) return false;

		return true;
	}
}