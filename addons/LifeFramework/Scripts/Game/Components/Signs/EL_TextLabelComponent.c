[ComponentEditorProps(category: "EveronLife/Signs", description: "Renders text in the world at the owner position")]
class EL_TextLabelComponentClass : ScriptComponentClass
{
}

class EL_TextLabelComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Text rendered in the world at this entity")]
	string m_sText;

	[Attribute("0.35", UIWidgets.EditBox, "Text height in meters")]
	float m_fTextSize;

	[Attribute("1 0.85 0.2", UIWidgets.ColorPicker, "Text color")]
	vector m_vTextColor;

	[Attribute("1", UIWidgets.Slider, "Text opacity", "0 1 0.05")]
	float m_fOpacity;

	[Attribute("0 2.25 0.45", UIWidgets.EditBox, "Offset from the entity origin where the text is drawn")]
	vector m_vOffset;

	[Attribute("0.6", UIWidgets.Slider, "Dark background opacity behind the text. 0 disables it", "0 1 0.05")]
	float m_fBackgroundOpacity;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		if (RplSession.Mode() != RplMode.Dedicated)
			SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
		else
			SetEventMask(owner, EntityEvent.INIT);

		owner.SetFlags(EntityFlags.ACTIVE, true);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		EL_Debug.Info("TextSign", "label ready: " + m_sText);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_sText == "")
			return;

		BaseWorld world = owner.GetWorld();
		if (!world)
			return;

		GenericEntity ge = GenericEntity.Cast(owner);
		if (!ge)
		{
			EL_Debug.Error("TextSign", "owner is not a GenericEntity, cannot render label: " + m_sText);
			return;
		}

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = ge.GetOrigin() + m_vOffset;

		DebugTextFlags flags = DebugTextFlags.CENTER | DebugTextFlags.ONCE | DebugTextFlags.FACE_CAMERA;
		DebugTextWorldSpace.CreateInWorld(world, m_sText, flags, mat, m_fTextSize, ARGBF(m_fOpacity, m_vTextColor[0], m_vTextColor[1], m_vTextColor[2]), ARGBF(m_fBackgroundOpacity, 0, 0, 0));
	}
}