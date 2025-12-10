class EL_PropertyMenu : ChimeraMenuBase
{
    protected EL_PropertyComponent m_PropertyComponent;
    protected Widget m_RootWidget;
    protected ButtonWidget m_LockButton;
    protected ButtonWidget m_AddFurnitureButton;
    protected EditBoxWidget m_FurnitureInput;
    protected ButtonWidget m_CloseButton;
    protected VerticalLayoutWidget m_FurnitureList;

    // Constructor
    void EL_PropertyMenu()
    {
    }

    // Get layout
    string GetLayoutFile()
    {
        return "{5A60AE2B8D435CE9}UI/Layouts/PropertyMenu.layout";
    }

    // Override to initialize
    override void OnMenuInit()
    {
        m_RootWidget = GetRootWidget();

        // Setup lock button
        m_LockButton = ButtonWidget.Cast(m_RootWidget.FindWidget("LockButton"));
        if (m_LockButton)
        {
            TextWidget buttonText = TextWidget.Cast(m_LockButton.FindAnyWidget("LockButtonText"));
            if (buttonText)
            {
                if (m_PropertyComponent && m_PropertyComponent.IsLocked())
                {
                    buttonText.SetText("#EL-Property_Unlock");
                }
                else
                {
                    buttonText.SetText("#EL-Property_Lock");
                }
            }
            // Clicks handled in OnClick override
        }

        // Setup add furniture button
        m_AddFurnitureButton = ButtonWidget.Cast(m_RootWidget.FindWidget("AddFurnitureButton"));
        // Add furniture clicks handled in OnClick override

        // Setup furniture input
        m_FurnitureInput = EditBoxWidget.Cast(m_RootWidget.FindWidget("FurnitureInput"));
        // No event needed, will read on button click

        // Setup close button
        m_CloseButton = ButtonWidget.Cast(m_RootWidget.FindWidget("CloseButton"));
        // Close clicks handled in OnClick override

        // Setup furniture list
        m_FurnitureList = VerticalLayoutWidget.Cast(m_RootWidget.FindWidget("FurnitureList"));
        UpdateFurnitureList();
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_LockButton)
        {
            OnLockButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_AddFurnitureButton)
        {
            OnAddFurnitureButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_CloseButton)
        {
            OnCloseButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        return false;
    }

    // Lock/Unlock button clicked
    void OnLockButtonClicked(ButtonWidget button)
    {
        m_PropertyComponent.ToggleLock();
        TextWidget buttonText = TextWidget.Cast(button.FindAnyWidget("LockButtonText"));
        if (buttonText)
        {
            if (m_PropertyComponent.IsLocked())
            {
                buttonText.SetText("#EL-Property_Unlock");
            }
            else
            {
                buttonText.SetText("#EL-Property_Lock");
            }
        }
        Print("Property lock toggled!");
    }

    // Add furniture button clicked
    void OnAddFurnitureButtonClicked(ButtonWidget button)
    {
        if (m_FurnitureInput)
        {
            string furnitureName = m_FurnitureInput.GetText();
            if (furnitureName != "")
            {
                // TODO: Add furniture to property (need to implement in component)
                Print("Adding furniture: " + furnitureName);
                m_FurnitureInput.SetText("");
                UpdateFurnitureList();
            }
        }
    }

    // Close button clicked
    void OnCloseButtonClicked(ButtonWidget button)
    {
        Close();
    }

    // Update furniture list display
    void UpdateFurnitureList()
    {
        if (!m_FurnitureList)
            return;

        // Clear existing children
        Widget child = m_FurnitureList.GetChildren();
        while (child)
        {
            Widget next = child.GetSibling();
            child.RemoveFromHierarchy();
            child = next;
        }

        // TODO: Get furniture from property component and add widgets
        // For now, placeholder
        Widget placeholder = GetGame().GetWorkspace().CreateWidgets("{5FD0C2A07AEC0F1C}UI/Layouts/Common/Text.layout", m_FurnitureList);
        TextWidget textWidget = TextWidget.Cast(placeholder.FindAnyWidget("Text"));
        if (textWidget)
            textWidget.SetText("No furniture yet");
    }

    // Set property component
    void SetPropertyComponent(EL_PropertyComponent comp)
    {
        m_PropertyComponent = comp;
    }

    // TODO: Add methods for adding/removing furniture
}