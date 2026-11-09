[EntityEditorProps(category: "EL/LicnesePlateManagerComponent", description: "This renders license plates on your vehicle.", color: "0 0 255 255")]
class LF_LicensePlateManagerComponentClass: ScriptComponentClass
{
};

class LF_LicensePlatePointInfo: PointInfo
{	
	LF_LicensePlateEntity m_Object;
	
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
		
		if (GetGame().InPlayMode() && rpl && rpl.IsMaster())
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
	}
	
	void SetRegistration(string plate)
	{
		if (m_Registration == plate)
			return;

		m_Registration = plate;
		Replication.BumpMe();
		OnRegistrationUpdated();
	}

	//------------------------------------------------------------------------------------------------
	void OnRegistrationUpdated()
	{
		foreach (auto plate : m_Plates)
		{
			if (!plate.m_Object || !plate.m_Object.m_TextWidget)
			{
				LF_Debug.Warn("LicensePlate", "plate entity or text widget missing, cannot render registration");
				continue;
			}
			plate.m_Object.m_TextWidget.SetText(m_Registration);
		}
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
