// red-proof: perturb LF_PlayerAccount.SetActiveCharacter (store Find result
// unconditionally) or RemoveCharacter (drop the re-clamp), then run the fast
// tier; the affected account/active-index-security assertions go red.

// tier: LOGIC
class LF_Test_AccountActiveIndex : LF_Test
{
	override string GetName()
	{
		return "account/active-index-security";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccount.Create("test-active-index-security");
		LF_PlayerCharacter a = LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "John", "Doe", 30);
		LF_PlayerCharacter b = LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Jane", "Roe", 28);
		LF_PlayerCharacter outsider = LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Out", "Sider", 40);

		account.AddCharacter(a, true);
		account.AddCharacter(b);
		ctx.True(account.GetActiveCharacter() == a, "a is active after add");

		account.SetActiveCharacter(outsider);
		ctx.True(account.GetActiveCharacter() == a, "SetActiveCharacter with a non-member leaves the active character unchanged");

		account.RemoveCharacter(a);
		ctx.Equal(1, account.GetCharacters().Count(), "removing the active character shrinks the roster");
		ctx.True(account.GetActiveCharacter() == b, "removing the active character falls back to the first remaining");

		account.RemoveCharacter(b);
		ctx.False(account.HasCharacters(), "roster is empty after removing the last character");
		ctx.True(account.GetActiveCharacter() == null, "empty roster yields a null active character, no out-of-bounds read");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_AccountCachePolicy : LF_Test
{
	override string GetName()
	{
		return "account/cache-keep-resident";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccountManager.Reset();
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();

		LF_PlayerAccount account = LF_PlayerAccount.Create("test-cache-resident");
		account.SetWantedLevel(3);
		manager.AddToCache(account);

		manager.SaveAndReleaseAccount(account);

		LF_PlayerAccount cached = manager.GetFromCache("test-cache-resident");
		ctx.NotNull(cached, "account stays cached after SaveAndReleaseAccount");
		if (!cached)
			return;
		ctx.True(cached == account, "cached account is the same instance after save-and-release");
		ctx.Equal(3, cached.GetWantedLevel(), "wanted level survives a save-and-release");

		LF_PlayerAccountManager.Reset();
	}
};