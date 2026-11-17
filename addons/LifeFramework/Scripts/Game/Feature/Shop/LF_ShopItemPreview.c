// LF_ShopItemPreview.c — the picture of the picked line on a counter's order slip.
//
// The slip carries a UI kit PORTRAIT part with no lines; its photo widget is an
// ItemPreviewWidget, the one the wanted poster and the identity card draw a person into.
// Here it draws the line's real item prefab through ItemPreviewManagerEntity
// (LF_WantedPortrait.ShowPrefab), resized to a square window so an item is not cropped to the
// head-and-shoulders framing. Client only. Cars and fences keep their own previews (follow-ups).
class LF_ShopItemPreview : Managed
{
	static const string PART = "Preview";
	//! Side of the square window, in layout units.
	static const float WINDOW = 100;	// lf-fixed: layout size of the preview window, not a gameplay value

	protected ref LF_WantedPortrait m_Portrait = new LF_WantedPortrait();
	protected Widget m_wRoot;

	//------------------------------------------------------------------------------------------------
	//! The part the slip adds for the preview: a photo window, no stamp, no printed lines.
	static LF_UIPart Part()
	{
		array<string> none = {};
		return LF_UIPart.CreatePortrait(PART, "", "", none, none);
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab of the line at index, or empty: no such line (a row the filter removed, an index
	//! left over from before a refill), a null entry, or a line without a prefab.
	static ResourceName Resolve(array<ref LF_ShopCatalogItem> shown, int index)
	{
		if (!shown || index < 0 || index >= shown.Count())
			return ResourceName.Empty;

		LF_ShopCatalogItem item = shown.Get(index);
		if (!item)
			return ResourceName.Empty;

		return item.m_ItemPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Attach to the screen's preview part and hide it until a line is picked.
	void Attach(LF_UIScreen screen)
	{
		if (!screen)
			return;

		ItemPreviewWidget photo = screen.GetPortrait(PART);
		if (!photo)
		{
			LF_Debug.Error("Shop", "slip preview: the screen has no " + PART + " part");
			return;
		}

		// photo -> PhotoClip -> PortraitFrame -> PortraitBox (SizeLayout) -> Portrait.
		Widget clip = photo.GetParent();
		Widget frame;
		if (clip)
			frame = clip.GetParent();

		if (!frame)
		{
			LF_Debug.Error("Shop", "slip preview: the portrait layout changed shape");
			return;
		}

		FrameSlot.SetPos(photo, 0, 0);
		FrameSlot.SetSize(photo, WINDOW, WINDOW);
		FrameSlot.SetPos(clip, 0, 0);
		FrameSlot.SetSize(clip, WINDOW, WINDOW);
		Widget back = frame.FindAnyWidget("PhotoBack");
		if (back)
			FrameSlot.SetSize(back, WINDOW, WINDOW);

		Widget box = frame.GetParent();
		SizeLayoutWidget size = SizeLayoutWidget.Cast(box);
		if (size)
		{
			size.SetHeightOverride(WINDOW);
			size.SetWidthOverride(WINDOW);
		}

		if (box)
			m_wRoot = box.GetParent();

		Show(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Draw the prefab; an empty prefab or a failed draw hides the window. \return true when drawn.
	bool Draw(LF_UIScreen screen, ResourceName prefab)
	{
		if (!screen || prefab.IsEmpty())
		{
			Show(false);
			return false;
		}

		bool drawn = m_Portrait.ShowPrefab(screen.GetPortrait(PART), prefab);
		Show(drawn);
		return drawn;
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		if (m_Portrait)
			m_Portrait.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void Show(bool on)
	{
		if (m_wRoot)
			m_wRoot.SetVisible(on);
	}
}
