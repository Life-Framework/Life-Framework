//------------------------------------------------------------------------------------------------
//! The serial number stamped on a fence tool (#272). The server stamps it when the fence sells the
//! tool (LF_FenceSerials); the string replicates so whoever holds the tool can read it, and
//! LF_SerialComponentSerializer saves it with the item. Empty means the tool has no serial.
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Serial number stamped on a fence tool.")]
class LF_SerialComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_SerialComponent : ScriptComponent
{
	[RplProp()]
	protected string m_sSerial;

	//------------------------------------------------------------------------------------------------
	string GetSerial()
	{
		return m_sSerial;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stamps the serial. A tool already stamped keeps its serial.
	//! \return True when the serial was set.
	bool SetSerial(string serial)
	{
		if (!Replication.IsServer() || serial.IsEmpty() || !m_sSerial.IsEmpty())
			return false;

		m_sSerial = serial;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a load restores the serial even over an existing one.
	void RestoreSerial(string serial)
	{
		m_sSerial = serial;
		Replication.BumpMe();
	}
}

//------------------------------------------------------------------------------------------------
//! Persists the serial with the item. Bound in Configs/Systems/Persistence/LifeFramework.conf
//! beside LF_QuantityComponentSerializer. Version first, then the serial only when stamped.
class LF_SerialComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return LF_SerialComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		LF_SerialComponent serial = LF_SerialComponent.Cast(component);
		if (!serial)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);
		if (serial.GetSerial().IsEmpty())
			return ESerializeResult.OK;

		string value = serial.GetSerial();
		context.Write(value);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		LF_SerialComponent serial = LF_SerialComponent.Cast(component);
		if (!serial)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		string value;
		if (!context.Read(value) || value.IsEmpty())
			return true;

		serial.RestoreSerial(value);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Read the serial off a tool you hold or one on the ground.
class LF_ReadSerialAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_SerialComponent serial = LF_SerialComponent.Cast(GetOwner().FindComponent(LF_SerialComponent));
		return serial && !serial.GetSerial().IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		LF_SerialComponent serial = LF_SerialComponent.Cast(pOwnerEntity.FindComponent(LF_SerialComponent));
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!serial || !manager)
			return;

		manager.ShowNotificationToast("#LF-Serial_Title", "#LF-Serial_Read", 6.0, LF_ENotificationType.INFO, serial.GetSerial());
	}
}
