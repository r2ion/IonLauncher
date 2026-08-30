#pragma once

#include "particledocument.h"
#include "mathlib/vector.h"
#include "vgui/IClientPanel.h"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class IEngineVGui;

namespace vgui
{
class IPanel;
class IVGui;
}

namespace ParticleTools
{

struct ParticlePreviewControlPoint
{
	int m_Index = 0;
	Vector m_Position;
	QAngle m_Angles;
};

std::filesystem::path GetParticleEditorDirectory();

class CParticleToolSystem;
class CParticleEditorVPanelClient;

class CParticleEditorWorkspace final : public vgui::IClientPanel
{
public:
	explicit CParticleEditorWorkspace(CParticleToolSystem& toolSystem);
	~CParticleEditorWorkspace();

	CParticleEditorWorkspace(const CParticleEditorWorkspace&) = delete;
	CParticleEditorWorkspace& operator=(const CParticleEditorWorkspace&) = delete;

	bool Open();
	void Close();
	bool IsOpen() const;
	void SetVisible(bool visible);
	void Think() override;
	void SetPreviewEnabled(bool enabled);
	void PaintEngineUi();
	void SetStatus(std::string status);

	vgui::VPANEL GetVPanel() override;
	bool GetPreviewViewport(int& x, int& y, int& width, int& height) const;
	bool OpenDocument(const std::filesystem::path& path);
	bool SaveDocument(bool saveAs);
	void PreviewDocument();
	void StopPreview();

private:
	friend class CParticleEditorVPanelClient;
	struct Rect
	{
		int m_X0 = 0;
		int m_Y0 = 0;
		int m_X1 = 0;
		int m_Y1 = 0;

		int Width() const { return m_X1 - m_X0; }
		int Height() const { return m_Y1 - m_Y0; }
		bool Contains(int x, int y) const { return x >= m_X0 && x < m_X1 && y >= m_Y0 && y < m_Y1; }
	};

	struct Layout
	{
		Rect m_MenuBar;
		Rect m_Toolbar;
		Rect m_Browser;
		Rect m_Preview;
		Rect m_Editor;
		Rect m_Properties;
		Rect m_Status;
	};

	enum class Action : std::uint8_t
	{
		None,
		FileMenu,
		EditMenu,
		ViewMenu,
		ParticleMenu,
		NewDocument,
		OpenDocument,
		SaveDocument,
		SaveDocumentAs,
		Preview,
		StopPreview,
		CloseDocument,
		SaveAndPreview,
		CreateParticleSystem,
		DuplicateParticleSystem,
		DeleteParticleSystem,
		HideWorkspace,
		SelectBrowserRow,
		ToggleBrowserRow,
		SelectPropertyRow,
		SelectComponentTab,
		SelectControlPointTab,
		PreviousComponentCategory,
		NextComponentCategory,
		AddComponent,
		RemoveComponent,
		MoveComponentUp,
		MoveComponentDown,
		SelectComponentPreset,
		PreviousAttributeType,
		NextAttributeType,
		AddAttribute,
		ApplyAttribute,
		RemoveAttribute,
		SelectControlPoint,
		AddControlPoint,
		ApplyControlPoint,
		RemoveControlPoint,
		EditField,
	};

	enum class TextField : std::uint8_t
	{
		None,
		EffectName,
		AttributeName,
		AttributeValue,
		ComponentFunction,
		ControlPointIndex,
		ControlPointPositionX,
		ControlPointPositionY,
		ControlPointPositionZ,
		ControlPointPitch,
		ControlPointYaw,
		ControlPointRoll,
	};

	enum class EditorTab : std::uint8_t
	{
		Components,
		ControlPoints,
	};

	enum class OpenMenu : std::uint8_t
	{
		None,
		File,
		Edit,
		View,
		Particle,
	};

	struct HitTarget
	{
		Rect m_Bounds;
		Action m_Action = Action::None;
		std::size_t m_Index = 0;
		TextField m_Field = TextField::None;
	};

	struct BrowserRow
	{
		std::string m_Label;
		std::optional<DmObjectId_t> m_ElementId;
		std::optional<std::string> m_Category;
		int m_Indent = 0;
		bool m_CategoryRow = false;
		bool m_Collapsible = false;
	};

