//------------------------------------------------------------------------------------------------
//! One input of a recycle recipe: how many of a catalogue item the broken-down item is made of (#3347).
[BaseContainerProps()]
class LF_RecyclePart
{
	[Attribute("", UIWidgets.EditBox, "Catalogue item id of the part (scrap_metal)")]
	string m_sItem;

	[Attribute("1", UIWidgets.Auto, "How many of this part the item is made of", params: "1 1000")]
	int m_iCount;

	//------------------------------------------------------------------------------------------------
	static LF_RecyclePart Make(string item, int count)
	{
		LF_RecyclePart part = new LF_RecyclePart();
		part.m_sItem = item;
		part.m_iCount = count;
		return part;
	}
}

//------------------------------------------------------------------------------------------------
//! One recycle recipe: an item breaks into part of its inputs at a return rate (#3347). The key is `m_sItem`,
//! a catalogue item id.
[BaseContainerProps()]
class LF_RecycleRecipe : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Catalogue item id that is recycled (car_door). The record key")]
	string m_sItem;

	[Attribute("50", UIWidgets.Auto, "Percent of each part that comes back, rounded down. 100 or more is refused: always back less than went in", params: "1 100")]
	int m_iReturnPercent;

	[Attribute("", UIWidgets.Object, "What the item is made of; each part comes back at the return percent")]
	ref array<ref LF_RecyclePart> m_aParts;

	//------------------------------------------------------------------------------------------------
	static LF_RecycleRecipe Make(string item, int percent, notnull array<ref LF_RecyclePart> parts)
	{
		LF_RecycleRecipe recipe = new LF_RecycleRecipe();
		recipe.m_sItem = item;
		recipe.m_iReturnPercent = percent;
		recipe.m_aParts = {};
		foreach (LF_RecyclePart part : parts)
		{
			recipe.m_aParts.Insert(part);
		}

		return recipe;
	}
}

//------------------------------------------------------------------------------------------------
//! One line of what a recycle gives back.
class LF_RecycleReturn
{
	string m_sItem;
	int m_iCount;

	//------------------------------------------------------------------------------------------------
	static LF_RecycleReturn Make(string item, int count)
	{
		LF_RecycleReturn line = new LF_RecycleReturn();
		line.m_sItem = item;
		line.m_iCount = count;
		return line;
	}
}

//------------------------------------------------------------------------------------------------
//! The recycle recipes. Loaded from Configs/Processing/LF_Recycle.conf; the core ships it with no recipe
//! (a pack adds the recipes of its server). Every recipe is checked against the item catalogue at load:
//! a recipe whose parts are worth as much as the item is refused with the reason (#3347).
[BaseContainerProps(configRoot: true)]
class LF_RecycleConfig
{
	static const ResourceName CONFIG_PATH = "{F0B2DCA7A67C8BA3}Configs/Processing/LF_Recycle.conf";

	[Attribute("", UIWidgets.Object, "Recipes; an item with no row gives nothing back")]
	ref array<ref LF_RecycleRecipe> m_aRecipes;

