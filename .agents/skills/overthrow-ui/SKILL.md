---
name: overthrow-ui
description: Overthrow-specific UI development patterns using OVT_UIContext, OVT_UIManagerComponent, layout files, widget interaction, and UI-system integration
version: 1.0.0
---

# Overthrow UI Development

Quick reference for Overthrow mod UI patterns. For interaction actions and input binding, see `reforger-actions-ui` skill.

---

## When to Use This Skill

Use this skill when:
- Creating new UI screens/contexts extending `OVT_UIContext`
- Registering UI contexts with `OVT_UIManagerComponent` on the player prefab
- Building `.layout` files in the Workbench UI editor
- Accessing widgets, binding events, updating UI state
- Integrating UI with manager components via `OVT_Global`
- Communicating from UI to server via `OVT_OverthrowController`

---

## Quick Reference

### UI Context Pattern
Every screen extends `OVT_UIContext` and defines a `.layout` file. Lifecycle: `OnCreate` → `OnShow` → `OnHide`.

### UI Manager
`OVT_UIManagerComponent` on the player entity manages all contexts. Access via `OVT_Global.GetUI()`.

### Layout Files
Created in Arma Reforger Workbench, stored in `UI/layouts/`. Contain widget hierarchies, styles, and event bindings.

### Widget Interaction
Find via `GetRootWidget().FindAnyWidget("name")`, cast to specific type, bind events via `m_OnClicked.Insert(callback)`.

### Server Communication
UI contexts talk to server via components on `OVT_OverthrowController` — see `overthrow-networking` skill.

---

## Architecture

```
OVT_UIManagerComponent (on player entity)
├── Manages multiple OVT_UIContext instances
├── ShowContext(typename) — activates a context
├── GetContext(typename) — retrieves a context
└── Coordinates layout lifecycle (create/show/hide)

OVT_UIContext (base class)
├── ResourceName m_Layout — .layout file reference
├── OnCreate() — one-time initialization
├── OnShow() — called when context becomes visible
├── OnHide() — cleanup when hidden
└── Widget access via GetRootWidget().FindAnyWidget()

OVT_Global
├── GetUI() → OVT_UIManagerComponent
├── GetController() → OVT_OverthrowController (for server comms)
└── GetPlayers(), GetShops(), etc. → manager singletons
```

---

## UI Context Pattern

### Basic Context Structure

```cpp
//! UI context for [feature description].
class OVT_ExampleContext : OVT_UIContext
{
    //! Layout file created in Workbench.
    override ResourceName GetLayout()
    {
        return "{F123456789}UI/layouts/Example.layout";
    }

    //! One-time initialization. Called when context is first created.
    override void OnCreate()
    {
        super.OnCreate();

        // Find and bind widgets
        ButtonWidget button = ButtonWidget.Cast(
            GetRootWidget().FindAnyWidget("Button_Confirm"));
        if (button)
            button.m_OnClicked.Insert(OnConfirmClicked);
    }

    //! Called each time the context becomes visible.
    override void OnShow()
    {
        super.OnShow();
        RefreshDisplay();
    }

    //! Called when context is hidden. Clean up resources.
    override void OnHide()
    {
        super.OnHide();
        // Release any temporary data
    }

    protected void OnConfirmClicked()
    {
        // Handle button click
    }

    protected void RefreshDisplay()
    {
        TextWidget text = TextWidget.Cast(
            GetRootWidget().FindAnyWidget("Text_Title"));
        if (text)
            text.SetText("Hello World");
    }
}
```

### Lifecycle

| Method | When | Purpose |
|---|---|---|
| `OnCreate()` | Context first created | Find widgets, bind events, one-time setup |
| `OnShow()` | Each time context shown | Refresh data from managers, update display |
| `OnHide()` | Each time context hidden | Clean temporary state, unbind if needed |

---

## Activating UI Contexts

```cpp
// Get the UI manager
OVT_UIManagerComponent ui = OVT_Global.GetUI();
if (!ui)
    return;

// Get or find the context
OVT_ShopContext context = OVT_ShopContext.Cast(
    ui.GetContext(OVT_ShopContext));
if (!context)
    return;

// Optional: configure context with data before showing
context.SetShop(shop);

// Show the context (triggers OnShow)
ui.ShowContext(OVT_ShopContext);
```

**Note:** `GetContext()` returns cached instance. `ShowContext()` activates it. If the context was hidden, `OnShow()` fires again.

---

## UI Context Registration