	void PerformApplySchemeSettings() override;
	void PaintTraverse(bool forceRepaint, bool allowForce) override;
	void Repaint() override;
	vgui::VPANEL IsWithinTraverse(int x, int y, bool traversePopups) override;
	void GetInset(int& top, int& left, int& right, int& bottom) override;
	void GetClipRect(int& x0, int& y0, int& x1, int& y1) override;
	void OnChildAdded(vgui::VPANEL child) override;
	void OnSizeChanged(int newWide, int newTall) override;
	void OnVisibleChanged(bool visible) override;
	void InternalFocusChanged(bool lost) override;
	bool RequestInfo(KeyValues* outputData) override;
	void RequestFocus(int direction) override;
	bool RequestFocusPrev(vgui::VPANEL existingPanel) override;
	bool RequestFocusNext(vgui::VPANEL existingPanel) override;
	void OnMessage(const KeyValues* params, vgui::VPANEL fromPanel) override;
	vgui::VPANEL GetCurrentKeyFocus() override;
	int GetTabPosition() override;
	bool Unknown20() override;
	const char* GetName() override;
	const char* GetClassName() override;
	vgui::HScheme GetScheme() override;
	bool IsProportional() override;
	bool IsAutoDeleteSet() override;
	void DeletePanel() override;
	void* QueryInterface(std::uintptr_t interfaceId) override;
	vgui::Panel* GetPanel() override;
	const char* GetModuleName() override;
	void OnTick() override;

	bool CreateVGuiPanel();
	void DestroyVGuiPanel();
	void FocusVGuiPanel();
	Layout CalculateLayout() const;
	Rect CalculatePreviewViewport(const Layout& layout) const;
	void UpdateBounds();
	void SetGameUIPanelsVisible(bool visible);
	void Paint();
	void PaintMenuBar(const Layout& layout);
	void PaintToolbar(const Layout& layout);
	void PaintBrowser(const Layout& layout);
	void PaintPreview(const Layout& layout);
	void PaintEditor(const Layout& layout);
	void PaintProperties(const Layout& layout);
	void PaintStatus(const Layout& layout);
	void PaintOpenMenu(const Layout& layout);

	void DrawFilledRect(const Rect& rect, int red, int green, int blue, int alpha = 255) const;
	void DrawOutlinedRect(const Rect& rect, int red, int green, int blue, int alpha = 255) const;
	void DrawText(int x, int y, std::string_view text, int red, int green, int blue, int alpha = 255,
		unsigned long font = 0) const;
	void DrawClippedText(const Rect& rect, int x, int y, std::string_view text,
		int red, int green, int blue, int alpha = 255) const;
	void DrawButton(const Rect& rect, std::string_view label, Action action, std::size_t index = 0,
		bool selected = false);
	void DrawTextField(const Rect& rect, std::string_view label, TextField field, const std::string& value);
	void AddHitTarget(const Rect& rect, Action action, std::size_t index = 0, TextField field = TextField::None);

	void HandleCursorMoved(int x, int y);
	void HandleMousePressed(int code);
	void HandleMouseReleased(int code);
	void HandleMouseWheel(int delta);
	void HandleKeyCodePressed(int code);
	void HandleKeyTyped(int character);
	void ExecuteAction(const HitTarget& target);
	int FindHitTarget(int x, int y) const;
	void BeginTextEdit(TextField field);
	void EndTextEdit(bool accept);
	std::string* GetTextField(TextField field);
	void PasteClipboardText();

	bool RequestNewDocument();
	bool RequestOpenDocument();
	bool RequestCloseDocument();
	bool ConfirmSaveChanges();
	std::optional<std::filesystem::path> ShowPcfFileDialog(bool save);
	void RestoreEditorFocus();
	void CloseDocument();
	void CreateNewDocument(std::string effectName, bool logCreation);
	void MarkDirty();
	void CreateParticleSystem();
	void DuplicateParticleSystem();
	void DeleteParticleSystem();
	void RebuildBrowserRows();
	void SelectBrowserRow(std::size_t index);
	void ToggleBrowserRow(std::size_t index);
	std::string BrowserRowKey(const BrowserRow& row) const;
	bool BrowserRowCollapsed(const BrowserRow& row) const;
	DmxElement* SelectedElement();
	const DmxElement* SelectedElement() const;
	DmxElement* CurrentSystem();
	const DmxElement* CurrentSystem() const;
	const DmxElement* SelectedPreviewSystem() const;
	std::optional<DmObjectId_t> FirstSystemId() const;
	std::optional<DmObjectId_t> PreferredPreviewSystemId() const;
	void RequestActivePreview(const DmxElement& previewSystem);
	void RefreshActivePreview();
	std::filesystem::path DefaultAssetPath(std::string_view effectName) const;

