// LF_SignStyle.c: the style and size axes of a sign face (docs/architecture/sign-system.md).
//
// A style is how the lettering is made (layout and so font, ink, ground, ghost, tilt, leaders); a size is
// the carrier mesh the face is drawn on and how many layout units it shows. Both are rows of
// Configs/Signs/LF_SignStyles.conf; a pack overlay of LF_SignStyleConfig adds, replaces, patches or
// removes a row by id (LF_ConfigOverlay, docs/config/override-model.md).

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), LF_SignStyleTitle()]
class LF_SignStyleDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Style id, named by LF_SignComponent.m_sStyle or LF_SignDef.m_sStyle")]
	string m_sId;

	[Attribute("", UIWidgets.ResourceNamePicker, "Face layout: RTTexture0 holding Ground, Ghost, Title and a Rows frame. The layout's fonts are the style's fonts", "layout")]
	ResourceName m_sLayout;

	[Attribute("", UIWidgets.ResourceNamePicker, "Row layout: Label, Leader and Value text widgets, made once per price row", "layout")]
	ResourceName m_sRowLayout;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional finish: an imageset drawn over the ground and under nothing else, the layout's Finish image widget sized to the shown area (worn edges, tape, chips). Empty draws none", "imageset")]
	ResourceName m_sFinishImage;

	[Attribute("full", UIWidgets.EditBox, "Image name inside the finish imageset")]
	string m_sFinishName;

	[Attribute("0.11 0.13 0.12", UIWidgets.ColorPicker, "Ground colour (RGB 0..1)")]
	vector m_vGround;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Optional ground texture (.edds), tinted by the ground colour", "edds")]
	ResourceName m_sGroundTexture;

	[Attribute("0.93 0.93 0.89", UIWidgets.ColorPicker, "Ink colour (RGB 0..1)")]
	vector m_vInk;

	[Attribute("1", UIWidgets.Slider, "Ink alpha: below 1 lets the ground show through, like chalk", "0 1 0.01")]
	float m_fInkAlpha;

	[Attribute("0", UIWidgets.Slider, "Alpha of the ghost: the same words, shifted, faint, like rubbed-out chalk. 0 draws none", "0 1 0.01")]
	float m_fGhostAlpha;

	[Attribute("0 0 0", UIWidgets.EditBox, "Ghost shift, layout units (x y)")]
	vector m_vGhostShift;

	[Attribute("0 0 0", UIWidgets.ColorPicker, "Ghost colour (RGB 0..1) at the ghost alpha: a drop shadow is dark (use 0.01 for near black). All zero: the ink")]
	vector m_vGhostColor;

	[Attribute("0", UIWidgets.CheckBox, "The ground and ink are the placed prop's paint and lettering colours (LF_SignComponent.m_vPaintColor, m_vTextColor) when it has a painted face: shop fascias, enamel plates")]
	bool m_bPlacementColors;

	[Attribute("0", UIWidgets.EditBox, "Hand tilt of the title in degrees (0 for printed styles)")]
	float m_fTilt;

	[Attribute("1", UIWidgets.CheckBox, "Title in capitals")]
	bool m_bUpperTitle;

	[Attribute("0.06", UIWidgets.EditBox, "Margin as a share of the shown area's shorter side")]
	float m_fMargin;

	[Attribute("0.32", UIWidgets.EditBox, "Share of the inner height the title takes when the sign has rows")]
	float m_fTitleShare;

	[Attribute("0", UIWidgets.EditBox, "Raises the title (and its ghost) by this share of the title font, in a tall box the engine centres the line in: a client seats the lettering below the middle of the widget. 0 keeps the plain box")]
	float m_fTitleLift;

	[Attribute("220", UIWidgets.EditBox, "Largest title font, layout units")]
	int m_iTitleFontMax;

	[Attribute("120", UIWidgets.EditBox, "Largest row font, layout units")]
	int m_iRowFontMax;

	[Attribute(".", UIWidgets.EditBox, "Leader between a row's label and value ('' draws none)")]
	string m_sLeader;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), LF_SignSizeTitle()]
class LF_SignSizeDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Size id, named by LF_SignComponent.m_sSize")]
	string m_sId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Carrier: a one-mesh quad whose material samples $rendertarget (LF_SignFace.emat baked, #3039). Empty: the prop's own mesh is the face", "et")]
	ResourceName m_sCarrier;

	[Attribute("1920 1080 0", UIWidgets.EditBox, "Render target size, layout units (the face layout's RTTexture0)")]
	vector m_vTarget;

	[Attribute("1920 1080 0", UIWidgets.EditBox, "The part of the target the mesh shows, from the top-left, layout units")]
	vector m_vShown;
}

