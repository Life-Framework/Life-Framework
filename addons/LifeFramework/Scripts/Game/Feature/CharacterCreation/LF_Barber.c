// LF_Barber.c — the barber's chair: sit, pick a look, pay at the till (#3234).
// The engine bakes hair and beard into each head mesh (Prefabs/Characters/Heads/Head_*.et has one
// MeshObject and no hair or beard part, and ParametricMaterialInstanceComponent has no hair colour), so
// a look is a whole vanilla head: the same VisualIdentity.SetHead + CommitChanges the base game's
// arsenal uses for its camo heads (SCR_PlayerArsenalLoadout.ApplyCamouflageToCharacter). The server
// sets the head, and the identity component replicates it to every client and to late joiners.
// The chosen head is kept per character record key and saved (LF_BarberSavePayload); every spawn
// puts it back (LF_BarberSpawnSubscriber), so it survives death, respawn and a restart.

enum LF_EBarberRefusal
{
	NONE,
	NO_CHARACTER,
	NO_LOOK,
	SAME_LOOK,
	NO_CASH,
	NO_SEAT,
	OFF
}

class LF_Barber
{
	static const string FEATURE = "Barber";
	//! Cash a haircut costs. A pack chair can set its own (LF_BarberAction m_iFee).
	static const int DEFAULT_FEE = 40;	// lf-fixed: the default chair fee; the chair prefab's m_iFee overrides it
	static const string REASON = "barber";

	//! Character record key -> chosen head.
	protected static ref map<string, ResourceName> s_mLooks = new map<string, ResourceName>();

