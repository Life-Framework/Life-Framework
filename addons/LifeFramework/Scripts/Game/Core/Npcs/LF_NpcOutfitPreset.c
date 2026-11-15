//! One NPC outfit: a clothing prefab per loadout slot. A preset is a `.conf` of this class; the World
//! Editor's LF NPC Dresser plugin (Scripts/WorkbenchGame/LFTools/LF_NpcDresserPlugin.c) lists every
//! one under Configs/NpcOutfits/ of any loaded addon and writes its slots into the selected NPCs'
//! BaseLoadoutManagerComponent. Core presets dress the framework's roles in its own uniforms; the era
//! presets (what a 1989 Everon local wore) live in the pack (docs/tooling/npc-dresser.md).
//! An empty slot is left as the NPC already has it.
[BaseContainerProps(configRoot: true)]
class LF_NpcOutfitPreset
{
	[Attribute("", UIWidgets.EditBox, "Name shown in the dresser and in its log")]
	string m_sName;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Head: hat, cap or helmet (the Hat slot)", "et")]
	ResourceName m_Head;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Torso: jacket, shirt or coat (the Jacket slot)", "et")]
	ResourceName m_Torso;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Legs: trousers (the Pants slot)", "et")]
	ResourceName m_Legs;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Feet: shoes or boots (the Boots slot)", "et")]
	ResourceName m_Feet;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Vest (the Vest slot)", "et")]
	ResourceName m_Vest;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Backpack (the Back slot)", "et")]
	ResourceName m_Backpack;
}
