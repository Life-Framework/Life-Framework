class EL_Test_Data : EL_Test
{
	override string GetName()
	{
		return "config-and-localization";
	}

	override void Run(EL_TestContext ctx)
	{
		array<string> menus = {
			"UI/Layouts/EL_SplitQuantityDialog.layout",
			"UI/Layouts/EL_CharacterCreationMenu.layout",
			"UI/Layouts/EL_ATMMenu.layout",
			"UI/Layouts/EL_SurvivalHUD.layout",
			"UI/Layouts/FactionSelectionMenu.layout",
			"UI/Layouts/ShopMenu.layout",
			"UI/Layouts/PoliceMenu.layout"
		};

		foreach (string layout : menus)
		{
			Resource res = Resource.Load(layout);
			ctx.True(res.IsValid(), string.Format("menu layout loads: %1", layout));
		}

		array<string> languages = {
			"Language/everonlife_localization.en_us.conf",
			"Language/everonlife_localization.de_de.conf",
			"Language/everonlife_localization.es_es.conf",
			"Language/everonlife_localization.fr_fr.conf",
			"Language/everonlife_localization.it_it.conf",
			"Language/everonlife_localization.pt_pt.conf",
			"Language/everonlife_localization.pt_br.conf"
		};

		foreach (string lang : languages)
		{
			Resource res = Resource.Load(lang);
			ctx.True(res.IsValid(), string.Format("localization loads: %1", lang));
		}
	}
};