Contexts are registered on the `SPT_UIManagerComponent` (or `OVT_UIManagerComponent` in Overthrow) via the `m_aContexts` array attribute on the player prefab:

```cpp
class SPT_UIManagerComponentClass : ScriptComponentClass {};

class SPT_UIManagerComponent : ScriptComponent
{
    [Attribute("", UIWidgets.Object)]
    ref array<ref SPT_UIContext> m_aContexts;
    // ...
};
```

Contexts are listed in the Workbench component editor. Each entry points to the compiled context class resource.

---

## Widget Interaction

```cpp
// Find a widget by name (set in layout editor)
Widget widget = GetRootWidget().FindAnyWidget("WidgetName");

// Cast to specific type
ButtonWidget button = ButtonWidget.Cast(widget);
TextWidget text = TextWidget.Cast(widget);
ProgressBarWidget progress = ProgressBarWidget.Cast(widget);

// Set properties
button.SetText("Click Me");
text.SetText("Hello World");

// Bind events
button.m_OnClicked.Insert(OnButtonClicked);

// List/Grid patterns
SCR_ListBoxComponent listBox = SCR_ListBoxComponent.Cast(
    widget.FindHandler(SCR_ListBoxComponent));
if (listBox)
{
    listBox.Clear();
    foreach (ItemData item : items)
    {
        listBox.AddItem(item.GetName(), item);
    }
}

// Progress bar
progress.SetCurrent(currentValue);
progress.SetMax(maxValue);
```

---

## UI-System Integration

### Accessing Manager Components

UI contexts read data from manager singletons via `OVT_Global`:

```cpp
OVT_ShopManagerComponent shopManager = OVT_Global.GetShops();
OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
```

### Server Communication

UI contexts send requests to the server via components on `OVT_OverthrowController`:

```cpp
OVT_OverthrowController controller = OVT_Global.GetController();
if (!controller)
    return;

OVT_ShopServerComponent shopServer = OVT_ShopServerComponent.Cast(
    controller.FindComponent(OVT_ShopServerComponent));
if (!shopServer)
    return;

shopServer.PurchaseItem(itemId, quantity);
```

See `overthrow-networking` skill for the complete client→server pattern.

---

## File Organization

| Directory | Purpose |
|---|---|
| `Scripts/Game/UI/` | UI context classes (C++) |
| `UI/layouts/` | `.layout` files created in Workbench |
| `UI/imagesets/` | Image resources (sprites, icons) |
| `UI/fonts/` | Font definitions |

---

## Best Practices

1. **Use the context pattern** for all UI screens — consistent lifecycle and management
2. **Register contexts** with the UI Manager component on the player prefab
3. **Handle null checks** when finding widgets — layout may not be loaded yet in `OnCreate()`
4. **Clean up in `OnHide()`** — release temporary references, stop timers
5. **Use localization** for all user-facing text strings
6. **Test on different resolutions** — layout anchoring should work across screen sizes
7. **Consider controller support** — ensure navigable via gamepad for console players
8. **Refresh in `OnShow()`** — re-read manager state each time context becomes visible
9. **Don't store widget references long-term** — layouts can be destroyed and recreated

---

## Common UI Patterns

### Confirmation Dialogs

```cpp
ui.ShowConfirmationDialog("Are you sure?", this, "OnConfirmed");

void OnConfirmed(bool confirmed)
{
    if (confirmed)
    {
        // Perform action
    }
}
```

### Progress Bars

```cpp
ProgressBarWidget progress = ProgressBarWidget.Cast(
    GetRootWidget().FindAnyWidget("Progress_Operation"));
if (progress)
{
    progress.SetCurrent(currentValue);
    progress.SetMax(maxValue);
}
```

### Lists and Grids

```cpp
SCR_ListBoxComponent listBox = SCR_ListBoxComponent.Cast(
    GetRootWidget().FindAnyWidget("List_Items").FindHandler(SCR_ListBoxComponent));
if (listBox)
{
    listBox.Clear();
    foreach (ItemData item : items)
    {
        listBox.AddItem(item.GetName(), item);
    }
}
```

---

## Resource Files

For interaction actions, input binding, and vanilla prefab proxy patterns:
- `reforger-actions-ui` — ScriptedUserAction, ActionsManagerComponent, proxy prefabs
- `enforcescript-patterns/ui-patterns.md` — Engine-level UI context and layout patterns
- `overthrow-networking` — Client→server communication from UI contexts

**Pattern:** Use this skill for Overthrow-specific UI context patterns; reference other skills for engine-level and interaction details.
