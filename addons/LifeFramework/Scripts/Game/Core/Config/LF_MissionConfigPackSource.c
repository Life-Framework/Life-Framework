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

	//! Tests set a manifest here; null reads the header's m_LF_PackManifest.
	protected ref LF_PackManifest m_Manifest;
	protected string m_sCoreVersion = LF_PackManifest.CORE_VERSION;
	protected bool m_bRefusalLogged;

	//------------------------------------------------------------------------------------------------
	void SetManifest(LF_PackManifest manifest, string coreVersion = LF_PackManifest.CORE_VERSION)
	{
		m_Manifest = manifest;
		m_sCoreVersion = coreVersion;
		m_bRefusalLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the mission's pack accepts the running core. No manifest means no check. A manifest
	//! that is named but does not load refuses the pack: the version cannot be shown to match.
	bool IsPackAccepted(LF_MissionHeader header)
	{
		LF_PackManifest manifest = m_Manifest;
		if (!manifest && header && !header.m_LF_PackManifest.IsEmpty())
		{
			Resource resource = BaseContainerTools.LoadContainer(header.m_LF_PackManifest);
			if (resource && resource.IsValid())
				manifest = LF_PackManifest.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!manifest)
			{
				if (!m_bRefusalLogged)
					LF_Debug.Error(LF_PackManifest.FEATURE, string.Format("pack manifest %1 does not load as an LF_PackManifest; this mission's config overlays are not loaded", header.m_LF_PackManifest));
				m_bRefusalLogged = true;
				return false;
			}
			m_Manifest = manifest;
		}
		if (!manifest)
			return true;
		if (manifest.Supports(m_sCoreVersion))
			return true;

		if (!m_bRefusalLogged)
			LF_Debug.Error(LF_PackManifest.FEATURE, manifest.RefusalLine(m_sCoreVersion));
		m_bRefusalLogged = true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		LF_MissionHeader header = ResolveHeader();
		if (!IsPackAccepted(header))
			return;
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
	override LF_PackManifest GetManifest()
	{
		LF_MissionHeader header = ResolveHeader();
		if (!IsPackAccepted(header))
			return null;
		return m_Manifest;
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mission-header";
	}
};
