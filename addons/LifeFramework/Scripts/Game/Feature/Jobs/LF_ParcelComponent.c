//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Jobs", description: "Courier parcel: its description names the drop.")]
class LF_ParcelComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ParcelComponent : ScriptComponent
{
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