	void SelectPropertyRow(std::size_t row);
	bool ReadAttributeEditor(DmxAttribute& attribute, std::string& error) const;
	void AddAttribute();
	void ApplyAttribute();
	void RemoveAttribute();

	DmxAttribute* FindCategoryAttribute(DmxElement& system, std::string_view category, bool create);
	void AddComponent(std::optional<std::string_view> preset = std::nullopt);
	void RemoveComponent();
	void MoveComponent(int direction);

	void LoadControlPoints();
	std::vector<ParticlePreviewControlPoint> ReadControlPoints(const DmxElement& system) const;
	bool ReadControlPointEditor(ParticlePreviewControlPoint& controlPoint, std::string& error) const;
	bool PersistControlPoints(const std::vector<ParticlePreviewControlPoint>& controlPoints, std::string& error);
	void SelectControlPoint(std::size_t index);
	void AddControlPoint();
	void ApplyControlPoint();
	void RemoveControlPoint();

	CParticleToolSystem& m_ToolSystem;
	std::unique_ptr<CParticleEditorVPanelClient> m_pVPanelClient;
	IEngineVGui* m_pEngineVGui = nullptr;
	vgui::IVGui* m_pVGui = nullptr;
	vgui::IPanel* m_pPanelInterface = nullptr;
	vgui::VPANEL m_VPanel = 0;
	bool m_Open = false;
	bool m_Visible = false;
	bool m_RestoreGameUI = false;
	bool m_NeedsRepaint = true;
	int m_Width = 0;
	int m_Height = 0;
	unsigned long m_Font = 0;
	unsigned long m_BoldFont = 0;
	std::atomic<bool> m_PreviewEnabled = false;

	std::array<HitTarget, 1024> m_HitTargets{};
	std::size_t m_HitTargetCount = 0;
	int m_PressedHitTarget = -1;
	OpenMenu m_OpenMenu = OpenMenu::None;
	EditorTab m_EditorTab = EditorTab::Components;
	TextField m_ActiveTextField = TextField::None;
	bool m_PreviewOrbiting = false;
	bool m_PreviewPanning = false;
	int m_PreviewCursorX = 0;
	int m_PreviewCursorY = 0;
	bool m_SelectAllText = false;
	bool m_SkipNextTypedSpace = false;
	std::string m_EditOriginalText;

	ParticleDocument m_Document;
	std::filesystem::path m_Path;
	std::string m_PathText;
	std::string m_EffectNameText = "northstar_particle";
	bool m_Dirty = false;
	std::string m_Status = "Ready";
	std::vector<BrowserRow> m_BrowserRows;
	std::size_t m_BrowserScroll = 0;
	std::set<std::string> m_CollapsedBrowserNodes;
	std::optional<DmObjectId_t> m_SelectedElementId;
	std::optional<DmObjectId_t> m_SelectedSystemId;
	std::optional<DmObjectId_t> m_PreviewSystemId;
	bool m_PreviewRunning = false;
	std::optional<std::string> m_SelectedCategory;
	std::optional<std::size_t> m_SelectedAttributeIndex;
	bool m_SelectedElementName = false;
	std::size_t m_PropertyScroll = 0;
	std::string m_AttributeNameText;
	std::string m_AttributeValueText;
	DmAttributeType_t m_AttributeType = AT_STRING;

	std::size_t m_ComponentCategoryIndex = 3;
	std::string m_ComponentFunctionText = "emit_continuously";
	std::vector<ParticlePreviewControlPoint> m_ControlPoints;
	std::optional<std::size_t> m_SelectedControlPoint;
	std::string m_ControlPointIndexText = "1";
	std::array<std::string, 3> m_ControlPointPositionText{{"0", "0", "0"}};
	std::array<std::string, 3> m_ControlPointAnglesText{{"0", "0", "0"}};
};

} // namespace ParticleTools
