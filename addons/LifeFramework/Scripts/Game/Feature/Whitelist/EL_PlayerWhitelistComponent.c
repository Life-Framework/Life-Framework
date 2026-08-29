//------------------------------------------------------------------------------------------------
//! Owner-replicated copy of a player's whitelist flags so client-side action checks
//! (EL_WhitelistAction) can run without server round trips.
[ComponentEditorProps(category: "EveronLife/Feature/Whitelist", description: "Local copy of whitelists to allow action checks")]
class EL_PlayerWhitelistComponentClass : ScriptComponentClass
{
}

class EL_PlayerWhitelistComponent : ScriptComponent
{
	protected EL_WhitelistType m_ePlayerWhitelists;

	//------------------------------------------------------------------------------------------------
	//! Server: push the full flag set to the owning client.
	void RpcSetWhitelists(EL_WhitelistType flags)
	{
		Rpc(RpcDo_SetWhitelists, flags);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SetWhitelists(EL_WhitelistType flags)
	{
		m_ePlayerWhitelists = flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add flags to the owning client.
	void RpcEnableWhitelist(EL_WhitelistType flags)
	{
		Rpc(RpcDo_EnableWhitelist, flags);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EnableWhitelist(EL_WhitelistType flags)
	{
		m_ePlayerWhitelists |= flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remove flags from the owning client.
	void RpcDisableWhitelist(EL_WhitelistType flags)
	{
		Rpc(RpcDo_DisableWhitelist, flags);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DisableWhitelist(EL_WhitelistType flags)
	{
		m_ePlayerWhitelists &= ~flags;
	}

	//------------------------------------------------------------------------------------------------
	bool HasWhitelist(EL_WhitelistType flags)
	{
		return SCR_Enum.HasPartialFlag(m_ePlayerWhitelists, flags);
	}
}