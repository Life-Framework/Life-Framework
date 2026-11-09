//------------------------------------------------------------------------------------------------
//! Owner-replicated copy of a player's whitelist flags so client-side action checks
//! (LF_WhitelistAction) can run without server round trips.
[ComponentEditorProps(category: "EveronLife/Feature/Whitelist", description: "Local copy of whitelists to allow action checks")]
class LF_PlayerWhitelistComponentClass : ScriptComponentClass
{
}

class LF_PlayerWhitelistComponent : ScriptComponent
{
	protected LF_WhitelistType m_ePlayerWhitelists;

	//------------------------------------------------------------------------------------------------
	//! Server: push the full flag set to the owning client.
	void RpcSetWhitelists(LF_WhitelistType flags)
	{
		Rpc(RpcDo_SetWhitelists, flags);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SetWhitelists(LF_WhitelistType flags)
	{
		m_ePlayerWhitelists = flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add flags to the owning client.
	void RpcEnableWhitelist(LF_WhitelistType flags)
	{
		Rpc(RpcDo_EnableWhitelist, flags);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EnableWhitelist(LF_WhitelistType flags)
	{
		m_ePlayerWhitelists |= flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remove flags from the owning client.
	void RpcDisableWhitelist(LF_WhitelistType flags)
	{
		Rpc(RpcDo_DisableWhitelist, flags);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DisableWhitelist(LF_WhitelistType flags)
	{
		m_ePlayerWhitelists &= ~flags;
	}

	//------------------------------------------------------------------------------------------------
	bool HasWhitelist(LF_WhitelistType flags)
	{
		return SCR_Enum.HasPartialFlag(m_ePlayerWhitelists, flags);
	}
}