	//------------------------------------------------------------------------------------------------
	//! The looks on offer: the base game's civilian heads, each with its own hair and beard.
	static array<ResourceName> Looks()
	{
		array<ResourceName> looks = {
			"{EAF055495BA85CA1}Prefabs/Characters/Heads/Head_White_02.et",
			"{0F6C19B0574DCBCA}Prefabs/Characters/Heads/Head_White_03.et",
			"{32583C88222142FD}Prefabs/Characters/Heads/Head_White_04.et",
			"{D7C470712EC4D596}Prefabs/Characters/Heads/Head_White_05.et",
			"{BB90449192005AB8}Prefabs/Characters/Heads/Head_White_06.et",
			"{C1F80EE178D948D6}Prefabs/Characters/Heads/Head_White_08.et",
			"{24644218743CDFBD}Prefabs/Characters/Heads/Head_White_09.et",
			"{143245D81F2BAB94}Prefabs/Characters/Heads/Head_White_10.et",
			"{04F4D8CBA36A534B}Prefabs/Characters/Heads/Head_Black_02.et",
			"{24D28E910BF9F648}Prefabs/Characters/Heads/Head_Asian_02.et"
		};
		return looks;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the look's head, or "" when the index is out of range
	static ResourceName LookAt(int index)
	{
		array<ResourceName> looks = Looks();
		if (index < 0 || index >= looks.Count())
			return ResourceName.Empty;

		return looks[index];
	}

	//------------------------------------------------------------------------------------------------
	//! \return the look index of a head, or -1 for a head not on offer
	static int IndexOf(ResourceName head)
	{
		return Looks().Find(head);
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EBarberRefusal refusal)
	{
		switch (refusal)
		{
			case LF_EBarberRefusal.NO_CHARACTER: return "#LF-Barber_RefuseNoCharacter";
			case LF_EBarberRefusal.NO_LOOK: return "#LF-Barber_RefuseNoLook";
			case LF_EBarberRefusal.SAME_LOOK: return "#LF-Barber_RefuseSameLook";
			case LF_EBarberRefusal.NO_CASH: return "#LF-Barber_RefuseNoCash";
			case LF_EBarberRefusal.NO_SEAT: return "#LF-Barber_RefuseNoSeat";
			case LF_EBarberRefusal.OFF: return "#LF-Barber_RefuseOff";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The head a character shows now, or "" when it has no identity.
	static ResourceName CurrentHead(IEntity character)
	{
		VisualIdentity visual = VisualOf(character);
		if (!visual)
			return ResourceName.Empty;

		return visual.GetHead();
	}

	//------------------------------------------------------------------------------------------------
	protected static VisualIdentity VisualOf(IEntity character)
	{
		if (!character)
			return null;

		CharacterIdentityComponent identityComp = CharacterIdentityComponent.Cast(character.FindComponent(CharacterIdentityComponent));
		if (!identityComp)
			return null;

		Identity identity = identityComp.GetIdentity();
		if (!identity)
			return null;

		return identity.GetVisualIdentity();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a head on a live character. On the server the identity replicates; on a client it is a
	//! local preview only (the next server commit or a respawn replaces it).
	//! \return true when the head was set
	static bool ApplyHead(IEntity character, ResourceName head)
	{
		if (head.IsEmpty())
			return false;

		CharacterIdentityComponent identityComp;
		if (character)
			identityComp = CharacterIdentityComponent.Cast(character.FindComponent(CharacterIdentityComponent));

		VisualIdentity visual = VisualOf(character);
		if (!identityComp || !visual)
		{
			LF_Debug.Error(FEATURE, "head not set: the character has no visual identity");
			return false;
		}

		if (visual.GetHead() == head)
			return true;

		visual.SetHead(head);
		identityComp.CommitChanges();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the haircut. Takes the fee in cash, sets the head and keeps it for the character key.
	//! Nothing is charged when anything refuses.
	static LF_EBarberRefusal Cut(IEntity character, string characterKey, int lookIndex, int fee)
	{
		if (!character || characterKey.IsEmpty())
			return Refuse(LF_EBarberRefusal.NO_CHARACTER, characterKey);

		ResourceName head = LookAt(lookIndex);
		if (head.IsEmpty())
			return Refuse(LF_EBarberRefusal.NO_LOOK, characterKey);

		if (CurrentHead(character) == head)
			return Refuse(LF_EBarberRefusal.SAME_LOOK, characterKey);

		if (fee < 0)
			fee = 0;

		if (fee > 0 && LF_MoneyUtils.GetCash(character) < fee)
			return Refuse(LF_EBarberRefusal.NO_CASH, characterKey);

		if (fee > 0)
		{
			int taken = LF_MoneyUtils.RemoveCash(character, fee, REASON);
			if (taken < fee)
			{
				if (taken > 0)
					LF_MoneyUtils.AddCash(character, taken, REASON);

				return Refuse(LF_EBarberRefusal.NO_CASH, characterKey);
			}
		}

		if (!ApplyHead(character, head))
		{
			if (fee > 0)
				LF_MoneyUtils.AddCash(character, fee, REASON);

			return Refuse(LF_EBarberRefusal.NO_CHARACTER, characterKey);
		}

		s_mLooks.Set(characterKey, head);
		LF_Debug.Info(FEATURE, string.Format("haircut: %1 paid %2 for look %3 (%4)", characterKey, fee, lookIndex, head));
		return LF_EBarberRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EBarberRefusal Refuse(LF_EBarberRefusal refusal, string characterKey)
	{
		LF_Debug.Info(FEATURE, string.Format("haircut refused for '%1': %2", characterKey, typename.EnumToString(LF_EBarberRefusal, refusal)));
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! The saved look of a character key, or "" when it never sat in the chair.
	static ResourceName SavedLook(string characterKey)
	{
		ResourceName head;
		s_mLooks.Find(characterKey, head);
		return head;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts a character's saved look back (spawn, respawn, restart load).
	//! \return true when a saved look was applied
	static bool Restore(IEntity character, string characterKey)
	{
		ResourceName head = SavedLook(characterKey);
		if (head.IsEmpty())
			return false;

		if (!ApplyHead(character, head))
			return false;

		LF_Debug.Info(FEATURE, string.Format("look restored for %1: %2", characterKey, head));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_BarberLookRow> ExportAll()
	{
		array<ref LF_BarberLookRow> rows = {};
		foreach (string key, ResourceName head : s_mLooks)
		{
			rows.Insert(LF_BarberLookRow.Create(key, head));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store with the saved rows. A row with no key or no head is skipped.
	//! \return the rows kept
	static int ApplyAll(array<ref LF_BarberLookRow> rows)
	{
		s_mLooks.Clear();
		if (!rows)
			return 0;

		foreach (LF_BarberLookRow row : rows)
		{
			if (!row || row.m_sKey.IsEmpty() || row.m_sHead.IsEmpty())
				continue;

			s_mLooks.Set(row.m_sKey, row.m_sHead);
		}

		return s_mLooks.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: forget one key's look.
	static void Forget(string characterKey)
	{
		s_mLooks.Remove(characterKey);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLooks = new map<string, ResourceName>();
	}
}

//------------------------------------------------------------------------------------------------
//! One saved look: a character record key and its head.
class LF_BarberLookRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sKey;
	string m_sHead;

	//------------------------------------------------------------------------------------------------
	static LF_BarberLookRow Create(string key, ResourceName head)
	{
		LF_BarberLookRow row = new LF_BarberLookRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_sHead = head;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: every spawn puts the character's saved look back.
class LF_BarberSpawnSubscriber : LF_EventSubscriber
{
	protected static ref LF_BarberSpawnSubscriber s_Subscriber;
	static const int RETRY_MS = 2000; // lf-fixed: one retry for a player whose character is not there at the spawn event

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Cast(busEvent);
		if (!spawned)
			return false;

		if (!RestoreFor(spawned.m_iPlayerId) && GetGame())
			GetGame().GetCallqueue().CallLater(LF_BarberSpawnSubscriber.RetryRestore, RETRY_MS, false, spawned.m_iPlayerId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RetryRestore(int playerId)
	{
		RestoreFor(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the player's character or key is not known yet
	protected static bool RestoreFor(int playerId)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return false;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		string key = LF_Utils.GetPlayerUID(playerId);
		if (!character || key.IsEmpty())
			return false;

		LF_Barber.Restore(character, key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Install()
	{
		ResetStatics();
		if (!Replication.IsServer())
			return;

		s_Subscriber = new LF_BarberSpawnSubscriber();
		LF_EventBus.Subscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);

		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the server restores looks from the world's start.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_BarberSpawnSubscriber.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Barber's statics (LF_StaticReset finds it by reflection).
class LF_Barber_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Barber";
	}

	override void Apply()
	{
		LF_BarberSpawnSubscriber.ResetStatics();
		LF_Barber.ResetStatics();
	}
}
