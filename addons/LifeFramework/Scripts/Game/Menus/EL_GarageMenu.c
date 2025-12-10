class EL_GarageMenu : ChimeraMenuBase
{
    protected Widget m_RootWidget;
    protected EL_GarageComponent m_GarageComponent;
    protected VerticalLayoutWidget m_VehicleList;
    protected ButtonWidget m_StoreButton;
    protected ButtonWidget m_CloseButton;
    protected EditBoxWidget m_VehicleInput;
    protected TextWidget m_VehicleText;

    //------------------------------------------------------------------------------------------------
    override void OnMenuInit()
    {
        m_RootWidget = GetRootWidget();

        // Setup vehicle list
        m_VehicleList = VerticalLayoutWidget.Cast(m_RootWidget.FindWidget("VehicleList"));
        UpdateVehicleList();

        // Setup store button
        m_StoreButton = ButtonWidget.Cast(m_RootWidget.FindWidget("StoreButton"));
        // Button events handled in OnClick override

        // Setup vehicle input
        m_VehicleInput = EditBoxWidget.Cast(m_RootWidget.FindWidget("VehicleInput"));

        // Setup close button
        m_CloseButton = ButtonWidget.Cast(m_RootWidget.FindWidget("CloseButton"));
        // Button events handled in OnClick override
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_StoreButton)
        {
            OnStoreButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_CloseButton)
        {
            OnCloseButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        return false;
    }

    // Store button clicked
    void OnStoreButtonClicked(ButtonWidget button)
    {
        if (m_VehicleInput && m_GarageComponent)
        {
            string vehicleType = m_VehicleInput.GetText();
            if (vehicleType != "")
            {
                // TODO: Get vehicle from player and store it
                EL_VehicleData vehicleData = new EL_VehicleData(vehicleType, vehicleType, m_GarageComponent.GetStorageFee());
                if (m_GarageComponent.StoreVehicle(vehicleData, GetPlayerUID()))
                {
                    Print("Vehicle stored: " + vehicleType);
                    m_VehicleInput.SetText("");
                    UpdateVehicleList();
                }
                else
                {
                    Print("Failed to store vehicle");
                }
            }
        }
    }

    // Close button clicked
    void OnCloseButtonClicked(ButtonWidget button)
    {
        Close();
    }

    // Update vehicle list display
    void UpdateVehicleList()
    {
        if (!m_VehicleList || !m_GarageComponent)
            return;

        // Clear existing children
        Widget child = m_VehicleList.GetChildren();
        while (child)
        {
            Widget next = child.GetSibling();
            child.RemoveFromHierarchy();
            child = next;
        }

        array<ref EL_VehicleData> vehicles = m_GarageComponent.GetStoredVehicles();
        for (int i = 0; i < vehicles.Count(); i++)
        {
            EL_VehicleData vehicle = vehicles.Get(i);
            if (vehicle)
            {
                Widget vehicleText = GetGame().GetWorkspace().CreateWidgets("{5FD0C2A07AEC0F1C}UI/Layouts/Common/Text.layout", m_VehicleList);
                TextWidget textWidget = TextWidget.Cast(vehicleText.FindAnyWidget("Text"));
                if (textWidget)
                    textWidget.SetText(vehicle.GetDisplayName() + " - Retrieve");
            }
        }

        if (vehicles.Count() == 0)
        {
            Widget placeholder = GetGame().GetWorkspace().CreateWidgets("{5FD0C2A07AEC0F1C}UI/Layouts/Common/Text.layout", m_VehicleList);
            TextWidget textWidget = TextWidget.Cast(placeholder.FindAnyWidget("Text"));
            if (textWidget)
                textWidget.SetText("No vehicles stored");
        }
    }

    // Vehicle button clicked (retrieve)
    void OnVehicleButtonClicked(ButtonWidget button)
    {
        // TODO: Implement retrieve logic
        // For now, just print
        Print("Vehicle retrieve clicked");
    }

    // Set garage component
    void SetGarageComponent(EL_GarageComponent comp)
    {
        m_GarageComponent = comp;
    }

    // Get player UID (placeholder)
    protected string GetPlayerUID()
    {
        // TODO: Get from player controller
        return "player_uid_placeholder";
    }
};