	protected static ref LF_RecycleConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config with refused recipes removed, or null (logged once) when the file does not resolve.
	static LF_RecycleConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_RecycleRules.FEATURE, string.Format("recycle config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_RecycleConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(LF_RecycleRules.FEATURE, string.Format("recycle config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.Sanitize(LF_ItemCatalogueConfig.Get());
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` again (world end, tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_RecycleConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every recipe the rules refuse (LF_RecycleRules.Problem) with `LF_Debug.Error` naming the item
	//! and the reason, so one bad row never degrades the others. A repeated item keeps its first row. With no
	//! catalogue no price can be read, so every recipe is refused.
	//! \return How many rows were removed.
	int Sanitize(LF_ItemCatalogueConfig catalogue)
	{
		if (!m_aRecipes)
			m_aRecipes = {};

		int removed = 0;
		for (int i = m_aRecipes.Count() - 1; i >= 0; i--)
		{
			LF_RecycleRecipe recipe = m_aRecipes[i];
			string problem = LF_RecycleRules.Problem(recipe, catalogue);
			if (problem.IsEmpty() && recipe && FirstIndexOf(recipe.m_sItem) != i)
				problem = "the item is listed twice";

			if (problem.IsEmpty())
				continue;

			string name = "(empty)";
			if (recipe)
				name = recipe.m_sItem;

			LF_Debug.Error(LF_RecycleRules.FEATURE, string.Format("recycle recipe '%1' refused at load: %2", name, problem));
			m_aRecipes.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstIndexOf(string item)
	{
		for (int i = 0; i < m_aRecipes.Count(); i++)
		{
			if (m_aRecipes[i] && m_aRecipes[i].m_sItem == item)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The recipe of a catalogue item id, or null when it has none.
	LF_RecycleRecipe Find(string item)
	{
		if (!m_aRecipes || item.IsEmpty())
			return null;

		foreach (LF_RecycleRecipe recipe : m_aRecipes)
		{
			if (recipe && recipe.m_sItem == item)
				return recipe;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The recycle rules (#3347), pure logic with no world: an item breaks into part of its inputs, and a
//! recipe worth as much as the item at catalogue prices is refused so recycling is never a profit loop.
//! Prices are the catalogue's `m_iPrice` on both sides (the same prices the profit-loop check reads, #2848).
//! Called on the server by the recycler that comes with a later card.
class LF_RecycleRules
{
	static const string FEATURE = "Processing";

	//------------------------------------------------------------------------------------------------
	//! \return How many of `count` come back at `percent`, rounded down; never more than `count`.
	static int ReturnCount(int count, int percent)
	{
		if (count <= 0 || percent <= 0)
			return 0;

		if (percent >= 100)
			return count;

		return (count * percent) / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What a recipe gives back at catalogue prices, in cash, or -1 when a part has no catalogue price.
	static int ReturnedValue(notnull LF_RecycleRecipe recipe, LF_ItemCatalogueConfig catalogue)
	{
		int total = 0;
		if (!recipe.m_aParts)
			return total;

		foreach (LF_RecyclePart part : recipe.m_aParts)
		{
			if (!part)
				continue;

			LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, part.m_sItem);
			if (!item)
				return -1;

			total += item.m_iPrice * ReturnCount(part.m_iCount, recipe.m_iReturnPercent);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the recipe is usable, else why not: no item, a percent outside 1..99, no parts,
	//! an unknown item or part, a part count under 1, nothing coming back, or parts worth as much as the item.
	static string Problem(LF_RecycleRecipe recipe, LF_ItemCatalogueConfig catalogue)
	{
		if (!recipe)
			return "empty row";
		if (recipe.m_sItem.IsEmpty())
			return "no item";
		if (recipe.m_iReturnPercent < 1)
			return string.Format("return percent %1 is under 1", recipe.m_iReturnPercent);
		if (recipe.m_iReturnPercent >= 100)
			return string.Format("return percent %1 gives back all of it or more", recipe.m_iReturnPercent);
		if (!recipe.m_aParts || recipe.m_aParts.IsEmpty())
			return "no parts";
		if (!catalogue)
			return "no item catalogue to read prices from";

		LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, recipe.m_sItem);
		if (!item)
			return string.Format("item '%1' is not in the catalogue", recipe.m_sItem);

		int back = 0;
		foreach (LF_RecyclePart part : recipe.m_aParts)
		{
			if (!part)
				return "empty part";
			if (part.m_iCount < 1)
				return string.Format("part '%1' has count %2", part.m_sItem, part.m_iCount);

			LF_CatalogueItem partItem = LF_ItemCatalogue.Item(catalogue, part.m_sItem);
			if (!partItem)
				return string.Format("part '%1' is not in the catalogue", part.m_sItem);

			back += ReturnCount(part.m_iCount, recipe.m_iReturnPercent);
		}

		if (back < 1)
			return "nothing comes back at this return percent";

		int worth = ReturnedValue(recipe, catalogue);
		if (worth >= item.m_iPrice)
			return string.Format("parts worth %1 against the item's %2: recycling would not lose value", worth, item.m_iPrice);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! What recycling `itemId` gives back: one line per part that returns at least one. An item with no
	//! recipe gives nothing (the list stays empty).
	//! \return The number of lines added to `outReturns`.
	static int Yield(LF_RecycleConfig config, string itemId, notnull array<ref LF_RecycleReturn> outReturns)
	{
		if (!config)
			return 0;

		LF_RecycleRecipe recipe = config.Find(itemId);
		if (!recipe || !recipe.m_aParts)
			return 0;

		int added = 0;
		foreach (LF_RecyclePart part : recipe.m_aParts)
		{
			if (!part)
				continue;

			int count = ReturnCount(part.m_iCount, recipe.m_iReturnPercent);
			if (count < 1)
				continue;

			outReturns.Insert(LF_RecycleReturn.Make(part.m_sItem, count));
			added++;
		}

		return added;
	}
}
