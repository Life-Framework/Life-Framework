//------------------------------------------------------------------------------------------------
//! One board component, many boards (#1945, chapter #1014): the server answers a read, the client
//! paints the answer. A board kind (the wanted board, a service board, a case board) is a provider
//! registered under its kind id; the read RPC, the range check, the unknown-kind refusal and the card
//! opening are written once, here.
//!
//! Nothing is replicated and nothing is cached: an answer is built per read (LF_BoardProvider.Answer),
//! crosses once as an RPC (arrays ride the RPC, never an RplProp), is painted, and is dropped. A second
//! read asks the server again, so a card never shows what changed since.
//!
//! Registering: a feature adds `modded class LF_BoardProviderSet` in its own file and inserts its
//! provider in Collect (the registry holds no central list).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! What the server says on one read: a flag for the reader's standing (an on-duty officer sees
//! more) and two flat arrays the provider lays out as it likes. Flat, so the answer crosses as an
//! RPC; a provider that needs rows packs a fixed stride.
class LF_BoardAnswer
{
	//! The reader may see the full board (the provider decides what that means).
	bool m_bPrivileged;
	ref array<string> m_aTexts = {};
	ref array<int> m_aNumbers = {};
}

//------------------------------------------------------------------------------------------------
//! A board kind: what the server answers and how the client paints it. Subclass per kind.
class LF_BoardProvider
{
	//------------------------------------------------------------------------------------------------
	//! The id a read names (RpcAsk) and the registry keys on. Unique per kind.
	string GetKind()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The read action the reader must stand at (LF_ActionSite). A kind that names none cannot be read
	//! over the network (LF_BoardProviders.ServerRead refuses it).
	string GetReadAction()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the answer for this reader, built fresh. Null refuses the read (log why).
	LF_BoardAnswer Answer(int playerId)
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the lines of the card for an answer, so a test reads what a player would see.
	array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the card opened; log what it shows.
	void OnOpened(notnull LF_BoardAnswer answer)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Features add `modded class LF_BoardProviderSet` and insert their provider in Collect (call super
//! first). The registry builds its table from this on first use.
class LF_BoardProviderSet
{
	void Collect(notnull array<ref LF_BoardProvider> providers)
	{
	}
}

//------------------------------------------------------------------------------------------------
class LF_BoardProviders
{
	static const string FEATURE = "Boards";
	//! The longest kind id a read may name; a longer one is refused unlogged.
	static const int MAX_KIND_LENGTH = 32;

	protected static ref map<string, ref LF_BoardProvider> s_Providers;

	//------------------------------------------------------------------------------------------------
	protected static void Ensure()
	{
		if (s_Providers)
			return;

		s_Providers = new map<string, ref LF_BoardProvider>();
		array<ref LF_BoardProvider> found = {};
		LF_BoardProviderSet providerSet = new LF_BoardProviderSet();
		providerSet.Collect(found);
		foreach (LF_BoardProvider provider : found)
		{
			Register(provider);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Adds (or replaces) the provider for its kind. A provider with no kind is refused.
	static void Register(LF_BoardProvider provider)
	{
		if (!provider || provider.GetKind().IsEmpty())
		{
			LF_Debug.Error(FEATURE, "board provider refused: none, or it names no kind");
			return;
		}

		// The table fills from the feature sets first, so a provider added here sits beside them.
		Ensure();
		s_Providers.Set(provider.GetKind(), provider);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The provider for a kind, or null for a kind nobody registered.
	static LF_BoardProvider Find(string kind)
	{
		Ensure();
		LF_BoardProvider provider;
		if (!s_Providers.Find(kind, provider))
			return null;

		return provider;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, no range rule: build the answer for a reader. Null for an unknown kind or a refusal.
	static LF_BoardAnswer Answer(string kind, int playerId)
	{
		LF_BoardProvider provider = Find(kind);
		if (!provider)
		{
			LF_Debug.Warn(FEATURE, string.Format("board read refused for player %1: no provider for kind '%2'", playerId, kind));
			return null;
		}

		return provider.Answer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the card lines for an answer. Empty for an unknown kind.
	static array<ref LF_UIPart> Paint(string kind, LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		LF_BoardProvider provider = Find(kind);
		if (!provider || !answer)
		{
			LF_Debug.Error(FEATURE, string.Format("board card not painted: kind '%1' provider=%2 answer=%3", kind, provider != null, answer != null));
			return parts;
		}

		return provider.Paint(answer);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of the read RPC. The reader must stand at the kind's board; the server finds it,
	//! the client names only the kind.
	static void ServerRead(SCR_PlayerController controller, string kind)
	{
		if (!controller)
			return;

		int playerId = controller.GetPlayerId();
		// The kind comes from the client: bound it before it is looked up or logged.
		if (kind.Length() > MAX_KIND_LENGTH)
		{
			LF_Debug.Warn(FEATURE, string.Format("board read refused for player %1: kind name of %2 characters", playerId, kind.Length()));
			return;
		}

		LF_BoardProvider provider = Find(kind);
		if (!provider)
		{
			LF_Debug.Warn(FEATURE, string.Format("board read refused for player %1: no provider for kind '%2'", playerId, kind));
			return;
		}

		// A board a player reads over the network must name where it stands; a provider with no read
		// action would answer anyone anywhere.
		string action = provider.GetReadAction();
		if (action.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("board '%1' read refused for player %2: the kind names no read action", kind, playerId));
			return;
		}

		IEntity reader = controller.GetControlledEntity();
		if (!reader || !LF_ActionSite.IsNear(reader.GetOrigin(), LF_PoliceConfig.BoardReadDistance(), action))
		{
			LF_Debug.Warn(FEATURE, string.Format("board '%1' read refused for player %2: not at the board", kind, playerId));
			return;
		}

		LF_BoardAnswer answer = provider.Answer(playerId);
		if (!answer)
			return;

		controller.LF_ShowBoard(kind, answer);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's answer arrived; paint it and open the card.
	//! \return False when there was nothing to show or the card did not open.
	static bool Open(string kind, LF_BoardAnswer answer)
	{
		LF_BoardProvider provider = Find(kind);
		if (!provider || !answer)
		{
			LF_Debug.Error(FEATURE, string.Format("board card not opened: kind '%1' provider=%2 answer=%3", kind, provider != null, answer != null));
			return false;
		}

		array<ref LF_UIPart> parts = provider.Paint(answer);
		if (!LF_WantedBoard.OpenParts(parts))
			return false;

		LF_Debug.Info(FEATURE, string.Format("board '%1' card open: privileged=%2 lines=%3", kind, answer.m_bPrivileged, parts.Count()));
		provider.OnOpened(answer);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Providers = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BoardProviders's statics (registered at script load; Core/LF_StaticReset.c).
class LF_BoardProviders_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_BoardProviders_StaticResetHook());

	override string Owner()
	{
		return "LF_BoardProviders";
	}

	override void Apply()
	{
		LF_BoardProviders.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The board bridge on the player controller (owned by the reader from join): one ask, one answer,
//! for every board kind.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server what the board of this kind says to this player.
	void LF_AskBoard(string kind)
	{
		if (Replication.IsServer())
			RpcAsk_LF_Board(kind);
		else
			Rpc(RpcAsk_LF_Board, kind);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send this player the answer.
	void LF_ShowBoard(string kind, notnull LF_BoardAnswer answer)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowBoard(kind, answer.m_bPrivileged, answer.m_aTexts, answer.m_aNumbers);
		else
			Rpc(RpcDo_LF_ShowBoard, kind, answer.m_bPrivileged, answer.m_aTexts, answer.m_aNumbers);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Board(string kind)
	{
		LF_BoardProviders.ServerRead(this, kind);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowBoard(string kind, bool privileged, array<string> texts, array<int> numbers)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_BoardProviders.FEATURE, "board card not opened: no workspace on this machine");
			return;
		}

		LF_BoardAnswer answer = new LF_BoardAnswer();
		answer.m_bPrivileged = privileged;
		if (texts)
			answer.m_aTexts.Copy(texts);

		if (numbers)
			answer.m_aNumbers.Copy(numbers);

		LF_BoardProviders.Open(kind, answer);
	}
}
