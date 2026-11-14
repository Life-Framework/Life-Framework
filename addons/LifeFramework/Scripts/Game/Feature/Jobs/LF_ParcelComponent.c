//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Jobs", description: "Courier parcel: its description names the drop.")]
class LF_ParcelComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ParcelComponent : ScriptComponent
{
	static const string MARKER_TITLE = "#LF-Parcel_DroppedTitle";
	static const string MARKER_TEXT = "#LF-Parcel_DroppedNearby";
	static const float REST_CLEARANCE = 0.02;

	protected LightEntity m_Marker;

	//! The collision layer mask of an entity's physics body, -1 without a body.
	static int LayerOf(IEntity e)
	{
		Physics p = e.GetPhysics();
		if (!p)
			return -1;

		return p.GetInteractionLayer();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a character (a player or a body) is downed, unconscious or dead: the only
	//! involuntary reason a carried parcel leaves the hands.
	static bool CarrierIsDown(IEntity character)
	{
		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(character);
		if (!controller)
			return true;

		return controller.GetLifeState() != ECharacterLifeState.ALIVE;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the parcel went into a slot (marker off) or onto the ground (marker on).
	//! The server also settles it on the ground and tells the one who dropped it.
	void OnSlotChanged(InventoryStorageSlot oldSlot, InventoryStorageSlot newSlot)
	{
		IEntity owner = GetOwner();
		if (newSlot)
		{
			RemoveMarker();
			return;
		}

		AddMarker(owner);
		if (!Replication.IsServer())
			return;

		int playerId = 0;
		if (oldSlot && oldSlot.GetStorage() && GetGame().GetPlayerManager())
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(oldSlot.GetStorage().GetOwner());

		LF_Debug.Info("Jobs", string.Format("parcel dropped on the ground (player %1)", playerId));
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, MARKER_TITLE, MARKER_TEXT, 6.0);

		GetGame().GetCallqueue().CallLater(Settle, 1500, false, owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a parcel that came to rest below the ground, or that fell through it, is put back
	//! on the surface the placement probe finds under it.
	protected void Settle(IEntity parcel)
	{
		if (!parcel || LF_ParcelComponent.InSlot(parcel))
			return;

		RestOnGround(parcel);
	}

	//------------------------------------------------------------------------------------------------
	//! Lifts a parcel that sank below the ground the placement probe finds under it. True if it moved.
	static bool RestOnGround(IEntity parcel)
	{
		vector pos = parcel.GetOrigin();
		float ground = LF_OreNodeComponent.FindGroundY(pos, parcel);
		if (pos[1] >= ground - 0.05)
			return false;

		LF_Debug.Warn("Jobs", string.Format("parcel sank %1 m below the ground; lifted onto it", ground - pos[1]));
		pos[1] = ground + REST_CLEARANCE;
		parcel.SetOrigin(pos);
		Physics physics = parcel.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetActive(ActiveState.ACTIVE);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool InSlot(IEntity parcel)
	{
		InventoryItemComponent item = LF_Component<InventoryItemComponent>.Find(parcel);
		return item && item.GetParentSlot() != null;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddMarker(IEntity owner)
	{
		if (m_Marker || !GetGame().GetWorld() || System.IsConsoleApp())
			return;

		m_Marker = LightEntity.CreateLight(LightType.POINT, 0, 2.2, Color.FromVector("1 0.7 0.25"), 3, "0 0.25 0");
		if (!m_Marker)
		{
			LF_Debug.Error("Jobs", "parcel marker light was not created");
			return;
		}

		m_Marker.SetLensFlareType(LightLensFlareType.Disabled);
		owner.AddChild(m_Marker, -1);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveMarker()
	{
		if (!m_Marker)
			return;

		delete m_Marker;
		m_Marker = null;
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_ParcelComponent()
	{
		RemoveMarker();
	}

	//------------------------------------------------------------------------------------------------
	//! The description line the parcel shows: where it goes.
	static string DescriptionText()
	{
		return WidgetManager.Translate("#LF-Item_Parcel_Deliver", WidgetManager.Translate("#LF-Job_ParcelDest"));
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		InventoryItemComponent item = LF_Component<InventoryItemComponent>.Find(owner);
		if (!item || !item.GetAttributes())
		{
			LF_Debug.Error("Jobs", "parcel has no item attributes; description unchanged");
			return;
		}

		UIInfo info = item.GetAttributes().GetUIInfo();
		if (!info)
		{
			LF_Debug.Error("Jobs", "parcel has no UI info; description unchanged");
			return;
		}

		info.SetDescription(DescriptionText());
	}
}
