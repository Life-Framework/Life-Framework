// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: appending
// "{0000000000000001}UI/Layouts/Missing.layout" to the layout list failed it with
// `[LFTEST] FAIL config-and-localization`,
// `- menu layout loads: {0000000000000001}UI/Layouts/Missing.layout`. Restored; green.
// tier: WORLD
class LF_Test_Data : LF_Test
{
	override string GetName()
	{
		return "config-and-localization";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> menus = {
			"{CBD13209101AACEE}UI/Layouts/Menus/Inventory/SplitQuantityDialog.layout",
			"{64B1F5DD29214016}UI/Layouts/EL_SurvivalHUD.layout",
			"{B1C08A9DC82E6FEC}UI/Layouts/UIKit/LF_UIKeyRow.layout",
			"{41C6F92AE5EA775C}UI/Layouts/PoliceMenu.layout",
			"{1CBC0B1329A8762C}UI/Layouts/DeathScreen.layout"
		};

		foreach (string layout : menus)
		{
			Resource res = Resource.Load(layout);
			ctx.True(res != null && res.IsValid(), string.Format("menu layout loads: %1", layout));
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
			ctx.True(res != null && res.IsValid(), string.Format("localization loads: %1", lang));
		}
	}
};