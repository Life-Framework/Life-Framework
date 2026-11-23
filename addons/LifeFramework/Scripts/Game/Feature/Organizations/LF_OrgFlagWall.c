// LF_OrgFlagWall.c - the flag sample wall at the registry (#4449, gangs C4). The wall is the menu: it
// stands twenty small sample flags and each one carries its own action "Fly the <name> flag"
// (LF_OrgFlagWallAction), so a gang leader picks the gang's flag by walking up to the flag they want.
// No window opens. The pick is checked on the server: the leader check (LF_OrgRights), the flag lock
// (LF_OrgFlagRules, #4220) and the table (LF_OrgFlags, #4213) are all reused, none rebuilt here.
//
// Core names no pack file: the samples come from prefab slots gang_flag_01 to gang_flag_20, whose rows
// the pack writes (Configs/PrefabSlots). The core slot file lists none of them, so SamplePrefab reads
// the layered slot config itself (core, then pack overlays) instead of LF_PrefabSlots.Get, which only
// answers for a core slot.

//------------------------------------------------------------------------------------------------
//! What a pick at the wall came to. Order is not saved.
enum LF_EOrgFlagWallResult
{
	//! The organization now flies the flag.
	PICKED,
	//! The organization already flew it; nothing changed.
	KEPT,
	//! The character is in no organization.
	NO_ORG,
	//! The character is a member but not a leader.
	NOT_LEADER,
	//! Not one of the preset flags (or the table refused it).
	BAD_FLAG,
	//! Another organization holds the flag and has a member online.
	HELD
}

//------------------------------------------------------------------------------------------------
class LF_OrgFlagWall
{
	static const string FEATURE = "OrgFlagWall";
	//! Slot id prefix; the slot of flag 7 is gang_flag_07.
	static const string SLOT_PREFIX = "gang_flag_";
	//! String id prefix of a flag's name; flag 7 is LF-OrgFlag_Name_07 (written without the # so the key
	//! scan of regen-localization does not read the prefix as a whole key).
	static const string NAME_KEY_PREFIX = "LF-OrgFlag_Name_";
	//! Samples per row on the wall.
	static const int COLUMNS = 5;	// lf-fixed: the wall's layout; the prefab's action contexts use the same grid
	//! Metres between two samples, both ways.
	static const float SPACING = 1.2;	// lf-fixed: the wall's layout
	//! Size of a sample against the full pole.
	static const float SAMPLE_SCALE = 0.3;	// lf-fixed: the wall's layout

	protected static ref LF_PrefabSlotsConfig s_Layered;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Layered = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Two(int flag)
	{
		if (flag < 10)
			return "0" + flag.ToString();

		return flag.ToString();
	}

	//------------------------------------------------------------------------------------------------
	static string SlotId(int flag)
	{
		return SLOT_PREFIX + Two(flag);
	}

	//------------------------------------------------------------------------------------------------
	//! The localization key of the flag's name: neutral in the core table, the real name in a pack's.
	static string NameKey(int flag)
	{
		return "#" + NAME_KEY_PREFIX + Two(flag);
	}

