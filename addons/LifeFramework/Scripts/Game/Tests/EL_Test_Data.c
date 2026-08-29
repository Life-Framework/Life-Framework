// red-proof: append a nonexistent path (e.g. "UI/Layouts/Missing.layout") to
// the layout list and run `tools\cli test --tier all`. Red run pending: first
// boot after the EPF dependency removal lands.
// tier: WORLD
class EL_Test_Data : EL_Test
{
	override string GetName()
	{
		return "config-and-localization";
	}

	override void Run(EL_TestContext ctx)
	{
		array<string> menus = {
			"{CBD13209101AACEE}UI/Layouts/Menus/Inventory/SplitQuantityDialog.layout",
			"{D9C065A9AD23B91B}UI/Layouts/EL_CharacterCreationMenu.layout",
			"{8012E7D806E87F1A}UI/Layouts/EL_ATMMenu.layout",
			"{64B1F5DD29214016}UI/Layouts/EL_SurvivalHUD.layout",
			"{41489243B750D7EC}UI/Layouts/FactionSelectionMenu.layout",
			"{CE3AB8A674B242C5}UI/Layouts/ShopMenu.layout",
			"{41C6F92AE5EA775C}UI/Layouts/PoliceMenu.layout"
		};

		foreach (string layout : menus)
		{
			Resource res = Resource.Load(layout);
			ctx.True(res.IsValid(), string.Format("menu layout loads: %1", layout));
		}

		array<string> languages = {
			"{55C94506A4DE38D4}Language/everonlife_localization.en_us.conf",
			"{22C3B3E7B5DE4F90}Language/everonlife_localization.de_de.conf",
			"{218D5638D4114435}Language/everonlife_localization.es_es.conf",
			"{0F28012222AD49A9}Language/everonlife_localization.fr_fr.conf",
			"{8D37A87656314392}Language/everonlife_localization.it_it.conf",
			"{DEFFC499E7D84576}Language/everonlife_localization.pt_pt.conf",
			"{7D2C51510B084418}Language/everonlife_localization.pt_br.conf"
		};

		foreach (string lang : languages)
		{
			Resource res = Resource.Load(lang);
			ctx.True(res.IsValid(), string.Format("localization loads: %1", lang));
		}
	}
};