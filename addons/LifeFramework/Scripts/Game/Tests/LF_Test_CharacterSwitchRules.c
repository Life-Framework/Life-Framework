// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_CharacterSwitchRules,LF_Test_CharacterIsolation` (one batch with the pack test): RED-OK, with the hook skipping each flag, the hit, the grant and the pack source it failed `- a prisoner serves the sentence first (expected '#LF-CharSwitch_Jailed', got '')`, `- no switch out of a fight`, `- the clerk's yes opens the sheet`, `- the pack overlay raises the limit to 3 (expected 3, got 1)`; green without it.
//! Changing character at the clerk (#331 made real): the switch is refused, each with its own reason,
//! while the character is dead, jailed, cuffed, downed, wanted, on duty, in a fight, with contraband,
//! on a job run, in a race or in a vehicle, and when the server keeps one character per account. The
//! fight window follows player-on-player hits; the clerk's yes is good once and only for a short
//! while; a switch pick names another existing character or a free new slot; the sheet in switch
//! mode marks the character being played and offers "stay". A pack overlay of LF_CharacterConfig
//! sets the character limit (the Everon pack allows 3).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CharacterSwitchRules : LF_Test
{
	static const ResourceName OVERLAY = "{D0AEA2948CC64E76}Configs/Tests/LF_Test_CharactersOverlay.conf";
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const string UID = "csr-account-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "character/switch-rules";
	}

	//------------------------------------------------------------------------------------------------
	//! A state a switch is allowed from. Under the red proof every refusal below is built from it.
	protected LF_CharacterSwitchState Clear()
	{
		LF_CharacterSwitchState state = new LF_CharacterSwitchState();
		state.m_bMultiAllowed = true;
		state.m_bHasCharacter = true;
		state.m_bAlive = true;
		return state;
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectRefused(LF_TestContext ctx, LF_CharacterSwitchState state, string key, string why)
	{
		ctx.EqualStr(key, LF_CharacterSwitch.Refuse(state), why);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		bool act = !RedProof();
		ctx.EqualStr("", LF_CharacterSwitch.Refuse(Clear()), "a living, free character may switch");

		LF_CharacterSwitchState state = Clear();
		if (act)
			state.m_bMultiAllowed = false;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Off", "one character per account: nothing to switch to");

		state = Clear();
		if (act)
			state.m_bAlive = false;
		ExpectRefused(ctx, state, "#LF-CharSel_Refused", "a dead character does not switch (death has its own respawn)");

		state = Clear();
		if (act)
			state.m_bJailed = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Jailed", "a prisoner serves the sentence first");

		state = Clear();
		if (act)
			state.m_bCuffed = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Cuffed", "cuffed or escorted is custody");

		state = Clear();
		if (act)
			state.m_bDowned = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Downed", "a downed character does not switch away");

		state = Clear();
		if (act)
			state.m_iWanted = 1;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Wanted", "a wanted character settles the record first");

		state = Clear();
		if (act)
			state.m_bOnDuty = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_OnDuty", "only the character on duty carries the duty kit");

		state = Clear();
		if (act)
			state.m_bInCombat = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Combat", "no switch out of a fight");

		state = Clear();
		if (act)
			state.m_bContraband = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Contraband", "no switch with contraband");

		state = Clear();
		if (act)
			state.m_bJobRun = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_JobRun", "no switch during a job run");

		state = Clear();
		if (act)
			state.m_bRacing = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Racing", "no switch during a race");

		state = Clear();
		if (act)
			state.m_bInVehicle = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Vehicle", "no switch from inside a vehicle");

		// Custody outranks the rest: a jailed and wanted prisoner hears about the sentence.
		state = Clear();
		state.m_iWanted = 3;
		if (act)
			state.m_bJailed = true;
		ExpectRefused(ctx, state, "#LF-CharSwitch_Jailed", "the sentence is named before the warrant");

		// The fight window.
		ctx.True(LF_CharacterSwitch.IsInCombat(100, 150, 120), "50 s after a hit is still a fight");
		ctx.False(LF_CharacterSwitch.IsInCombat(100, 221, 120), "121 s after a hit the fight is over");
		ctx.False(LF_CharacterSwitch.IsInCombat(-1, 150, 120), "never hit is no fight");
		ctx.False(LF_CharacterSwitch.IsInCombat(100, 101, 0), "a window of 0 turns the rule off");

		// The account and the hit log feed the state.
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		accounts.AddToCache(account);
		account.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Ada", "Lovelace", 36), true);
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Bob", "Builder", 40);
		account.AddCharacter(second, true);
		if (act)
		{
			account.SetWantedLevel(2);
			LF_CharacterSwitch.NoteHit(second.GetRecordKey(), LF_CharacterSwitch.Now());
		}

		LF_CharacterSwitchState read = LF_CharacterSwitch.ReadStateFor(new LF_CharacterSwitchState(), account, second.GetRecordKey(), null);
		ctx.True(read.m_bHasCharacter, "the account's active character is read");
		ctx.Equal(2, read.m_iWanted, "the active character's wanted level is read");
		ctx.True(read.m_bInCombat, "a hit just now puts the character in a fight");
		ctx.False(read.m_bAlive, "no body is not a living character");
		LF_CharacterSwitchState other = LF_CharacterSwitch.ReadStateFor(new LF_CharacterSwitchState(), account, "csr-somebody-else", null);
		ctx.False(other.m_bInCombat, "another character's hit is not this one's fight");

		// The clerk's yes.
		const int playerId = 9341;
		ctx.False(LF_CharacterSwitch.ConsumeGrant(playerId, 10), "no sheet without the clerk");
		if (act)
			LF_CharacterSwitch.Grant(playerId, 10);
		ctx.True(LF_CharacterSwitch.ConsumeGrant(playerId, 15), "the clerk's yes opens the sheet");
		ctx.False(LF_CharacterSwitch.ConsumeGrant(playerId, 16), "the yes is good once");
		LF_CharacterSwitch.Grant(playerId, 10);
		ctx.False(LF_CharacterSwitch.ConsumeGrant(playerId, 10 + LF_CharacterSwitch.GRANT_SECONDS + 1), "a stale yes opens nothing");
		LF_CharacterSwitch.Grant(playerId, 10);
		LF_CharacterSwitch.Forget(playerId);
		ctx.False(LF_CharacterSwitch.ConsumeGrant(playerId, 11), "a leaver's yes goes with the connection");

		// The slot a switch pick names.
		LF_CharacterConfig.SetMaxPerAccountOverride(3);
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseSwitchSlot(account, 0), "another existing character can be picked");
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseSwitchSlot(account, LF_CharacterSelectSheet.SLOT_NEW), "a new resident while a slot is free");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefuseSwitchSlot(account, 5), "a slot past the roster is refused");
		LF_CharacterConfig.SetMaxPerAccountOverride(2);
		ctx.EqualStr("#LF-Char_Error_Full", LF_CharacterCreationRules.RefuseSwitchSlot(account, LF_CharacterSelectSheet.SLOT_NEW), "no new resident when the slots are full");
		LF_CharacterConfig.SetMaxPerAccountOverride(0);

		// The sheet in switch mode: title, intro, the played one marked, the other a button, new, stay.
		array<ref LF_UIPart> parts = LF_CharacterSelectSheet.BuildParts("Ada Lovelace\nBob Builder", true, 1);
		ctx.Equal(6, parts.Count(), "switch sheet: title, intro, two characters, new, stay");
		if (parts.Count() == 6)
		{
			ctx.EqualStr("#LF-CharSwitch_Title", parts[0].GetText(), "switch sheet has its own title");
			ctx.EqualStr(LF_CharacterSelectSheet.PART_SLOT + "0", parts[2].GetName(), "the other character is a button");
			ctx.EqualStr(LF_CharacterSelectSheet.PART_PLAYING, parts[3].GetName(), "the character being played is marked, not a button");
			ctx.EqualStr(LF_CharacterSelectSheet.PART_STAY, parts[5].GetName(), "the last line is stay as you are");
		}

		ctx.Equal(5, LF_CharacterSelectSheet.BuildParts("Ada Lovelace\nBob Builder", true).Count(), "the arrival sheet has no stay line");
		ctx.True(LF_CharacterSelectSheet.SlotOfPart(LF_CharacterSelectSheet.PART_PLAYING) < LF_CharacterSelectSheet.SLOT_NEW, "the marked line is no pick");

		// The character limit from a pack overlay.
		LF_ConfigService.Reset();
		LF_CharacterConfig.Reload();
		ctx.Equal(1, LF_CharacterConfig.MaxPerAccount(), "without a pack the core allows one character");
		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (act)
			LF_ConfigService.SetPackSource(source);

		LF_CharacterConfig.Reload();
		ctx.Equal(3, LF_CharacterConfig.MaxPerAccount(), "the pack overlay raises the limit to 3");
		ctx.Equal(45, LF_CharacterConfig.SwitchCombatSeconds(), "the pack overlay sets the fight window");
		ctx.Equal(18, LF_CharacterConfig.MinAge(), "a field the overlay leaves alone stays core");

		LF_ConfigService.Reset();
		LF_CharacterConfig.Reload();
		LF_CharacterSwitch.ResetStatics();
		accounts.RemoveFromCache(UID);
	}
}