	//------------------------------------------------------------------------------------------------
	//! Where a flag's sample stands, local to the wall: a grid COLUMNS wide in front of the board (its
	//! front is -Z), the first row one metre out and each next row SPACING further.
	static vector SampleOffset(int flag)
	{
		int index = flag - 1;
		int column = index % COLUMNS;
		int row = index / COLUMNS;
		return Vector((column - (COLUMNS - 1) * 0.5) * SPACING, 0, -(1.0 + row * SPACING));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a leader holds the right to manage ranks (the founder, or a rank promoted to it).
	static bool IsLeader(int rights)
	{
		return LF_OrgRights.Has(rights, LF_EOrganizationPermission.MANAGE_RANKS);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the wall offers a pick only to a leader of an organization.
	static bool Offers(LF_OrgView view)
	{
		return view && view.HasOrg() && IsLeader(view.m_iRights);
	}

	//------------------------------------------------------------------------------------------------
	//! The sample prefab for a flag from its slot (core then pack layers), or empty when the slot has no
	//! usable prefab (logged). A pack-less boot has no rows, so the wall stands empty and says so.
	static ResourceName SamplePrefab(int flag)
	{
		if (!LF_OrgFlags.IsValid(flag))
			return ResourceName.Empty;

		if (!s_Layered)
		{
			s_Layered = LF_PrefabSlotsConfig.LoadCore();
			if (!s_Layered)
				return ResourceName.Empty;

			s_Layered.ApplyLayers();
		}

		LF_PrefabSlot slot = s_Layered.Find(SlotId(flag));
		if (!slot)
		{
			LF_Debug.Warn(FEATURE, string.Format("no prefab slot '%1'; that sample is not on the wall", SlotId(flag)));
			return ResourceName.Empty;
		}

		string why = LF_PrefabSlots.Unusable(slot.m_Prefab, null);
		if (!why.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("slot '%1' prefab %2 is unusable, %3; that sample is not on the wall", slot.m_sSlot, slot.m_Prefab, why));
			return ResourceName.Empty;
		}

		return slot.m_Prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the pick of `characterId`'s organization. Pure over the online list, so a test needs no
	//! players. holderName is the holding gang's name when the result is HELD.
	static LF_EOrgFlagWallResult Pick(string characterId, int flag, notnull array<string> online, out string holderName)
	{
		holderName = string.Empty;
		if (!LF_OrgFlags.IsValid(flag))
			return LF_EOrgFlagWallResult.BAD_FLAG;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
			return LF_EOrgFlagWallResult.NO_ORG;

		if (!IsLeader(LF_OrgRules.RightsOf(org, characterId)))
		{
			LF_Debug.Info(FEATURE, string.Format("flag pick refused: character=%1 is not a leader of org=%2", characterId, org.GetId()));
			return LF_EOrgFlagWallResult.NOT_LEADER;
		}

		if (LF_OrgFlags.FlagOf(org.GetId()) == flag)
			return LF_EOrgFlagWallResult.KEPT;

		string holderId = LF_OrgFlagRules.BlockingHolder(org.GetId(), flag, online);
		if (!holderId.IsEmpty())
		{
			LF_Organization holder = LF_OrganizationManager.Get(holderId);
			if (holder)
				holderName = holder.GetName();

			return LF_EOrgFlagWallResult.HELD;
		}

		if (!LF_OrgFlagRules.Pick(org.GetId(), flag, online))
			return LF_EOrgFlagWallResult.BAD_FLAG;

		LF_Debug.Info(FEATURE, string.Format("org=%1 now flies flag %2 (picked by character=%3)", org.GetId(), flag, characterId));
		return LF_EOrgFlagWallResult.PICKED;
	}

	//------------------------------------------------------------------------------------------------
	//! The words a result is shown in: the toast message key. HELD takes the holder's name as its argument.
	static string MessageKey(LF_EOrgFlagWallResult result)
	{
		if (result == LF_EOrgFlagWallResult.PICKED)
			return "#LF-OrgFlagWall_Done";
		if (result == LF_EOrgFlagWallResult.KEPT)
			return "#LF-OrgFlagWall_Same";
		if (result == LF_EOrgFlagWallResult.NO_ORG)
			return "#LF-OrgFlagWall_NoGang";
		if (result == LF_EOrgFlagWallResult.NOT_LEADER)
			return "#LF-OrgFlagWall_NotLeader";
		if (result == LF_EOrgFlagWallResult.HELD)
			return "#LF-OrgFlagWall_Held";

		return "#LF-OrgFlagWall_Refused";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` picks the flag at the wall, with whoever is online now, and is told in words.
	static LF_EOrgFlagWallResult PickBy(IEntity user, int flag)
	{
		if (!Replication.IsServer() || !user)
			return LF_EOrgFlagWallResult.BAD_FLAG;

		if (LF_Systems.Refuses(LF_OrgRules.SYSTEM, "gang flag wall"))
			return LF_EOrgFlagWallResult.BAD_FLAG;

		array<string> online = {};
		LF_OrgFlagRules.OnlineCharacterIds(online);
		string holderName;
		LF_EOrgFlagWallResult result = Pick(LF_Hideout.CharacterIdOf(user), flag, online, holderName);

		LF_ENotificationType type = LF_ENotificationType.INFO;
		if (result != LF_EOrgFlagWallResult.PICKED && result != LF_EOrgFlagWallResult.KEPT)
			type = LF_ENotificationType.WARNING;

		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(user), "#LF-Org_Title", MessageKey(result), 5.0, type, holderName);
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgFlagWall's statics (LF_StaticReset finds it by reflection).
class LF_OrgFlagWall_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgFlagWall";
	}

	override void Apply()
	{
		LF_OrgFlagWall.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Organizations", description: "Stands the sample flags of the registry flag wall")]
class LF_OrgFlagWallComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Stands the twenty sample flags on the wall, local to each screen (a sample is decoration; the actions
//! sit on the wall entity itself). A dedicated server stands none.
class LF_OrgFlagWallComponent : ScriptComponent
{
	protected static const int SPAWN_DELAY_MS = 500;	// lf-fixed: after the world finishes placing the wall

	protected ref array<IEntity> m_aSamples = {};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame() || !GetGame().InPlayMode() || RplSession.Mode() == RplMode.Dedicated)
			return;

		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(StandSamples, SPAWN_DELAY_MS, false, owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void StandSamples(IEntity owner)
	{
		if (!owner)
			return;

		int stood;
		for (int flag = 1; flag <= LF_OrgFlags.COUNT; flag++)
		{
			ResourceName prefab = LF_OrgFlagWall.SamplePrefab(flag);
			if (prefab.IsEmpty())
				continue;

			Resource resource = Resource.Load(prefab);
			if (!resource || !resource.IsValid())
				continue;

			EntitySpawnParams params();
			params.TransformMode = ETransformMode.LOCAL;
			params.Parent = owner;
			params.Transform[3] = LF_OrgFlagWall.SampleOffset(flag);
			params.Scale = LF_OrgFlagWall.SAMPLE_SCALE;
			IEntity sample = GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld(), params);
			if (!sample)
			{
				LF_Debug.Error(LF_OrgFlagWall.FEATURE, string.Format("sample flag %1 failed to spawn", flag));
				continue;
			}

			m_aSamples.Insert(sample);
			stood++;
		}

		LF_Debug.Info(LF_OrgFlagWall.FEATURE, string.Format("flag wall stands %1 of %2 samples", stood, LF_OrgFlags.COUNT));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(StandSamples);

		foreach (IEntity sample : m_aSamples)
		{
			if (sample)
				SCR_EntityHelper.DeleteEntityAndChildren(sample);
		}

		m_aSamples.Clear();
		super.OnDelete(owner);
	}
}
