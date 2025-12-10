class EL_FurnitureShopMenu : ChimeraMenuBase
{
    protected EL_FurnitureShopComponent m_ShopComponent;
    protected Widget m_RootWidget;

    // Get layout
    string GetLayoutFile()
    {
        return "{5A60AE2B8D435CE9}UI/Layouts/FurnitureShopMenu.layout";
    }

    // Override to initialize
    override void OnMenuInit()
    {
        m_RootWidget = GetRootWidget();

        // Setup buy buttons
        ButtonWidget buyChairButton = ButtonWidget.Cast(m_RootWidget.FindWidget("BuyChairButton"));
        // Handled by OnClick override

        ButtonWidget buyTableButton = ButtonWidget.Cast(m_RootWidget.FindWidget("BuyTableButton"));
        // Handled by OnClick override

        ButtonWidget buyBedButton = ButtonWidget.Cast(m_RootWidget.FindWidget("BuyBedButton"));
        // Handled by OnClick override

        // Setup close button
        ButtonWidget closeButton = ButtonWidget.Cast(m_RootWidget.FindWidget("CloseButton"));
        // Handled by OnClick override
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        // Button lookups by widget references
        ButtonWidget b = ButtonWidget.Cast(w);
        if (!b)
            return false;

        // Compare with widgets by name (safer than relying on captured locals)
        string name = b.GetName();
        if (name == "BuyChairButton")
        {
            OnBuyChairClicked(b);
            return true;
        }

        if (name == "BuyTableButton")
        {
            OnBuyTableClicked(b);
            return true;
        }

        if (name == "BuyBedButton")
        {
            OnBuyBedClicked(b);
            return true;
        }

        if (name == "CloseButton")
        {
            OnCloseButtonClicked(b);
            return true;
        }

        return false;
    }

    // Buy Chair
    void OnBuyChairClicked(ButtonWidget button)
    {
        BuyFurniture("Chair", 500);
    }

    // Buy Table
    void OnBuyTableClicked(ButtonWidget button)
    {
        BuyFurniture("Table", 1000);
    }

    // Buy Bed
    void OnBuyBedClicked(ButtonWidget button)
    {
        BuyFurniture("Bed", 2000);
    }

    // Close button clicked
    void OnCloseButtonClicked(ButtonWidget button)
    {
        Close();
    }

    // Buy furniture logic
    void BuyFurniture(string name, int price)
    {
        // Try to perform purchase via shop component if available
        if (m_ShopComponent)
        {
            // Placeholder for player UID retrieval - use local controlled entity
            IEntity playerEnt = SCR_PlayerController.GetLocalControlledEntity();
            string playerUID = "";
            if (playerEnt)
                playerUID = playerEnt.GetName(); // TODO: replace with proper UID retrieval

            if (m_ShopComponent.BuyFurniture(name, playerUID))
            {
                Print("Furniture purchased: " + name + " for $" + price.ToString());
                return;
            }
            else
            {
                Print("Purchase failed for " + name);
                return;
            }
        }

        // Fallback behaviour
        Print("Buying " + name + " for $" + price.ToString());
    }

    //------------------------------------------------------------------------------------------------
    // Allow actions to set the shop component
    void SetShopComponent(EL_FurnitureShopComponent comp)
    {
        m_ShopComponent = comp;
        // Optionally refresh UI or pre-fill values here
    }
}