//------------------------------------------------------------------------------------------------
//! D4 option (b): the pack `.conf` overlays come from the running mission header
//! (LF_MissionHeader.m_aLF_ConfigOverlays). The core names no pack: a world picks its pack by
//! picking a mission, and that header lists the overlay files. The list is in precedence order,
//! lowest first. It returns every listed file for every core resource; the feature's apply step
//! keeps the ones whose class matches its own (LF_ShopPricesConfig.ApplyLayers). Installed by
//! default in LF_ConfigService.GetPackSource.
//------------------------------------------------------------------------------------------------
class LF_MissionConfigPackSource : LF_ConfigPackSource
{
	//! Tests set a header here; null reads the running mission's.
	protected ref LF_MissionHeader m_Header;

	//------------------------------------------------------------------------------------------------
	void SetHeader(LF_MissionHeader header)
	{
		m_Header = header;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MissionHeader ResolveHeader()
	{
		if (m_Header)
			return m_Header;
		return LF_MissionHeader.GetCurrent();
	}

	//------------------------------------------------------------------------------------------------
	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		LF_MissionHeader header = ResolveHeader();
		if (!header || !header.m_aLF_ConfigOverlays)
			return;

		foreach (ResourceName overlay : header.m_aLF_ConfigOverlays)
		{
			if (overlay.IsEmpty())
				continue;
			if (overlay == coreConf)
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("mission header lists the core file as its own overlay, skipped: %1", overlay));
				continue;
			}
			overlays.Insert(overlay);
		}
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mission-header";
	}
};
