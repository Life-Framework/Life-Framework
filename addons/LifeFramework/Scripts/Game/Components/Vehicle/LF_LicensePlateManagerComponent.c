[EntityEditorProps(category: "EL/LicnesePlateManagerComponent", description: "This renders license plates on your vehicle.", color: "0 0 255 255")]
class LF_LicensePlateManagerComponentClass: ScriptComponentClass
{
};

class LF_LicensePlatePointInfo: PointInfo
{	
	LF_LicensePlateEntity m_Object;

	// The registration last applied to this plate. Kept here so it is readable where no text widget exists
	// (dedicated server, tests); the widget, when there is one, shows the same text.
	protected string m_sAppliedText;

	string GetAppliedText()
	{
		return m_sAppliedText;
	}

	void SetAppliedText(string text)
	{
		m_sAppliedText = text;
	}
	
	void ~LF_LicensePlatePointInfo()
	{
		if (m_Object)
		{
			delete m_Object;
		}
	}
}

class LF_LicensePlateManagerComponent: ScriptComponent
{
	[Attribute(uiwidget: UIWidgets.Auto)]
	ref array<ref LF_LicensePlatePointInfo> m_Plates;
	
	[Attribute("{E95486C43308F36B}Prefabs/Vehicles/LicensePlate/LicensePlate.et")]
	protected ResourceName m_LicensePlatePrefab;
	
	[RplProp(onRplName: "OnRegistrationUpdated")]
	string m_Registration;

	//! The plate this manager's EOnInit issued; empty once released. Server only, never replicated.
	protected string m_sIssuedPlate;
	
	override void EOnInit(IEntity owner)
	{
		Resource resource = Resource.Load(m_LicensePlatePrefab);
		
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Warn("LicensePlate", "plate prefab does not resolve: " + m_LicensePlatePrefab);
			return;
		}
		
		EntitySpawnParams params();
		params.Parent = owner;
		params.TransformMode = ETransformMode.LOCAL;
		
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		
		// An entity with no RplComponent is local and its own authority (the test fixture).
		if (GetGame().InPlayMode() && (!rpl || rpl.IsMaster()))
		{
			Resource container = BaseContainerTools.LoadContainer("{B1DD7B5D4812AB19}Configs/Vehicles/VehicleSettings.conf");
			if (!container || !container.GetResource())
			{
				LF_Debug.Warn("LicensePlate", "vehicle settings config does not resolve");
				return;
			}
			LF_VehicleSettings vehicleSettings = LF_VehicleSettings.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
			if (!vehicleSettings || !vehicleSettings.m_LicensePlateGenerator)
			{
				LF_Debug.Warn("LicensePlate", "vehicle settings carries no plate generator");
				return;
			}
			string vehicleId = "";
			LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(owner);
			if (vehicleLock)
				vehicleId = vehicleLock.GetVehicleIdentifier();

			m_Registration = LF_LicensePlateRegistry.Issue(vehicleSettings.m_LicensePlateGenerator, vehicleId);
			if (m_Registration.IsEmpty())
			{
				LF_Debug.Error("LicensePlate", "registration skipped: no unique plate");
				return;
			}
			m_sIssuedPlate = m_Registration;
		}
		
		if (!m_Plates)
		{
			LF_Debug.Warn("LicensePlate", "manager has no plate points configured");
			return;
		}

		for (int i = 0; i < m_Plates.Count(); i++)
		{
			auto plate = m_Plates[i];
			GetPositionFromPoint(i, params.Transform);

			plate.m_Object = LF_LicensePlateEntity.Cast(GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld(), params));
			if (!plate.m_Object)
			{
				LF_Debug.Warn("LicensePlate", "plate entity failed to spawn");
				continue;
			}
			plate.m_Object.m_LicensePlateManager = this;
		}

		// A late joiner or a streamed-in vehicle can receive the registration (RplLoad) before the plates exist.
		if (!m_Registration.IsEmpty())
			ApplyRegistration();
	}

	//------------------------------------------------------------------------------------------------
	// JIP and stream-in: the client gets the registration as part of the vehicle's initial state.
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteString(m_Registration);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		string registration;
		if (!reader.ReadString(registration))
			return false;

		m_Registration = registration;
		if (!m_Registration.IsEmpty())
			ApplyRegistration();
		return true;
	}
	
	void SetRegistration(string plate)
	{
		if (m_Registration == plate)
			return;

		// The spawn's plate is replaced by the owner's: free it, once, and only if this manager issued it.
		if (!m_sIssuedPlate.IsEmpty())
		{
			if (m_sIssuedPlate != plate)
			{
				LF_LicensePlateRegistry.Release(m_sIssuedPlate);
				LF_Debug.Log("LicensePlate", string.Format("released spawn plate %1, now %2", m_sIssuedPlate, plate));
			}
			m_sIssuedPlate = "";
		}

		m_Registration = plate;
		Replication.BumpMe();
		OnRegistrationUpdated();
	}

	//------------------------------------------------------------------------------------------------
	// RplProp callback: runs on clients when the server changes the registration.
	void OnRegistrationUpdated()
	{
		ApplyRegistration();
	}

	//------------------------------------------------------------------------------------------------
	// Puts the current registration on every plate this manager spawned. Plates that have not spawned yet
	// get it at the end of EOnInit.
	protected void ApplyRegistration()
	{
		if (!m_Plates)
			return;

		int applied;
		foreach (LF_LicensePlatePointInfo plate : m_Plates)
		{
			if (!plate || !plate.m_Object)
				continue;

			// RplLoad and the onRpl handler can both deliver the same value: apply and log a change once.
			if (plate.GetAppliedText() == m_Registration)
				continue;

			plate.SetAppliedText(m_Registration);
			applied++;
			if (plate.m_Object.m_TextWidget)
				plate.m_Object.m_TextWidget.SetText(m_Registration);
		}

		if (applied == 0)
			return;

		string side = "client";
		if (Replication.IsServer())
			side = "server";
		// The entity name is empty on clients; the plate itself is unique (LF_LicensePlateRegistry).
		LF_Debug.Info("LicensePlate", string.Format("%1 applied plate %2 on %3 (%4 plates)", side, m_Registration, GetOwner().GetName(), applied));
	}
	
	override void OnPostInit(IEntity owner)
	{		
		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, false);
	}
	
	bool GetPositionFromPoint(int index, out vector transform[4])
	{
		Math3D.MatrixIdentity4(transform);
		BaseContainer source;
		GenericEntity entity = GetOwner();
		BaseContainerList list;
		
		source = GetComponentSource(entity);
		if (!source) return false;
		
		list = source.GetObjectArray("m_Plates");
		if (!list) return false;

		source = list.Get(index);
		if (!source) return false;

		vector position;
		source.Get("Offset", position);
		
		vector rotation;
		source.Get("Angles", rotation);

		transform[3] = position;
		
		Math3D.AnglesToMatrix(rotation, transform);

		return true;
	}
};