//------------------------------------------------------------------------------------------------
class LF_SignStyleTitle : BaseContainerCustomTitle
{
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		source.Get("m_sId", title);
		title = "style " + title;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_SignSizeTitle : BaseContainerCustomTitle
{
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		source.Get("m_sId", title);
		title = "size " + title;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_SignStyleConfig
{
	[Attribute("", UIWidgets.Object, "Styles")]
	ref array<ref LF_SignStyleDef> m_aStyles;

	[Attribute("", UIWidgets.Object, "Sizes")]
	ref array<ref LF_SignSizeDef> m_aSizes;

	//------------------------------------------------------------------------------------------------
	protected void EnsureLists()
	{
		if (!m_aStyles)
			m_aStyles = new array<ref LF_SignStyleDef>();
		if (!m_aSizes)
			m_aSizes = new array<ref LF_SignSizeDef>();
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class the mission header lists, in load order. \return Rows changed.
	int ApplyPackOverlays()
	{
		EnsureLists();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_SignStyles.CONFIG_PATH, LF_SignStyleConfig, "signstyles", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		int total;
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info("Signs", string.Format("pack overlay %1 changed %2 sign styles and sizes", names[i], changed));
			total += changed;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one overlay container's m_aStyles and m_aSizes by the override verbs. \return Rows changed.
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		EnsureLists();
		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_SignStyleList(this), container, "m_aStyles", source, claims);
		changed += LF_ConfigOverlay.ApplyContainerList(new LF_SignSizeList(this), container, "m_aSizes", source, claims);
		return changed;
	}
}

//------------------------------------------------------------------------------------------------
class LF_SignStyleList : LF_ConfigList
{
	protected LF_SignStyleConfig m_Config;

	void LF_SignStyleList(LF_SignStyleConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "signstyles.styles";
	}

	override int Count()
	{
		return m_Config.m_aStyles.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aStyles[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_SignStyleDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_SignStyleDef style = new LF_SignStyleDef();
		style.m_sId = id;
		return style;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_SignStyleDef style = LF_SignStyleDef.Cast(record);
		if (index < m_Config.m_aStyles.Count())
			m_Config.m_aStyles.Set(index, style);
		else
			m_Config.m_aStyles.Insert(style);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aStyles.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_SignStyleDef style = LF_SignStyleDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sLayout") && container.Get("m_sLayout", style.m_sLayout))
			changed++;
		if (container.IsVariableSetDirectly("m_sRowLayout") && container.Get("m_sRowLayout", style.m_sRowLayout))
			changed++;
		if (container.IsVariableSetDirectly("m_sFinishImage") && container.Get("m_sFinishImage", style.m_sFinishImage))
			changed++;
		if (container.IsVariableSetDirectly("m_sFinishName") && container.Get("m_sFinishName", style.m_sFinishName))
			changed++;
		if (container.IsVariableSetDirectly("m_vGround") && container.Get("m_vGround", style.m_vGround))
			changed++;
		if (container.IsVariableSetDirectly("m_sGroundTexture") && container.Get("m_sGroundTexture", style.m_sGroundTexture))
			changed++;
		if (container.IsVariableSetDirectly("m_vInk") && container.Get("m_vInk", style.m_vInk))
			changed++;
		if (container.IsVariableSetDirectly("m_fInkAlpha") && container.Get("m_fInkAlpha", style.m_fInkAlpha))
			changed++;
		if (container.IsVariableSetDirectly("m_fGhostAlpha") && container.Get("m_fGhostAlpha", style.m_fGhostAlpha))
			changed++;
		if (container.IsVariableSetDirectly("m_vGhostShift") && container.Get("m_vGhostShift", style.m_vGhostShift))
			changed++;
		if (container.IsVariableSetDirectly("m_fTilt") && container.Get("m_fTilt", style.m_fTilt))
			changed++;
		if (container.IsVariableSetDirectly("m_bUpperTitle") && container.Get("m_bUpperTitle", style.m_bUpperTitle))
			changed++;
		if (container.IsVariableSetDirectly("m_fMargin") && container.Get("m_fMargin", style.m_fMargin))
			changed++;
		if (container.IsVariableSetDirectly("m_fTitleShare") && container.Get("m_fTitleShare", style.m_fTitleShare))
			changed++;
		if (container.IsVariableSetDirectly("m_fTitleLift") && container.Get("m_fTitleLift", style.m_fTitleLift))
			changed++;
		if (container.IsVariableSetDirectly("m_iTitleFontMax") && container.Get("m_iTitleFontMax", style.m_iTitleFontMax))
			changed++;
		if (container.IsVariableSetDirectly("m_iRowFontMax") && container.Get("m_iRowFontMax", style.m_iRowFontMax))
			changed++;
		if (container.IsVariableSetDirectly("m_sLeader") && container.Get("m_sLeader", style.m_sLeader))
			changed++;

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_SignStyleDef style = LF_SignStyleDef.Cast(record);
		if (style.m_sLayout.IsEmpty() || style.m_sRowLayout.IsEmpty())
			return "a sign style needs a layout and a row layout (m_sLayout, m_sRowLayout)";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
class LF_SignSizeList : LF_ConfigList
{
	protected LF_SignStyleConfig m_Config;

	void LF_SignSizeList(LF_SignStyleConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "signstyles.sizes";
	}

	override int Count()
	{
		return m_Config.m_aSizes.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aSizes[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_SignSizeDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_SignSizeDef size = new LF_SignSizeDef();
		size.m_sId = id;
		return size;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_SignSizeDef size = LF_SignSizeDef.Cast(record);
		if (index < m_Config.m_aSizes.Count())
			m_Config.m_aSizes.Set(index, size);
		else
			m_Config.m_aSizes.Insert(size);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aSizes.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_SignSizeDef size = LF_SignSizeDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sCarrier") && container.Get("m_sCarrier", size.m_sCarrier))
			changed++;
		if (container.IsVariableSetDirectly("m_vTarget") && container.Get("m_vTarget", size.m_vTarget))
			changed++;
		if (container.IsVariableSetDirectly("m_vShown") && container.Get("m_vShown", size.m_vShown))
			changed++;

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_SignSizeDef size = LF_SignSizeDef.Cast(record);
		if (size.m_vShown[0] <= 0 || size.m_vShown[1] <= 0)
			return "a sign size needs a shown area (m_vShown)";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The loaded styles and sizes, core plus pack overlays, read once.
class LF_SignStyles
{
	static const ResourceName CONFIG_PATH = "{6C2E9A41D7F05B38}Configs/Signs/LF_SignStyles.conf";

	protected static ref LF_SignStyleConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! Loads a style config. Logs and returns null when it does not resolve.
	static LF_SignStyleConfig Load(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error("Signs", "sign style config does not resolve: " + path);
			return null;
		}

		LF_SignStyleConfig config = LF_SignStyleConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error("Signs", "sign style config has an unexpected root class: " + path);

		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureLoaded()
	{
		if (s_bLoadAttempted)
			return;

		s_bLoadAttempted = true;
		s_Config = Load(CONFIG_PATH);
		if (s_Config)
			s_Config.ApplyPackOverlays();
	}

	//------------------------------------------------------------------------------------------------
	//! The style with this id, or null. Empty id: null, without a log (a legacy sign).
	static LF_SignStyleDef FindStyle(string id)
	{
		if (id.IsEmpty())
			return null;

		EnsureLoaded();
		return FindStyleIn(s_Config, id);
	}

	//------------------------------------------------------------------------------------------------
	static LF_SignStyleDef FindStyleIn(LF_SignStyleConfig config, string id)
	{
		if (!config || !config.m_aStyles)
			return null;

		foreach (LF_SignStyleDef style : config.m_aStyles)
		{
			if (style && style.m_sId == id)
				return style;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The size with this id, or null.
	static LF_SignSizeDef FindSize(string id)
	{
		if (id.IsEmpty())
			return null;

		EnsureLoaded();
		return FindSizeIn(s_Config, id);
	}

	//------------------------------------------------------------------------------------------------
	static LF_SignSizeDef FindSizeIn(LF_SignStyleConfig config, string id)
	{
		if (!config || !config.m_aSizes)
			return null;

		foreach (LF_SignSizeDef size : config.m_aSizes)
		{
			if (size && size.m_sId == id)
				return size;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Forget the loaded config (world end, or Workbench after an edit of LF_SignStyles.conf).
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every style id, core and pack merged (the World Editor picker).
	static void StyleIds(notnull array<string> ids)
	{
		EnsureLoaded();
		if (!s_Config || !s_Config.m_aStyles)
			return;

		foreach (LF_SignStyleDef style : s_Config.m_aStyles)
		{
			if (style)
				ids.Insert(style.m_sId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every size id, core and pack merged (the World Editor picker).
	static void SizeIds(notnull array<string> ids)
	{
		EnsureLoaded();
		if (!s_Config || !s_Config.m_aSizes)
			return;

		foreach (LF_SignSizeDef size : s_Config.m_aSizes)
		{
			if (size)
				ids.Insert(size.m_sId);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_SignStyles's statics (LF_StaticReset finds it by reflection).
class LF_SignStyles_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SignStyles";
	}

	override void Apply()
	{
		LF_SignStyles.ResetStatics();
	}
}
