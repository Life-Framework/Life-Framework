// red-proof: RED-OK 2026-11-17, `tools/cli test --no-build --red LF_Test_BanRecordCharacters,...` (hook asks with the account uid only, as the unfixed code did): `a ban lists the incidents of both characters, as either party` failed (1 of 3 listed); restored, green.

// tier: LOGIC
//! moderation/ban-record-characters (#2414): incidents are stored under the key of the character that
//! was active, which is not the account uid for any character after the first. A ban names the
//! account, so the incidents it lists are those of every character of that account.
class LF_Test_BanRecordCharacters : LF_Test
{
	protected static const string UID = "ban-chars-account";
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	override string GetName()
	{
		return "moderation/ban-record-characters";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		accounts.AddToCache(account);
		LF_PlayerCharacter first = LF_PlayerCharacter.Create(PREFAB, "Ada", "Lovelace", 36);
		account.AddCharacter(first, true);
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Bob", "Builder", 40);
		account.AddCharacter(second, true);
		string secondKey = second.GetRecordKey();
		ctx.False(secondKey == UID, "the second character has a key of its own");

		int now = System.GetUnixTime();
		LF_IncidentRecord asFirst = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, now - 90, "ban-chars-victim", UID, vector.Zero, 0, "rifle", true);
		LF_IncidentRecord asSecond = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, now - 60, "ban-chars-victim", secondKey, vector.Zero, 20, "rifle", false);
		LF_IncidentRecord asSecondSubject = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DOWNED, now - 30, secondKey, "ban-chars-other", vector.Zero, 0, "", false);
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, now - 20, "someone-else", "nobody", vector.Zero, 0, "rifle", false);
		ctx.True(asFirst && asSecond && asSecondSubject, "the incidents were logged");
		if (!asFirst || !asSecond || !asSecondSubject)
			return;

		string askedWith = UID;
		array<int> ids;
		if (RedProof())
			ids = LF_IncidentLog.RecentNumbersFor(askedWith, LF_BanRecord.INCIDENT_MAX);
		else
			ids = LF_BanRecord.RecentIncidentIds(askedWith);

		ctx.Equal(3, ids.Count(), "a ban lists the incidents of both characters, as either party");
		if (ids.Count() == 3)
		{
			ctx.Equal(asFirst.m_iNumber, ids[0], "the first character's incident is listed");
			ctx.Equal(asSecond.m_iNumber, ids[1], "the second character's incident as the other party is listed");
			ctx.Equal(asSecondSubject.m_iNumber, ids[2], "and as the subject, oldest first");
		}

		// The same through the command path: a ban typed with the account uid.
		ScrServerCmdResult pendingResult = new ScrServerCmdResult("", EServerCmdResultType.PENDING);
		array<string> createArgv = {"ban", "create", UID, "600", "RDM"};
		LF_BanPending pending = LF_BanRecord.Prepare(createArgv, "ban-chars-staff", pendingResult);
		ctx.True(pending != null, "the ban is held until the backend answers");
		if (pending)
			ctx.Equal(3, pending.m_aIncidentIds.Count(), "the held ban names the incidents of both characters");

		// A key typed as the second character's key finds the same account.
		array<string> keys = LF_PlayerAccountManager.RecordKeysOf(secondKey);
		ctx.True(keys.Find(UID) >= 0 && keys.Find(secondKey) >= 0, "a character key names every key of its account");
		ctx.Equal(1, LF_PlayerAccountManager.RecordKeysOf("ban-chars-unknown").Count(), "an unknown id is just itself");
		ctx.Equal(0, LF_PlayerAccountManager.RecordKeysOf("").Count(), "an empty id has no keys");

		accounts.RemoveFromCache(UID);
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
