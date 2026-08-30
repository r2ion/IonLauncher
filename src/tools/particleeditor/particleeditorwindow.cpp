#include "particleeditorwindow.h"
#include "particletoolssystem.h"

#include "config/profile.h"
#include "core/tier0.h"
#include "core/tier1.h"
#include "logging/logging.h"
#include "tier1/keyvalues.h"
#include "util/wininfo.h"
#include "vgui/ienginevgui.h"
#include "vgui/IPanel.h"
#include "vgui/IVGui.h"
#include "vgui/surface.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <windows.h>
#include <commdlg.h>

namespace ParticleTools
{
namespace ParticleEditorVGui
{
constexpr int R2VGuiMouseLeft = 107;
constexpr int R2VGuiMouseMiddle = 109;

constexpr int MenuHeight = 26;
constexpr int ToolbarHeight = 38;
constexpr int StatusHeight = 24;
constexpr int PaneGap = 4;
constexpr int RowHeight = 20;
constexpr int HeaderHeight = 25;
constexpr int PreviewAspectWidth = 16;
constexpr int PreviewAspectHeight = 9;
inline constexpr wchar_t PcfDialogFilter[] =
	L"Particle Configuration File (*.pcf)\0*.pcf\0All Files (*.*)\0*.*\0";


constexpr std::array<std::string_view, 8> ComponentCategories = {
	"renderers", "operators", "initializers", "emitters", "forces", "constraints", "scripts", "children"};
constexpr std::array<std::string_view, 28> AttributeTypeNames = {
	"element", "int", "float", "bool", "string", "binary", "time", "color",
	"vector2", "vector3", "vector4", "qangle", "quaternion", "vmatrix",
	"element_array", "int_array", "float_array", "bool_array", "string_array",
	"binary_array", "time_array", "color_array", "vector2_array", "vector3_array",
	"vector4_array", "qangle_array", "quaternion_array", "vmatrix_array"};

struct ComponentPreset
{
	std::string_view m_Label;
	std::string_view m_Category;
	std::string_view m_Function;
};

constexpr std::array<ComponentPreset, 6> ComponentPresets = {{
    {"Sprite renderer", "renderers", "render_animated_sprites"},
    {"Movement", "operators", "Movement Basic"},
    {"Lifespan decay", "operators", "Lifespan Decay"},
    {"Lifetime", "initializers", "Lifetime Random"},
    {"Continuous emitter", "emitters", "emit_continuously"},
    {"Radius", "initializers", "Radius Random"},
}};






std::wstring ToWide(std::string_view value)
{
	if (value.empty())
		return {};
	const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
	if (length <= 0)
		return {};
	std::wstring result(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
	return result;
}

std::string ToUtf8(std::wstring_view value)
{
	if (value.empty())
		return {};
	const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
	if (length <= 0)
		return {};
	std::string result(static_cast<std::size_t>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
	return result;
}

std::filesystem::path PathFromUtf8(std::string_view value)
{
	return std::filesystem::path(ToWide(value));
}

std::string PathToUtf8(const std::filesystem::path& path)
{
	return ToUtf8(path.wstring());
}

bool ParseInt(std::string_view text, int& value)
{
	const char* begin = text.data();
	const char* end = begin + text.size();
	const auto result = std::from_chars(begin, end, value);
	return result.ec == std::errc() && result.ptr == end;
}

bool ParseFloat(std::string_view text, float& value)
{
	const char* begin = text.data();
	const char* end = begin + text.size();
	const auto result = std::from_chars(begin, end, value, std::chars_format::general);
	return result.ec == std::errc() && result.ptr == end && std::isfinite(value);
}

std::vector<float> ParseNumberList(std::string value)
{
	for (char& character : value)
	{
		if (character == '[' || character == ']' || character == ',')
			character = ' ';
	}
	std::istringstream stream(value);
	stream.imbue(std::locale::classic());
	std::vector<float> result;
	float number = 0.0f;
	while (stream >> number)
		result.push_back(number);
	return result;
}

std::vector<int> ParseIntegerList(std::string value)
{
	for (char& character : value)
	{
		if (character == '[' || character == ']' || character == ',')
			character = ' ';
	}
	std::istringstream stream(value);
	stream.imbue(std::locale::classic());
	std::vector<int> result;
	int number = 0;
	while (stream >> number)
		result.push_back(number);
	return result;
}

std::string FormatControlPointIndices(const std::vector<ParticlePreviewControlPoint>& controlPoints)
{
	std::ostringstream stream;
	stream << '[';
	for (std::size_t index = 0; index < controlPoints.size(); ++index)
	{
		if (index != 0)
			stream << ", ";
		stream << controlPoints[index].m_Index;
	}
	stream << ']';
	return stream.str();
}

std::string FormatControlPointTuples(const std::vector<ParticlePreviewControlPoint>& controlPoints, bool angles)
{
	std::ostringstream stream;
	stream.imbue(std::locale::classic());
	stream << std::setprecision(std::numeric_limits<float>::max_digits10) << '[';
	for (std::size_t index = 0; index < controlPoints.size(); ++index)
	{
		if (index != 0)
			stream << ", ";
		const float* values = angles ? controlPoints[index].m_Angles.Base() : controlPoints[index].m_Position.Base();
		stream << '[' << values[0] << ", " << values[1] << ", " << values[2] << ']';
	}
	stream << ']';
	return stream.str();
}

std::string ReferenceText(const DmxAttribute& attribute)
{
	if (attribute.m_Type == AT_ELEMENT)
		return attribute.m_ElementIds.empty() ? std::string() : FormatObjectId(attribute.m_ElementIds.front());
	std::string result = "[";
	for (std::size_t index = 0; index < attribute.m_ElementIds.size(); ++index)
	{
		if (index != 0)
			result += ", ";
		result += FormatObjectId(attribute.m_ElementIds[index]);
	}
	result += ']';
	return result;
}

bool ParseElementReferences(std::string_view value, bool array, std::vector<DmObjectId_t>& ids)
{
	ids.clear();
	if (!array && value.empty())
		return true;
	for (std::size_t position = 0; position < value.size();)
	{
		while (position < value.size() &&
			(value[position] == ' ' || value[position] == '\t' || value[position] == '\r' || value[position] == '\n' ||
			 value[position] == '[' || value[position] == ']' || value[position] == ',' || value[position] == '"'))
			++position;
		if (position == value.size())
			break;
		if (position + 36 > value.size())
			return false;
		DmObjectId_t id;
		if (!ParseObjectId(value.substr(position, 36), id) || !IsUniqueIdValid(id))
			return false;
		ids.push_back(id);
		position += 36;
		if (!array)
		{
			while (position < value.size() && (value[position] == ' ' || value[position] == '\t'))
				++position;
			if (position != value.size())
				return false;
		}
	}
	return array || ids.size() <= 1;
}

std::string SanitizeAssetName(std::string_view value)
{
	std::string result;
	result.reserve(value.size());
	for (char character : value)
	{
		const unsigned char byte = static_cast<unsigned char>(character);
		if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
			(byte >= '0' && byte <= '9') || character == '_' || character == '-')
			result.push_back(character);
		else if (character == ' ')
			result.push_back('_');
	}
	return result.empty() ? "northstar_particle" : result;
}

std::size_t TypeIndex(DmAttributeType_t type)
{
	const std::size_t index = static_cast<std::size_t>(type);
	return index > 0 && index <= AttributeTypeNames.size() ? index - 1 : 4;
}

std::vector<DmObjectId_t> CollectOwnedElementIds(const ParticleDocument& document, const DmxElement& system)
{
	std::vector<DmObjectId_t> result;
	for (std::string_view categoryName : ComponentCategories)
	{
		const DmxAttribute* category = system.FindAttribute(categoryName);
		if (!category || category->m_Type != AT_ELEMENT_ARRAY)
			continue;
		for (const DmObjectId_t& id : category->m_ElementIds)
		{
			if (std::find(result.begin(), result.end(), id) == result.end())
				result.push_back(id);
		}
	}

	const DmxElement* root = document.Root();
	for (std::size_t index = 0; index < result.size(); ++index)
	{
		const DmxElement* element = document.FindElement(result[index]);
		if (!element)
			continue;
		for (const DmxAttribute& attribute : element->m_Attributes)
		{
			if (!attribute.IsElementReference())
				continue;
			for (const DmObjectId_t& id : attribute.m_ElementIds)
			{
				const DmxElement* referenced = document.FindElement(id);
				if (!referenced || referenced == root || referenced->m_Type == "DmeParticleSystemDefinition")
					continue;
				if (std::find(result.begin(), result.end(), id) == result.end())
					result.push_back(id);
			}
		}
	}
	return result;
}

void RemapElementReferences(
	DmxElement& element, const std::vector<std::pair<DmObjectId_t, DmObjectId_t>>& idMap)
{
	for (DmxAttribute& attribute : element.m_Attributes)
	{
		if (!attribute.IsElementReference())
			continue;
		for (DmObjectId_t& id : attribute.m_ElementIds)
		{
			const auto iterator = std::find_if(idMap.begin(), idMap.end(),
				[&id](const auto& mapping) { return mapping.first == id; });
			if (iterator != idMap.end())
				id = iterator->second;
		}
	}
}

} // namespace ParticleEditorVGui
#define PARTICLE_EDITOR_PANEL_SLOT(number) \
	virtual std::uintptr_t PanelSlot##number() { return 0; }
#define PARTICLE_EDITOR_PANEL_SLOTS_10(a, b, c, d, e, f, g, h, i, j) \
	PARTICLE_EDITOR_PANEL_SLOT(a) \
	PARTICLE_EDITOR_PANEL_SLOT(b) \
	PARTICLE_EDITOR_PANEL_SLOT(c) \
	PARTICLE_EDITOR_PANEL_SLOT(d) \
	PARTICLE_EDITOR_PANEL_SLOT(e) \
	PARTICLE_EDITOR_PANEL_SLOT(f) \
	PARTICLE_EDITOR_PANEL_SLOT(g) \
	PARTICLE_EDITOR_PANEL_SLOT(h) \
	PARTICLE_EDITOR_PANEL_SLOT(i) \
	PARTICLE_EDITOR_PANEL_SLOT(j)

// VPanel stores an IClientPanel pointer, but Titanfall 2's surface traversal also
// invokes the concrete Panel virtual tail. Keep the workspace UI engine-owned
// while supplying that complete C++ ABI without patching a retail vtable.
class CParticleEditorVPanelClient final : public vgui::IClientPanel
{
public:
	explicit CParticleEditorVPanelClient(CParticleEditorWorkspace& workspace) : m_Workspace(workspace) {}

	vgui::VPANEL GetVPanel() override { return m_Workspace.GetVPanel(); }
	void Think() override { m_Workspace.Think(); }
	void PerformApplySchemeSettings() override { m_Workspace.PerformApplySchemeSettings(); }
	void PaintTraverse(bool forceRepaint, bool allowForce) override
	{
		m_Workspace.PaintTraverse(forceRepaint, allowForce);
	}
	void Repaint() override { m_Workspace.Repaint(); }
	vgui::VPANEL IsWithinTraverse(int x, int y, bool traversePopups) override
	{
		return m_Workspace.IsWithinTraverse(x, y, traversePopups);
	}
	void GetInset(int& top, int& left, int& right, int& bottom) override
	{
		m_Workspace.GetInset(top, left, right, bottom);
	}
	void GetClipRect(int& x0, int& y0, int& x1, int& y1) override
	{
		m_Workspace.GetClipRect(x0, y0, x1, y1);
	}
	void OnChildAdded(vgui::VPANEL child) override { m_Workspace.OnChildAdded(child); }
	void OnSizeChanged(int newWide, int newTall) override { m_Workspace.OnSizeChanged(newWide, newTall); }
	void OnVisibleChanged(bool visible) override { m_Workspace.OnVisibleChanged(visible); }
	void InternalFocusChanged(bool lost) override { m_Workspace.InternalFocusChanged(lost); }
	bool RequestInfo(KeyValues* outputData) override { return m_Workspace.RequestInfo(outputData); }
	void RequestFocus(int direction) override { m_Workspace.RequestFocus(direction); }
	bool RequestFocusPrev(vgui::VPANEL existingPanel) override
	{
		return m_Workspace.RequestFocusPrev(existingPanel);
	}
	bool RequestFocusNext(vgui::VPANEL existingPanel) override
	{
		return m_Workspace.RequestFocusNext(existingPanel);
	}
	void OnMessage(const KeyValues* params, vgui::VPANEL fromPanel) override
	{
		m_Workspace.OnMessage(params, fromPanel);
	}
	vgui::VPANEL GetCurrentKeyFocus() override { return m_Workspace.GetCurrentKeyFocus(); }
	int GetTabPosition() override { return m_Workspace.GetTabPosition(); }
	bool Unknown20() override { return m_Workspace.Unknown20(); }
	const char* GetName() override { return m_Workspace.GetName(); }
	const char* GetClassName() override { return m_Workspace.GetClassName(); }
	vgui::HScheme GetScheme() override { return m_Workspace.GetScheme(); }
	bool IsProportional() override { return m_Workspace.IsProportional(); }
	bool IsAutoDeleteSet() override { return m_Workspace.IsAutoDeleteSet(); }
	void DeletePanel() override { m_Workspace.DeletePanel(); }
	void* QueryInterface(std::uintptr_t interfaceId) override
	{
		return m_Workspace.QueryInterface(interfaceId);
	}
	vgui::Panel* GetPanel() override { return reinterpret_cast<vgui::Panel*>(this); }
	const char* GetModuleName() override { return m_Workspace.GetModuleName(); }
	void OnTick() override { m_Workspace.OnTick(); }

	PARTICLE_EDITOR_PANEL_SLOTS_10(31, 32, 33, 34, 35, 36, 37, 38, 39, 40)
	PARTICLE_EDITOR_PANEL_SLOTS_10(41, 42, 43, 44, 45, 46, 47, 48, 49, 50)
	PARTICLE_EDITOR_PANEL_SLOTS_10(51, 52, 53, 54, 55, 56, 57, 58, 59, 60)
	PARTICLE_EDITOR_PANEL_SLOTS_10(61, 62, 63, 64, 65, 66, 67, 68, 69, 70)
	PARTICLE_EDITOR_PANEL_SLOTS_10(71, 72, 73, 74, 75, 76, 77, 78, 79, 80)
	PARTICLE_EDITOR_PANEL_SLOTS_10(81, 82, 83, 84, 85, 86, 87, 88, 89, 90)
	PARTICLE_EDITOR_PANEL_SLOTS_10(91, 92, 93, 94, 95, 96, 97, 98, 99, 100)
	PARTICLE_EDITOR_PANEL_SLOTS_10(101, 102, 103, 104, 105, 106, 107, 108, 109, 110)
	PARTICLE_EDITOR_PANEL_SLOTS_10(111, 112, 113, 114, 115, 116, 117, 118, 119, 120)
	PARTICLE_EDITOR_PANEL_SLOTS_10(121, 122, 123, 124, 125, 126, 127, 128, 129, 130)
	PARTICLE_EDITOR_PANEL_SLOTS_10(131, 132, 133, 134, 135, 136, 137, 138, 139, 140)
	PARTICLE_EDITOR_PANEL_SLOTS_10(141, 142, 143, 144, 145, 146, 147, 148, 149, 150)
	PARTICLE_EDITOR_PANEL_SLOTS_10(151, 152, 153, 154, 155, 156, 157, 158, 159, 160)
	PARTICLE_EDITOR_PANEL_SLOTS_10(161, 162, 163, 164, 165, 166, 167, 168, 169, 170)
	PARTICLE_EDITOR_PANEL_SLOTS_10(171, 172, 173, 174, 175, 176, 177, 178, 179, 180)
	PARTICLE_EDITOR_PANEL_SLOTS_10(181, 182, 183, 184, 185, 186, 187, 188, 189, 190)
	PARTICLE_EDITOR_PANEL_SLOTS_10(191, 192, 193, 194, 195, 196, 197, 198, 199, 200)
	PARTICLE_EDITOR_PANEL_SLOTS_10(201, 202, 203, 204, 205, 206, 207, 208, 209, 210)
	PARTICLE_EDITOR_PANEL_SLOTS_10(211, 212, 213, 214, 215, 216, 217, 218, 219, 220)
	PARTICLE_EDITOR_PANEL_SLOT(221)
	PARTICLE_EDITOR_PANEL_SLOT(222)
	PARTICLE_EDITOR_PANEL_SLOT(223)
	PARTICLE_EDITOR_PANEL_SLOT(224)
	PARTICLE_EDITOR_PANEL_SLOT(225)
	PARTICLE_EDITOR_PANEL_SLOT(226)
	PARTICLE_EDITOR_PANEL_SLOT(227)
	PARTICLE_EDITOR_PANEL_SLOT(228)
	PARTICLE_EDITOR_PANEL_SLOT(229)

private:
	std::array<std::byte, 0x260> m_PanelData{};
	CParticleEditorWorkspace& m_Workspace;
};

static_assert(sizeof(CParticleEditorVPanelClient) == 0x270);

#undef PARTICLE_EDITOR_PANEL_SLOTS_10
#undef PARTICLE_EDITOR_PANEL_SLOT

std::filesystem::path GetParticleEditorDirectory()
{
	std::filesystem::path profileDirectory = GetNorthstarPrefix();
	if (profileDirectory.empty())
		profileDirectory = "R2Northstar";

	wchar_t modulePath[MAX_PATH]{};
	if (!GetModuleFileNameW(g_NorthstarModule, modulePath, MAX_PATH))
		return profileDirectory / "tools" / "particleeditor";

	std::filesystem::path moduleDirectory = std::filesystem::path(modulePath).parent_path();
	if (moduleDirectory.filename() != profileDirectory.filename())
		moduleDirectory /= profileDirectory;
	return moduleDirectory / "tools" / "particleeditor";
}

CParticleEditorWorkspace::CParticleEditorWorkspace(CParticleToolSystem& toolSystem)
	: m_ToolSystem(toolSystem), m_pVPanelClient(std::make_unique<CParticleEditorVPanelClient>(*this))
{
}

CParticleEditorWorkspace::~CParticleEditorWorkspace()
{
	Close();
}

bool CParticleEditorWorkspace::Open()
{
	if (m_Open)
	{
		SetVisible(true);
		return true;
	}
	if (!CreateVGuiPanel())
		return false;

	m_Open = true;
	CreateNewDocument(m_EffectNameText, false);
	SetVisible(true);
	spdlog::info("[ParticleTools] Engine VGUI workspace opened");
	return true;
}

void CParticleEditorWorkspace::Close()
{
	if (m_Visible)
		SetVisible(false);
	if (!m_Open && !m_VPanel)
		return;
	DestroyVGuiPanel();
	m_Open = false;
	m_Visible = false;
	spdlog::info("[ParticleTools] Engine VGUI workspace closed");
}

bool CParticleEditorWorkspace::IsOpen() const
{
	return m_Open;
}

void CParticleEditorWorkspace::SetVisible(bool visible)
{
	if (!visible)
	{
		m_PreviewOrbiting = false;
		m_PreviewPanning = false;
		m_ToolSystem.SetEditorMouseCapture(false);
		m_ToolSystem.SetEditorInputEnabled(false);
	}

	const bool visibilityChanged = m_Visible != visible;
	m_Visible = visible;
	if (visibilityChanged && !visible)
	{
		m_PreviewRunning = false;
		m_ToolSystem.RequestStopPreview();
	}
	if (visible)
	{
		if (m_pEngineVGui && m_pEngineVGui->IsGameUIVisible())
			m_RestoreGameUI = true;
		SetGameUIPanelsVisible(false);
	}
	if (!m_VPanel || !m_pPanelInterface)
		return;

	m_pPanelInterface->SetMouseInputEnabled(m_VPanel, visible);
	m_pPanelInterface->SetKeyBoardInputEnabled(m_VPanel, visible);
	m_pPanelInterface->SetVisible(m_VPanel, visible);
	if (visible)
	{
		m_pPanelInterface->SetZPos(m_VPanel, 32000);
		m_pPanelInterface->MoveToFront(m_VPanel);
		FocusVGuiPanel();
		m_ToolSystem.SetEditorInputEnabled(true);
		Repaint();
	}
	else if (m_RestoreGameUI && m_pEngineVGui)
	{
		SetGameUIPanelsVisible(true);
		m_pEngineVGui->ActivateGameUI();
		m_RestoreGameUI = false;
	}
}

void CParticleEditorWorkspace::SetGameUIPanelsVisible(bool visible)
{
	if (!m_pEngineVGui || !m_pPanelInterface)
		return;

	const vgui::VPANEL gameUiPanel = m_pEngineVGui->GetPanel(PANEL_GAMEUIDLL);
	if (gameUiPanel)
		m_pPanelInterface->SetVisible(gameUiPanel, visible);
	const vgui::VPANEL backgroundPanel = m_pEngineVGui->GetPanel(PANEL_GAMEUIBACKGROUND);
	if (backgroundPanel)
		m_pPanelInterface->SetVisible(backgroundPanel, visible);
}

void CParticleEditorWorkspace::Think()
{
	if (!m_Open || !m_Visible || !m_VPanel)
		return;
	if (m_pEngineVGui && m_pEngineVGui->IsGameUIVisible())
		m_RestoreGameUI = true;
	SetGameUIPanelsVisible(false);
	UpdateBounds();
	if (m_pEngineVGui && m_pEngineVGui->IsConsoleVisible())
		m_pEngineVGui->HideConsole();

	// Orbit/pan the preview camera from the live cursor position each frame. The
	// mouse is never captured during dragging, so the cursor stays free and its
	// position keeps updating even though the engine's "CursorMoved" messages
	// stop carrying new coordinates once the workspace has input focus.
	if (m_PreviewOrbiting || m_PreviewPanning)
	{
		const bool held = m_PreviewOrbiting ? (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0
			: (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
		if (!held)
		{
			m_PreviewOrbiting = false;
			m_PreviewPanning = false;
			m_ToolSystem.SetEditorMouseCapture(false);
		}
		else if (vgui::g_pVGuiSurface)
		{
			int x = 0;
			int y = 0;
			vgui::g_pVGuiSurface->SurfaceGetCursorPos(x, y);
			const int deltaX = x - m_PreviewCursorX;
			const int deltaY = y - m_PreviewCursorY;
			m_PreviewCursorX = x;
			m_PreviewCursorY = y;
			if (deltaX != 0 || deltaY != 0)
			{
				if (m_PreviewPanning)
					m_ToolSystem.PanPreviewCamera(static_cast<float>(deltaX), static_cast<float>(deltaY));
				else
					m_ToolSystem.AdjustPreviewCamera(static_cast<float>(deltaX) * 0.35f, static_cast<float>(deltaY) * 0.35f, 0.0f);
			}
		}
	}

	Repaint();
}
void CParticleEditorWorkspace::PaintEngineUi()
{
	if (m_Open && m_Visible && m_VPanel && m_pPanelInterface)
		m_pPanelInterface->PaintTraverse(m_VPanel, true, true);
}


void CParticleEditorWorkspace::SetStatus(std::string status)
{
	m_Status = std::move(status);
	Repaint();
}

vgui::VPANEL CParticleEditorWorkspace::GetVPanel()
{
	return m_VPanel;
}

bool CParticleEditorWorkspace::GetPreviewViewport(int& x, int& y, int& width, int& height) const
{
	if (!m_PreviewEnabled.load(std::memory_order_acquire) ||
		!m_Open || !m_Visible || !m_VPanel || m_Width <= 0 || m_Height <= 0)
		return false;
	const Rect preview = CalculatePreviewViewport(CalculateLayout());
	x = preview.m_X0;
	y = preview.m_Y0;
	width = preview.Width();
	height = preview.Height();
	return width > 8 && height > 8;
}

void CParticleEditorWorkspace::SetPreviewEnabled(bool enabled)
{
	if (!enabled)
		m_PreviewRunning = false;
	if (m_PreviewEnabled.exchange(enabled, std::memory_order_acq_rel) == enabled)
		return;
	Repaint();
}

bool CParticleEditorWorkspace::CreateVGuiPanel()
{
	if (m_VPanel)
		return true;

	m_pEngineVGui = Sys_GetFactoryPtr("engine.dll", VENGINE_VGUI_INTERFACE_VERSION).RCast<IEngineVGui*>();
	m_pVGui = Sys_GetFactoryPtr("vgui2.dll", vgui::VGUI_IVGUI_INTERFACE_VERSION).RCast<vgui::IVGui*>();
	m_pPanelInterface = Sys_GetFactoryPtr("vgui2.dll", vgui::VGUI_PANEL_INTERFACE_VERSION).RCast<vgui::IPanel*>();
	if (!m_pEngineVGui || !m_pVGui || !m_pPanelInterface || !vgui::g_pVGuiSurface)
	{
		spdlog::error("[ParticleTools] VEngineVGui001, VGUI_ivgui008, VGUI_Panel009, or VGUI_Surface031 is unavailable");
		return false;
	}

	vgui::VPANEL parent = m_pEngineVGui->GetPanel(PANEL_GAMEDLL);
	if (!parent)
		parent = m_pEngineVGui->GetPanel(PANEL_ROOT);
	if (!parent)
	{
		spdlog::error("[ParticleTools] The engine tool root is unavailable");
		return false;
	}

	m_VPanel = m_pVGui->AllocPanel();
	if (!m_VPanel)
	{
		spdlog::error("[ParticleTools] Could not allocate the VGUI workspace panel");
		return false;
	}

	m_pPanelInterface->Init(m_VPanel, m_pVPanelClient.get());
	m_pPanelInterface->SetParent(m_VPanel, parent);
	m_pPanelInterface->SetMouseInputEnabled(m_VPanel, true);
	m_pPanelInterface->SetKeyBoardInputEnabled(m_VPanel, true);
	vgui::g_pVGuiSurface->CreatePopup(m_VPanel, false, false, false, true, true);

	m_Font = vgui::g_pVGuiSurface->CreateFont();
	m_BoldFont = vgui::g_pVGuiSurface->CreateFont();
	vgui::g_pVGuiSurface->SetFontGlyphSet(m_Font, "Tahoma", 14, 500, 0, 0, vgui::FONTFLAG_ANTIALIAS);
	vgui::g_pVGuiSurface->SetFontGlyphSet(m_BoldFont, "Tahoma", 14, 700, 0, 0, vgui::FONTFLAG_ANTIALIAS);
	UpdateBounds();
	spdlog::info("[ParticleTools] Workspace VPanel {} parent {} visible={} bounds={}x{}",
		m_VPanel, parent, m_pPanelInterface->IsVisible(parent), m_Width, m_Height);
	return true;
}

void CParticleEditorWorkspace::DestroyVGuiPanel()
{
	if (!m_VPanel)
		return;

	const vgui::VPANEL panel = m_VPanel;
	if (m_pPanelInterface)
	{
		m_pPanelInterface->SetMouseInputEnabled(panel, false);
		m_pPanelInterface->SetKeyBoardInputEnabled(panel, false);
		m_pPanelInterface->SetVisible(panel, false);
		m_pPanelInterface->SetParent(panel, 0);
	}
	if (m_pVGui)
		m_pVGui->FreePanel(panel);

	m_VPanel = 0;
	m_pPanelInterface = nullptr;
	m_pVGui = nullptr;
	m_pEngineVGui = nullptr;
	m_Font = 0;
	m_BoldFont = 0;
}

void CParticleEditorWorkspace::FocusVGuiPanel()
{
	if (!m_VPanel)
		return;
	RequestFocus(0);
}

void CParticleEditorWorkspace::PerformApplySchemeSettings()
{
}

void CParticleEditorWorkspace::PaintTraverse(bool forceRepaint, bool allowForce)
{
	static bool loggedFirstPaintTraverse = false;
	if (!loggedFirstPaintTraverse)
	{
		loggedFirstPaintTraverse = true;
		spdlog::info("[ParticleTools] First workspace PaintTraverse force={} allow={} visible={} repaint={} bounds={}x{}",
			forceRepaint, allowForce, m_Visible, m_NeedsRepaint, m_Width, m_Height);
	}
	if (!m_Visible || !m_VPanel || !vgui::g_pVGuiSurface)
		return;
	if (!forceRepaint && (!allowForce || !m_NeedsRepaint))
		return;

	m_NeedsRepaint = false;
	vgui::g_pVGuiSurface->PushMakeCurrent(m_VPanel, false);
	Paint();
	vgui::g_pVGuiSurface->PopMakeCurrent(m_VPanel);
}

void CParticleEditorWorkspace::Repaint()
{
	m_NeedsRepaint = true;
	if (m_VPanel && vgui::g_pVGuiSurface)
		vgui::g_pVGuiSurface->Invalidate(m_VPanel);
}

vgui::VPANEL CParticleEditorWorkspace::IsWithinTraverse(int x, int y, bool traversePopups)
{
	NOTE_UNUSED(traversePopups);
	if (!m_Visible || !m_VPanel || !m_pPanelInterface ||
		!m_pPanelInterface->IsMouseInputEnabled(m_VPanel))
		return 0;

	int x0 = 0;
	int y0 = 0;
	int x1 = 0;
	int y1 = 0;
	m_pPanelInterface->GetClipRect(m_VPanel, x0, y0, x1, y1);
	return x >= x0 && x < x1 && y >= y0 && y < y1 ? m_VPanel : 0;
}

void CParticleEditorWorkspace::GetInset(int& top, int& left, int& right, int& bottom)
{
	top = 0;
	left = 0;
	right = 0;
	bottom = 0;
}

void CParticleEditorWorkspace::GetClipRect(int& x0, int& y0, int& x1, int& y1)
{
	if (!m_VPanel || !m_pPanelInterface)
	{
		x0 = 0;
		y0 = 0;
		x1 = 0;
		y1 = 0;
		return;
	}
	m_pPanelInterface->GetClipRect(m_VPanel, x0, y0, x1, y1);
}

void CParticleEditorWorkspace::OnChildAdded(vgui::VPANEL child)
{
	NOTE_UNUSED(child);
}

void CParticleEditorWorkspace::OnSizeChanged(int newWide, int newTall)
{
	m_Width = newWide;
	m_Height = newTall;
	Repaint();
}

void CParticleEditorWorkspace::OnVisibleChanged(bool visible)
{
	NOTE_UNUSED(visible);
}

void CParticleEditorWorkspace::InternalFocusChanged(bool lost)
{
	if (!lost)
		return;
	m_PreviewOrbiting = false;
	m_PreviewPanning = false;
	m_ToolSystem.SetEditorMouseCapture(false);
}

bool CParticleEditorWorkspace::RequestInfo(KeyValues* outputData)
{
	NOTE_UNUSED(outputData);
	return false;
}

void CParticleEditorWorkspace::RequestFocus(int direction)
{
	NOTE_UNUSED(direction);
	if (!m_VPanel || !m_pPanelInterface)
		return;
	const vgui::VPANEL parent = m_pPanelInterface->GetParent(m_VPanel);
	if (!parent)
		return;

	KeyValues* message = new KeyValues("OnRequestFocus");
	message->SetPtr("subFocus", reinterpret_cast<void*>(m_VPanel));
	message->SetPtr("defaultPanel", nullptr);
	m_pPanelInterface->SendMessage(parent, message, m_VPanel);
	message->DeleteThis();
}

bool CParticleEditorWorkspace::RequestFocusPrev(vgui::VPANEL existingPanel)
{
	NOTE_UNUSED(existingPanel);
	if (!m_VPanel || !m_pPanelInterface)
		return false;
	const vgui::VPANEL parent = m_pPanelInterface->GetParent(m_VPanel);
	return parent && m_pPanelInterface->RequestFocusPrev(parent, m_VPanel);
}

bool CParticleEditorWorkspace::RequestFocusNext(vgui::VPANEL existingPanel)
{
	NOTE_UNUSED(existingPanel);
	if (!m_VPanel || !m_pPanelInterface)
		return false;
	const vgui::VPANEL parent = m_pPanelInterface->GetParent(m_VPanel);
	return parent && m_pPanelInterface->RequestFocusNext(parent, m_VPanel);
}


void CParticleEditorWorkspace::OnMessage(const KeyValues* params, vgui::VPANEL fromPanel)
{
	NOTE_UNUSED(fromPanel);
	if (!params)
		return;

	const char* name = params->GetName();
	if (!name)
		return;
	KeyValues* values = const_cast<KeyValues*>(params);
	if (std::strcmp(name, "CursorMoved") == 0)
	{
		HandleCursorMoved(values->GetInt("x", 0), values->GetInt("y", 0));
	}
	else if (std::strcmp(name, "MousePressed") == 0 ||
		std::strcmp(name, "MouseDoublePressed") == 0 ||
		std::strcmp(name, "MouseTriplePressed") == 0)
	{
		FocusVGuiPanel();
		HandleMousePressed(values->GetInt("code", 0));
	}
	else if (std::strcmp(name, "MouseReleased") == 0)
	{
		HandleMouseReleased(values->GetInt("code", 0));
	}
	else if (std::strcmp(name, "MouseWheeled") == 0)
	{
		HandleMouseWheel(values->GetInt("delta", 0));
	}
	else if (std::strcmp(name, "KeyCodePressed") == 0)
	{
		HandleKeyCodePressed(values->GetInt("code", 0));
	}
	else if (std::strcmp(name, "KeyTyped") == 0)
	{
		HandleKeyTyped(values->GetInt("unichar", 0));
	}
}

vgui::VPANEL CParticleEditorWorkspace::GetCurrentKeyFocus()
{
	return m_VPanel;
}

int CParticleEditorWorkspace::GetTabPosition()
{
	return 0;
}

bool CParticleEditorWorkspace::Unknown20()
{
	return false;
}

const char* CParticleEditorWorkspace::GetName()
{
	return "NorthstarParticleEditorWorkspace";
}

const char* CParticleEditorWorkspace::GetClassName()
{
	return "NorthstarParticleEditorWorkspace";
}

vgui::HScheme CParticleEditorWorkspace::GetScheme()
{
	return 0;
}

bool CParticleEditorWorkspace::IsProportional()
{
	return false;
}

bool CParticleEditorWorkspace::IsAutoDeleteSet()
{
	return false;
}

void CParticleEditorWorkspace::DeletePanel()
{
}

void* CParticleEditorWorkspace::QueryInterface(std::uintptr_t interfaceId)
{
	NOTE_UNUSED(interfaceId);
	return nullptr;
}

vgui::Panel* CParticleEditorWorkspace::GetPanel()
{
	return nullptr;
}

const char* CParticleEditorWorkspace::GetModuleName()
{
	return "Northstar.dll";
}

void CParticleEditorWorkspace::OnTick()
{
}

CParticleEditorWorkspace::Layout CParticleEditorWorkspace::CalculateLayout() const
{
	Layout layout;
	layout.m_MenuBar = {0, 0, m_Width, ParticleEditorVGui::MenuHeight};
	layout.m_Toolbar = {0, ParticleEditorVGui::MenuHeight, m_Width,
		ParticleEditorVGui::MenuHeight + ParticleEditorVGui::ToolbarHeight};
	layout.m_Status = {0, std::max(layout.m_Toolbar.m_Y1, m_Height - ParticleEditorVGui::StatusHeight),
		m_Width, m_Height};

	const int bodyTop = layout.m_Toolbar.m_Y1 + ParticleEditorVGui::PaneGap;
	const int bodyBottom = layout.m_Status.m_Y0 - ParticleEditorVGui::PaneGap;
	const int browserWidth = std::clamp(m_Width / 5, 220, 310);
	const int propertiesWidth = std::clamp(m_Width / 4, 300, 430);
	const int centerLeft = browserWidth + ParticleEditorVGui::PaneGap;
	const int centerRight = std::max(centerLeft + 200, m_Width - propertiesWidth - ParticleEditorVGui::PaneGap);
	const int centerSplit = bodyTop + std::max(120, (bodyBottom - bodyTop) * 56 / 100);

	layout.m_Browser = {0, bodyTop, browserWidth, bodyBottom};
	layout.m_Preview = {centerLeft, bodyTop, centerRight, centerSplit - ParticleEditorVGui::PaneGap / 2};
	layout.m_Editor = {centerLeft, centerSplit + ParticleEditorVGui::PaneGap / 2, centerRight, bodyBottom};
	layout.m_Properties = {centerRight + ParticleEditorVGui::PaneGap, bodyTop, m_Width, bodyBottom};
	return layout;
}

CParticleEditorWorkspace::Rect CParticleEditorWorkspace::CalculatePreviewViewport(const Layout& layout) const
{
	const Rect available{
		layout.m_Preview.m_X0 + 2,
		layout.m_Preview.m_Y0 + ParticleEditorVGui::HeaderHeight + 1,
		layout.m_Preview.m_X1 - 2,
		layout.m_Preview.m_Y1 - 2};
	if (available.Width() <= 0 || available.Height() <= 0)
		return available;

	const int widthAtFullHeight =
		available.Height() * ParticleEditorVGui::PreviewAspectWidth / ParticleEditorVGui::PreviewAspectHeight;
	if (widthAtFullHeight <= available.Width())
	{
		const int x = available.m_X0 + (available.Width() - widthAtFullHeight) / 2;
		return {x, available.m_Y0, x + widthAtFullHeight, available.m_Y1};
	}

	const int heightAtFullWidth =
		available.Width() * ParticleEditorVGui::PreviewAspectHeight / ParticleEditorVGui::PreviewAspectWidth;
	const int y = available.m_Y0 + (available.Height() - heightAtFullWidth) / 2;
	return {available.m_X0, y, available.m_X1, y + heightAtFullWidth};
}

void CParticleEditorWorkspace::UpdateBounds()
{
	if (!m_VPanel || !m_pPanelInterface || !vgui::g_pVGuiSurface)
		return;
	int width = 0;
	int height = 0;
	vgui::g_pVGuiSurface->GetScreenSize(width, height);
	if (width <= 0 || height <= 0 || (width == m_Width && height == m_Height))
		return;
	m_Width = width;
	m_Height = height;
	m_pPanelInterface->SetPos(m_VPanel, 0, 0);
	m_pPanelInterface->SetSize(m_VPanel, width, height);
	Repaint();
}

void CParticleEditorWorkspace::Paint()
{
	if (!m_Visible || !vgui::g_pVGuiSurface)
		return;

	int clipX0 = 0;
	int clipY0 = 0;
	int clipX1 = 0;
	int clipY1 = 0;
	vgui::g_pVGuiSurface->GetClipRect(clipX0, clipY0, clipX1, clipY1);
	const bool needsWorkspaceClip = clipX1 <= clipX0 || clipY1 <= clipY0;
	if (needsWorkspaceClip)
		vgui::g_pVGuiSurface->SetClipRect(0, 0, m_Width, m_Height);


	m_HitTargetCount = 0;
	const Layout layout = CalculateLayout();
	PaintMenuBar(layout);
	PaintToolbar(layout);
	PaintBrowser(layout);
	PaintPreview(layout);
	PaintEditor(layout);
	PaintProperties(layout);
	PaintStatus(layout);
	PaintOpenMenu(layout);

	if (needsWorkspaceClip)
		vgui::g_pVGuiSurface->SetClipRect(clipX0, clipY0, clipX1, clipY1);

}

void CParticleEditorWorkspace::PaintMenuBar(const Layout& layout)
{
	DrawFilledRect(layout.m_MenuBar, 39, 43, 50);
	DrawOutlinedRect(layout.m_MenuBar, 70, 76, 87);
	const std::array<std::pair<std::string_view, Action>, 4> menus = {{
		{"File", Action::FileMenu}, {"Edit", Action::EditMenu}, {"View", Action::ViewMenu}, {"Particle", Action::ParticleMenu}}};
	int x = 6;
	for (std::size_t index = 0; index < menus.size(); ++index)
	{
		const int width = index == 3 ? 72 : 54;
		const bool selected = static_cast<int>(m_OpenMenu) == static_cast<int>(index) + 1;
		DrawButton({x, 2, x + width, layout.m_MenuBar.m_Y1 - 2}, menus[index].first, menus[index].second, 0, selected);
		x += width + 2;
	}
	std::string title = m_Path.empty() ? "untitled - Particle Editor" :
		ParticleEditorVGui::PathToUtf8(m_Path.filename()) + " - Particle Editor";
	if (m_Dirty)
		title.insert(0, "* ");
	DrawText(std::max(x + 8, m_Width - 300), 6, title, 190, 198, 212, 255, m_BoldFont);
}

void CParticleEditorWorkspace::PaintToolbar(const Layout& layout)
{
	DrawFilledRect(layout.m_Toolbar, 30, 34, 40);
	const int y0 = layout.m_Toolbar.m_Y0 + 5;
	const int y1 = layout.m_Toolbar.m_Y1 - 5;
	int x = 6;
	DrawButton({x, y0, x + 50, y1}, "New", Action::NewDocument); x += 54;
	DrawButton({x, y0, x + 50, y1}, "Open", Action::OpenDocument); x += 54;
	DrawButton({x, y0, x + 50, y1}, "Save", Action::SaveDocument); x += 54;
	DrawButton({x, y0, x + 60, y1}, "Preview", Action::Preview); x += 64;
	DrawButton({x, y0, x + 46, y1}, "Stop", Action::StopPreview); x += 52;
	const int effectWidth = std::clamp(m_Width / 7, 130, 220);
	DrawTextField({x, y0, x + effectWidth, y1}, "Effect: ", TextField::EffectName, m_EffectNameText);
	x += effectWidth + 5;
	const Rect assetRect{x, y0, m_Width - 6, y1};
	DrawFilledRect(assetRect, 20, 23, 28);
	DrawOutlinedRect(assetRect, 72, 80, 93);
	DrawClippedText(assetRect, assetRect.m_X0 + 6, assetRect.m_Y0 + 5,
		std::string("Asset: ") + (m_Path.empty() ? "untitled" : m_PathText), 211, 216, 225);
}

void CParticleEditorWorkspace::PaintBrowser(const Layout& layout)
{
	DrawFilledRect(layout.m_Browser, 27, 30, 35);
	DrawOutlinedRect(layout.m_Browser, 70, 76, 87);
	DrawFilledRect({layout.m_Browser.m_X0, layout.m_Browser.m_Y0, layout.m_Browser.m_X1,
		layout.m_Browser.m_Y0 + ParticleEditorVGui::HeaderHeight}, 46, 51, 59);
	DrawText(layout.m_Browser.m_X0 + 8, layout.m_Browser.m_Y0 + 6, "Particle Systems", 225, 228, 234, 255, m_BoldFont);

	const int actionTop = layout.m_Browser.m_Y1 - 62;
	const int availableRows = std::max(0, (actionTop - layout.m_Browser.m_Y0 -
		ParticleEditorVGui::HeaderHeight - 2) / ParticleEditorVGui::RowHeight);
	m_BrowserScroll = std::min(m_BrowserScroll,
		m_BrowserRows.size() > static_cast<std::size_t>(availableRows) ? m_BrowserRows.size() - availableRows : 0);
	for (int visibleRow = 0; visibleRow < availableRows; ++visibleRow)
	{
		const std::size_t index = m_BrowserScroll + static_cast<std::size_t>(visibleRow);
		if (index >= m_BrowserRows.size())
			break;
		const BrowserRow& row = m_BrowserRows[index];
		const int y = layout.m_Browser.m_Y0 + ParticleEditorVGui::HeaderHeight +
			visibleRow * ParticleEditorVGui::RowHeight;
		const Rect bounds{layout.m_Browser.m_X0 + 2, y, layout.m_Browser.m_X1 - 2,
			y + ParticleEditorVGui::RowHeight};
		const bool selected = row.m_ElementId && m_SelectedElementId && *row.m_ElementId == *m_SelectedElementId &&
			row.m_Category == m_SelectedCategory;
		if (selected)
			DrawFilledRect(bounds, 54, 82, 112);
		else if ((visibleRow & 1) != 0)
			DrawFilledRect(bounds, 31, 34, 40);
		const int labelX = bounds.m_X0 + 16 + row.m_Indent * 14;
		if (row.m_Collapsible)
		{
			const Rect toggle{bounds.m_X0 + 2 + row.m_Indent * 14, bounds.m_Y0 + 3,
				bounds.m_X0 + 14 + row.m_Indent * 14, bounds.m_Y0 + 15};
			DrawOutlinedRect(toggle, 128, 138, 154);
			DrawText(toggle.m_X0 + 3, toggle.m_Y0 + 1, BrowserRowCollapsed(row) ? "+" : "-", 196, 207, 222, 255, m_BoldFont);
			// Added after the row target so FindHitTarget (last-first) resolves the
			// toggle before the row's select action.
			AddHitTarget(bounds, Action::SelectBrowserRow, index);
			AddHitTarget(toggle, Action::ToggleBrowserRow, index);
		}
		else
		{
			AddHitTarget(bounds, Action::SelectBrowserRow, index);
		}
		DrawClippedText(bounds, labelX, bounds.m_Y0 + 3, row.m_Label,
			row.m_CategoryRow ? 144 : 210, row.m_CategoryRow ? 178 : 214, row.m_CategoryRow ? 208 : 220);
	}

	const int left = layout.m_Browser.m_X0 + 4;
	const int right = layout.m_Browser.m_X1 - 4;
	const int firstRowBottom = actionTop + 27;
	const int halfWidth = (right - left - 4) / 2;
	DrawButton({left, actionTop, left + halfWidth, firstRowBottom}, "Save", Action::SaveDocument);
	DrawButton({left + halfWidth + 4, actionTop, right, firstRowBottom}, "Save & Preview", Action::SaveAndPreview);
	const int secondRowTop = firstRowBottom + 4;
	const int thirdWidth = (right - left - 8) / 3;
	DrawButton({left, secondRowTop, left + thirdWidth, layout.m_Browser.m_Y1 - 4},
		"Create", Action::CreateParticleSystem);
	DrawButton({left + thirdWidth + 4, secondRowTop, left + thirdWidth * 2 + 4, layout.m_Browser.m_Y1 - 4},
		"Duplicate", Action::DuplicateParticleSystem);
	DrawButton({left + thirdWidth * 2 + 8, secondRowTop, right, layout.m_Browser.m_Y1 - 4},
		"Delete", Action::DeleteParticleSystem);

}

void CParticleEditorWorkspace::PaintPreview(const Layout& layout)
{
	const Rect availablePreviewBody{
		layout.m_Preview.m_X0 + 2,
		layout.m_Preview.m_Y0 + ParticleEditorVGui::HeaderHeight + 1,
		layout.m_Preview.m_X1 - 2,
		layout.m_Preview.m_Y1 - 2};
	const Rect previewBody = CalculatePreviewViewport(layout);
	DrawFilledRect(
		{availablePreviewBody.m_X0, availablePreviewBody.m_Y0, previewBody.m_X0, availablePreviewBody.m_Y1},
		8, 10, 13);
	DrawFilledRect(
		{previewBody.m_X1, availablePreviewBody.m_Y0, availablePreviewBody.m_X1, availablePreviewBody.m_Y1},
		8, 10, 13);
	DrawFilledRect(
		{previewBody.m_X0, availablePreviewBody.m_Y0, previewBody.m_X1, previewBody.m_Y0},
		8, 10, 13);
	DrawFilledRect(
		{previewBody.m_X0, previewBody.m_Y1, previewBody.m_X1, availablePreviewBody.m_Y1},
		8, 10, 13);
	if (!m_PreviewEnabled.load(std::memory_order_acquire))
	{
		DrawFilledRect(previewBody, 8, 10, 13);
		DrawText(
			previewBody.m_X0 + 12,
			previewBody.m_Y0 + 12,
			"Waiting for an active client level",
			135,
			145,
			160);
	}
	DrawOutlinedRect(previewBody, 70, 76, 87);
	DrawOutlinedRect(layout.m_Preview, 86, 94, 108);
	DrawFilledRect({layout.m_Preview.m_X0 + 1, layout.m_Preview.m_Y0 + 1, layout.m_Preview.m_X1 - 1,
		layout.m_Preview.m_Y0 + ParticleEditorVGui::HeaderHeight}, 38, 43, 50, 245);
	DrawText(layout.m_Preview.m_X0 + 8, layout.m_Preview.m_Y0 + 6,
		"Live Engine Preview", 225, 228, 234, 255, m_BoldFont);
	DrawText(layout.m_Preview.m_X1 - 306, layout.m_Preview.m_Y0 + 6,
		"LMB: orbit | MMB: pan | wheel: zoom", 145, 154, 168);
}

void CParticleEditorWorkspace::PaintEditor(const Layout& layout)
{
	DrawFilledRect(layout.m_Editor, 27, 30, 35);
	DrawOutlinedRect(layout.m_Editor, 70, 76, 87);
	const int tabY0 = layout.m_Editor.m_Y0 + 2;
	const int tabY1 = tabY0 + 24;
	DrawButton({layout.m_Editor.m_X0 + 4, tabY0, layout.m_Editor.m_X0 + 118, tabY1}, "Components",
		Action::SelectComponentTab, 0, m_EditorTab == EditorTab::Components);
	DrawButton({layout.m_Editor.m_X0 + 121, tabY0, layout.m_Editor.m_X0 + 247, tabY1}, "Control Points",
		Action::SelectControlPointTab, 0, m_EditorTab == EditorTab::ControlPoints);

	const int left = layout.m_Editor.m_X0 + 8;
	const int right = layout.m_Editor.m_X1 - 8;
	int y = tabY1 + 8;
	if (m_EditorTab == EditorTab::Components)
	{
		DrawText(left, y + 5, "Category", 164, 171, 184); y += 22;
		DrawButton({left, y, left + 28, y + 25}, "<", Action::PreviousComponentCategory);
		DrawFilledRect({left + 32, y, right - 32, y + 25}, 36, 41, 48);
		DrawOutlinedRect({left + 32, y, right - 32, y + 25}, 76, 84, 97);
		DrawText(left + 40, y + 5, ParticleEditorVGui::ComponentCategories[m_ComponentCategoryIndex], 224, 228, 235, 255, m_BoldFont);
		DrawButton({right - 28, y, right, y + 25}, ">", Action::NextComponentCategory); y += 32;
		DrawTextField({left, y, right, y + 27}, "Function: ", TextField::ComponentFunction, m_ComponentFunctionText); y += 34;
		int buttonX = left;
		DrawButton({buttonX, y, buttonX + 66, y + 26}, "Add", Action::AddComponent); buttonX += 70;
		DrawButton({buttonX, y, buttonX + 74, y + 26}, "Remove", Action::RemoveComponent); buttonX += 78;
		DrawButton({buttonX, y, buttonX + 48, y + 26}, "Up", Action::MoveComponentUp); buttonX += 52;
		DrawButton({buttonX, y, buttonX + 56, y + 26}, "Down", Action::MoveComponentDown); y += 36;
		DrawText(left, y, "Presets", 164, 171, 184); y += 20;
		for (std::size_t index = 0; index < ParticleEditorVGui::ComponentPresets.size(); ++index)
		{
			const int presetWidth = std::max(100, (right - left - 6) / 2);
			const int column = static_cast<int>(index % 2);
			const int row = static_cast<int>(index / 2);
			const int x0 = left + column * (presetWidth + 4);
			DrawButton({x0, y + row * 30, std::min(right, x0 + presetWidth), y + row * 30 + 26},
				ParticleEditorVGui::ComponentPresets[index].m_Label, Action::SelectComponentPreset, index);
		}
	}
	else
	{
		const int listWidth = std::max(150, (right - left) * 42 / 100);
		DrawText(left, y, "Preview control points", 164, 171, 184); y += 21;
		const int listTop = y;
		const int listBottom = std::max(listTop + 50, layout.m_Editor.m_Y1 - 40);
		DrawFilledRect({left, listTop, left + listWidth, listBottom}, 22, 25, 30);
		DrawOutlinedRect({left, listTop, left + listWidth, listBottom}, 70, 76, 87);
		for (std::size_t index = 0; index < m_ControlPoints.size(); ++index)
		{
			const int rowY = listTop + 2 + static_cast<int>(index) * ParticleEditorVGui::RowHeight;
			if (rowY + ParticleEditorVGui::RowHeight > listBottom)
				break;
			const Rect rowBounds{left + 2, rowY, left + listWidth - 2, rowY + ParticleEditorVGui::RowHeight};
			if (m_SelectedControlPoint && *m_SelectedControlPoint == index)
				DrawFilledRect(rowBounds, 54, 82, 112);
			std::ostringstream label;
			label << "CP " << m_ControlPoints[index].m_Index << "  pos "
				<< m_ControlPoints[index].m_Position[0] << ' ' << m_ControlPoints[index].m_Position[1] << ' '
				<< m_ControlPoints[index].m_Position[2];
			DrawClippedText(rowBounds, rowBounds.m_X0 + 5, rowBounds.m_Y0 + 3, label.str(), 211, 216, 225);
			AddHitTarget(rowBounds, Action::SelectControlPoint, index);
		}

		const int fieldLeft = left + listWidth + 9;
		int fieldY = listTop;
		DrawTextField({fieldLeft, fieldY, right, fieldY + 23}, "Index: ", TextField::ControlPointIndex, m_ControlPointIndexText); fieldY += 27;
		const std::array<std::pair<std::string_view, TextField>, 3> positions = {{
			{"Pos X: ", TextField::ControlPointPositionX}, {"Pos Y: ", TextField::ControlPointPositionY},
			{"Pos Z: ", TextField::ControlPointPositionZ}}};
		for (std::size_t axis = 0; axis < positions.size(); ++axis)
		{
			DrawTextField({fieldLeft, fieldY, right, fieldY + 23}, positions[axis].first, positions[axis].second,
				m_ControlPointPositionText[axis]);
			fieldY += 27;
		}
		const std::array<std::pair<std::string_view, TextField>, 3> angles = {{
			{"Pitch: ", TextField::ControlPointPitch}, {"Yaw: ", TextField::ControlPointYaw},
			{"Roll: ", TextField::ControlPointRoll}}};
		for (std::size_t axis = 0; axis < angles.size(); ++axis)
		{
			DrawTextField({fieldLeft, fieldY, right, fieldY + 23}, angles[axis].first, angles[axis].second,
				m_ControlPointAnglesText[axis]);
			fieldY += 27;
		}
		int buttonX = fieldLeft;
		DrawButton({buttonX, fieldY, buttonX + 48, fieldY + 24}, "Add", Action::AddControlPoint); buttonX += 52;
		DrawButton({buttonX, fieldY, buttonX + 56, fieldY + 24}, "Apply", Action::ApplyControlPoint); buttonX += 60;
		DrawButton({buttonX, fieldY, std::min(right, buttonX + 70), fieldY + 24}, "Remove", Action::RemoveControlPoint);
	}
}

void CParticleEditorWorkspace::PaintProperties(const Layout& layout)
{
	DrawFilledRect(layout.m_Properties, 27, 30, 35);
	DrawOutlinedRect(layout.m_Properties, 70, 76, 87);
	DrawFilledRect({layout.m_Properties.m_X0, layout.m_Properties.m_Y0, layout.m_Properties.m_X1,
		layout.m_Properties.m_Y0 + ParticleEditorVGui::HeaderHeight}, 46, 51, 59);
	DrawText(layout.m_Properties.m_X0 + 8, layout.m_Properties.m_Y0 + 6, "Properties", 225, 228, 234, 255, m_BoldFont);

	DmxElement* element = SelectedElement();
	const int left = layout.m_Properties.m_X0 + 6;
	const int right = layout.m_Properties.m_X1 - 6;
	const int editorHeight = 137;
	const int listTop = layout.m_Properties.m_Y0 + ParticleEditorVGui::HeaderHeight + 2;
	const int listBottom = std::max(listTop + 20, layout.m_Properties.m_Y1 - editorHeight);
	DrawFilledRect({left, listTop, right, listBottom}, 22, 25, 30);
	DrawOutlinedRect({left, listTop, right, listBottom}, 65, 71, 82);
	if (!element)
	{
		DrawText(left + 6, listTop + 6, "Select a document element", 160, 166, 178);
		return;
	}

	const std::size_t propertyCount = element->m_Attributes.size() + 1;
	const int visibleRows = std::max(1, (listBottom - listTop - 2) / ParticleEditorVGui::RowHeight);
	m_PropertyScroll = std::min(m_PropertyScroll,
		propertyCount > static_cast<std::size_t>(visibleRows) ? propertyCount - visibleRows : 0);
	for (int visibleRow = 0; visibleRow < visibleRows; ++visibleRow)
	{
		const std::size_t row = m_PropertyScroll + static_cast<std::size_t>(visibleRow);
		if (row >= propertyCount)
			break;
		const int y = listTop + 1 + visibleRow * ParticleEditorVGui::RowHeight;
		const Rect bounds{left + 1, y, right - 1, y + ParticleEditorVGui::RowHeight};
		const bool selected = row == 0 ? m_SelectedElementName :
			(m_SelectedAttributeIndex && *m_SelectedAttributeIndex == row - 1 && !m_SelectedElementName);
		if (selected)
			DrawFilledRect(bounds, 54, 82, 112);
		else if ((visibleRow & 1) != 0)
			DrawFilledRect(bounds, 29, 33, 39);
		std::string name;
		std::string type;
		std::string value;
		if (row == 0)
		{
			name = "name";
			type = "element";
			value = element->m_Name;
		}
		else
		{
			const DmxAttribute& attribute = element->m_Attributes[row - 1];
			name = attribute.m_Name;
			type = ParticleEditorVGui::AttributeTypeNames[ParticleEditorVGui::TypeIndex(attribute.m_Type)];
			value = attribute.IsElementReference() ? ParticleEditorVGui::ReferenceText(attribute) : attribute.m_Value;
		}
		const int nameWidth = std::max(95, bounds.Width() * 38 / 100);
		DrawClippedText({bounds.m_X0, bounds.m_Y0, bounds.m_X0 + nameWidth, bounds.m_Y1},
			bounds.m_X0 + 4, bounds.m_Y0 + 3, name, 214, 218, 226);
		DrawClippedText({bounds.m_X0 + nameWidth, bounds.m_Y0, bounds.m_X0 + nameWidth + 75, bounds.m_Y1},
			bounds.m_X0 + nameWidth + 3, bounds.m_Y0 + 3, type, 143, 181, 207);
		DrawClippedText({bounds.m_X0 + nameWidth + 75, bounds.m_Y0, bounds.m_X1, bounds.m_Y1},
			bounds.m_X0 + nameWidth + 79, bounds.m_Y0 + 3, value, 185, 191, 202);
		AddHitTarget(bounds, Action::SelectPropertyRow, row);
	}

	int y = listBottom + 6;
	DrawTextField({left, y, right, y + 26}, "Name: ", TextField::AttributeName, m_AttributeNameText); y += 31;
	DrawButton({left, y, left + 28, y + 25}, "<", Action::PreviousAttributeType);
	DrawFilledRect({left + 32, y, right - 32, y + 25}, 36, 41, 48);
	DrawOutlinedRect({left + 32, y, right - 32, y + 25}, 76, 84, 97);
	DrawText(left + 39, y + 5, ParticleEditorVGui::AttributeTypeNames[ParticleEditorVGui::TypeIndex(m_AttributeType)],
		205, 213, 224);
	DrawButton({right - 28, y, right, y + 25}, ">", Action::NextAttributeType); y += 31;
	DrawTextField({left, y, right, y + 28}, "Value: ", TextField::AttributeValue, m_AttributeValueText); y += 34;
	int buttonX = left;
	DrawButton({buttonX, y, buttonX + 48, y + 26}, "Add", Action::AddAttribute); buttonX += 52;
	DrawButton({buttonX, y, buttonX + 58, y + 26}, "Apply", Action::ApplyAttribute); buttonX += 62;
	DrawButton({buttonX, y, std::min(right, buttonX + 72), y + 26}, "Remove", Action::RemoveAttribute);
}

void CParticleEditorWorkspace::PaintStatus(const Layout& layout)
{
	DrawFilledRect(layout.m_Status, 38, 43, 50);
	DrawOutlinedRect(layout.m_Status, 70, 76, 87);
	DrawClippedText(layout.m_Status, layout.m_Status.m_X0 + 8, layout.m_Status.m_Y0 + 5,
		m_Status, 206, 213, 223);
	if (m_Dirty)
		DrawText(layout.m_Status.m_X1 - 92, layout.m_Status.m_Y0 + 5, "Modified", 242, 183, 89, 255, m_BoldFont);
}

void CParticleEditorWorkspace::PaintOpenMenu(const Layout& layout)
{
	NOTE_UNUSED(layout);
	if (m_OpenMenu == OpenMenu::None)
		return;
	int x = 6;
	int width = 170;
	std::vector<std::pair<std::string_view, Action>> items;
	switch (m_OpenMenu)
	{
	case OpenMenu::File:
		items = {{"New", Action::NewDocument}, {"Open...", Action::OpenDocument}, {"Save", Action::SaveDocument},
			{"Save As...", Action::SaveDocumentAs}, {"Close", Action::CloseDocument},
			{"Return to Game", Action::HideWorkspace}};
		break;
	case OpenMenu::Edit:
		x = 62;
		items = {{"Add attribute", Action::AddAttribute}, {"Apply attribute", Action::ApplyAttribute},
			{"Remove attribute", Action::RemoveAttribute}};
		break;
	case OpenMenu::View:
		x = 118;
		items = {{"Components", Action::SelectComponentTab}, {"Control points", Action::SelectControlPointTab}};
		break;
	case OpenMenu::Particle:
		x = 174;
		items = {{"Start Preview", Action::Preview}, {"Stop Preview", Action::StopPreview},
			{"Save and Preview", Action::SaveAndPreview}};
		break;
	default:
		return;
	}
	const int y0 = ParticleEditorVGui::MenuHeight;
	const int y1 = y0 + static_cast<int>(items.size()) * 27 + 4;
	DrawFilledRect({x, y0, x + width, y1}, 35, 39, 46);
	DrawOutlinedRect({x, y0, x + width, y1}, 91, 99, 113);
	for (std::size_t index = 0; index < items.size(); ++index)
	{
		const int y = y0 + 2 + static_cast<int>(index) * 27;
		DrawButton({x + 2, y, x + width - 2, y + 25}, items[index].first, items[index].second);
	}
}

void CParticleEditorWorkspace::DrawFilledRect(const Rect& rect, int red, int green, int blue, int alpha) const
{
	if (rect.Width() <= 0 || rect.Height() <= 0)
		return;
	vgui::g_pVGuiSurface->DrawSetColor(red, green, blue, alpha);


	vgui::g_pVGuiSurface->DrawFilledRect(rect.m_X0, rect.m_Y0, rect.m_X1, rect.m_Y1);
}

void CParticleEditorWorkspace::DrawOutlinedRect(const Rect& rect, int red, int green, int blue, int alpha) const
{
	if (rect.Width() <= 0 || rect.Height() <= 0)
		return;
	vgui::g_pVGuiSurface->DrawSetColor(red, green, blue, alpha);
	vgui::g_pVGuiSurface->DrawOutlinedRect(rect.m_X0, rect.m_Y0, rect.m_X1, rect.m_Y1);
}

void CParticleEditorWorkspace::DrawText(int x, int y, std::string_view text, int red, int green, int blue, int alpha,
	unsigned long font) const
{
	if (text.empty())
		return;
	const std::wstring wide = ParticleEditorVGui::ToWide(text);
	if (wide.empty())
		return;
	vgui::g_pVGuiSurface->DrawSetTextFont(font ? font : m_Font);
	vgui::g_pVGuiSurface->DrawSetTextColor(red, green, blue, alpha);
	vgui::g_pVGuiSurface->DrawSetTextPos(x, y);
	vgui::g_pVGuiSurface->DrawPrintText(wide.c_str(), static_cast<int>(wide.size()));
}

void CParticleEditorWorkspace::DrawClippedText(const Rect& rect, int x, int y, std::string_view text,
	int red, int green, int blue, int alpha) const
{
	if (x >= rect.m_X1)
		return;
	const int approximateCharacters = std::max(0, (rect.m_X1 - x - 3) / 7);
	if (approximateCharacters <= 0)
		return;
	if (text.size() <= static_cast<std::size_t>(approximateCharacters))
	{
		DrawText(x, y, text, red, green, blue, alpha);
		return;
	}
	std::string clipped(text.substr(0, static_cast<std::size_t>(std::max(1, approximateCharacters - 3))));
	clipped += "...";
	DrawText(x, y, clipped, red, green, blue, alpha);
}

void CParticleEditorWorkspace::DrawButton(const Rect& rect, std::string_view label, Action action, std::size_t index,
	bool selected)
{
	const int targetIndex = static_cast<int>(m_HitTargetCount);
	const bool pressed = targetIndex == m_PressedHitTarget;
	if (selected)
		DrawFilledRect(rect, 57, 85, 116);
	else if (pressed)
		DrawFilledRect(rect, 67, 73, 84);
	else
		DrawFilledRect(rect, 46, 51, 59);
	DrawOutlinedRect(rect, selected ? 104 : 79, selected ? 151 : 87, selected ? 190 : 100);
	DrawClippedText(rect, rect.m_X0 + 7, rect.m_Y0 + 5, label, 225, 229, 236);
	AddHitTarget(rect, action, index);
}

void CParticleEditorWorkspace::DrawTextField(const Rect& rect, std::string_view label, TextField field, const std::string& value)
{
	DrawFilledRect(rect, 20, 23, 28);
	const bool active = m_ActiveTextField == field;
	DrawOutlinedRect(rect, active ? 88 : 72, active ? 145 : 80, active ? 190 : 93);
	std::string display(label);
	display += value;
	if (active)
		display += "|";
	DrawClippedText(rect, rect.m_X0 + 6, rect.m_Y0 + 5, display, active ? 236 : 211, active ? 240 : 216, active ? 246 : 225);
	AddHitTarget(rect, Action::EditField, 0, field);
}

void CParticleEditorWorkspace::AddHitTarget(const Rect& rect, Action action, std::size_t index, TextField field)
{
	if (m_HitTargetCount >= m_HitTargets.size())
		return;
	m_HitTargets[m_HitTargetCount++] = {rect, action, index, field};
}

int CParticleEditorWorkspace::FindHitTarget(int x, int y) const
{
	for (std::size_t reverse = m_HitTargetCount; reverse > 0; --reverse)
	{
		if (m_HitTargets[reverse - 1].m_Bounds.Contains(x, y))
			return static_cast<int>(reverse - 1);
	}
	return -1;
}

void CParticleEditorWorkspace::HandleCursorMoved(int x, int y)
{
	NOTE_UNUSED(x);
	NOTE_UNUSED(y);
	// Orbiting is driven from Think() via SurfaceGetCursorPos; the engine's
	// "CursorMoved" message coordinates freeze once a panel captures the mouse.
}

void CParticleEditorWorkspace::HandleMousePressed(int code)
{
	if (!vgui::g_pVGuiSurface)
		return;
	int x = 0;
	int y = 0;
	vgui::g_pVGuiSurface->SurfaceGetCursorPos(x, y);
	const Rect previewBody = CalculatePreviewViewport(CalculateLayout());
	if (m_PreviewEnabled.load(std::memory_order_acquire) && previewBody.Contains(x, y) &&
		(code == ParticleEditorVGui::R2VGuiMouseLeft || code == ParticleEditorVGui::R2VGuiMouseMiddle))
	{
		EndTextEdit(true);
		m_OpenMenu = OpenMenu::None;
		m_PressedHitTarget = -1;
		m_PreviewCursorX = x;
		m_PreviewCursorY = y;
		m_PreviewPanning = code == ParticleEditorVGui::R2VGuiMouseMiddle;
		m_PreviewOrbiting = !m_PreviewPanning;
		Repaint();
		return;
	}
	if (code != ParticleEditorVGui::R2VGuiMouseLeft)
		return;

	m_PreviewOrbiting = false;
	m_PreviewPanning = false;
	m_PressedHitTarget = FindHitTarget(x, y);
	if (m_PressedHitTarget < 0)
	{
		EndTextEdit(true);
		m_OpenMenu = OpenMenu::None;
	}
	m_ToolSystem.SetEditorMouseCapture(true);
	Repaint();
}

void CParticleEditorWorkspace::HandleMouseReleased(int code)
{
	if (!vgui::g_pVGuiSurface)
		return;
	if (m_PreviewOrbiting || m_PreviewPanning)
	{
		m_PreviewOrbiting = false;
		m_PreviewPanning = false;
		m_PressedHitTarget = -1;
		m_ToolSystem.SetEditorMouseCapture(false);
		Repaint();
		return;
	}
	if (code != ParticleEditorVGui::R2VGuiMouseLeft)
		return;

	int x = 0;
	int y = 0;
	vgui::g_pVGuiSurface->SurfaceGetCursorPos(x, y);
	const int released = FindHitTarget(x, y);
	const int pressed = m_PressedHitTarget;
	m_PressedHitTarget = -1;
	m_ToolSystem.SetEditorMouseCapture(false);
	if (pressed >= 0 && released == pressed && static_cast<std::size_t>(pressed) < m_HitTargetCount)
		ExecuteAction(m_HitTargets[static_cast<std::size_t>(pressed)]);
	Repaint();
}

void CParticleEditorWorkspace::HandleMouseWheel(int delta)
{
	if (!vgui::g_pVGuiSurface || delta == 0)
		return;
	int x = 0;
	int y = 0;
	vgui::g_pVGuiSurface->SurfaceGetCursorPos(x, y);
	const Layout layout = CalculateLayout();
	const Rect previewBody = CalculatePreviewViewport(layout);
	if (m_PreviewEnabled.load(std::memory_order_acquire) && previewBody.Contains(x, y))
	{
		m_ToolSystem.AdjustPreviewCamera(0.0f, 0.0f, delta > 0 ? 1.0f : -1.0f);
		Repaint();
		return;
	}

	const int direction = delta > 0 ? -1 : 1;
	if (layout.m_Browser.Contains(x, y))
	{
		if (direction < 0 && m_BrowserScroll > 0)
			--m_BrowserScroll;
		else if (direction > 0 && m_BrowserScroll + 1 < m_BrowserRows.size())
			++m_BrowserScroll;
	}
	else if (layout.m_Properties.Contains(x, y))
	{
		DmxElement* element = SelectedElement();
		const std::size_t count = element ? element->m_Attributes.size() + 1 : 0;
		if (direction < 0 && m_PropertyScroll > 0)
			--m_PropertyScroll;
		else if (direction > 0 && m_PropertyScroll + 1 < count)
			++m_PropertyScroll;
	}
	Repaint();
}

void CParticleEditorWorkspace::HandleKeyCodePressed(int code)
{
	const bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
	if (m_ActiveTextField != TextField::None)
	{
		std::string* value = GetTextField(m_ActiveTextField);
		if (!value)
			return;
		if (code == KEY_ESCAPE)
		{
			EndTextEdit(false);
			return;
		}
		if (code == KEY_ENTER)
		{
			EndTextEdit(true);
			return;
		}
		if (control && code == KEY_A)
		{
			m_SelectAllText = true;
			return;
		}
		if (control && code == KEY_V)
		{
			PasteClipboardText();
			return;
		}
		if (!control && code == KEY_SPACE)
		{
			HandleKeyTyped(' ');
			m_SkipNextTypedSpace = true;
			return;
		}
		if (code == KEY_BACKSPACE)
		{
			if (m_SelectAllText)
				value->clear();
			else if (!value->empty())
				value->pop_back();
			m_SelectAllText = false;
			return;
		}
		if (code == KEY_DELETE && m_SelectAllText)
		{
			value->clear();
			m_SelectAllText = false;
			return;
		}
	}
	else if (control)
	{
		if (code == KEY_N)
			RequestNewDocument();
		else if (code == KEY_O)
			RequestOpenDocument();
		else if (code == KEY_S)
			SaveDocument((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
		else if (code == KEY_W)
			RequestCloseDocument();
	}
	else if (code == KEY_ESCAPE)
	{
		SetVisible(false);
	}
}

void CParticleEditorWorkspace::HandleKeyTyped(int character)
{
	if (m_SkipNextTypedSpace)
	{
		m_SkipNextTypedSpace = false;
		if (character == ' ')
			return;
	}
	if (m_ActiveTextField == TextField::None || character < 32 || character == 127)
		return;
	std::string* value = GetTextField(m_ActiveTextField);
	if (!value)
		return;
	if (m_SelectAllText)
	{
		value->clear();
		m_SelectAllText = false;
	}

	wchar_t wide = static_cast<wchar_t>(character);
	if ((wide >= L'a' && wide <= L'z') || (wide >= L'A' && wide <= L'Z'))
	{
		const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		const bool capsLock = (GetKeyState(VK_CAPITAL) & 1) != 0;
		const bool uppercase = shift != capsLock;
		const wchar_t letter = wide >= L'a' ? wide : static_cast<wchar_t>(wide - L'A' + L'a');
		wide = uppercase ? static_cast<wchar_t>(letter - L'a' + L'A') : letter;
	}
	value->append(ParticleEditorVGui::ToUtf8(std::wstring_view(&wide, 1)));
}

void CParticleEditorWorkspace::ExecuteAction(const HitTarget& target)
{
	if (target.m_Action != Action::EditField)
		EndTextEdit(true);
	switch (target.m_Action)
	{
	case Action::FileMenu: m_OpenMenu = m_OpenMenu == OpenMenu::File ? OpenMenu::None : OpenMenu::File; return;
	case Action::EditMenu: m_OpenMenu = m_OpenMenu == OpenMenu::Edit ? OpenMenu::None : OpenMenu::Edit; return;
	case Action::ViewMenu: m_OpenMenu = m_OpenMenu == OpenMenu::View ? OpenMenu::None : OpenMenu::View; return;
	case Action::ParticleMenu: m_OpenMenu = m_OpenMenu == OpenMenu::Particle ? OpenMenu::None : OpenMenu::Particle; return;
	case Action::NewDocument: RequestNewDocument(); break;
	case Action::OpenDocument: RequestOpenDocument(); break;
	case Action::SaveDocument: SaveDocument(false); break;
	case Action::SaveDocumentAs: SaveDocument(true); break;
	case Action::CloseDocument: RequestCloseDocument(); break;
	case Action::SaveAndPreview:
		if (SaveDocument(false))
			PreviewDocument();
		break;
	case Action::CreateParticleSystem: CreateParticleSystem(); break;
	case Action::DuplicateParticleSystem: DuplicateParticleSystem(); break;
	case Action::DeleteParticleSystem: DeleteParticleSystem(); break;
	case Action::Preview: PreviewDocument(); break;
	case Action::StopPreview: StopPreview(); break;
	case Action::HideWorkspace: SetVisible(false); break;
	case Action::SelectBrowserRow: SelectBrowserRow(target.m_Index); break;
	case Action::ToggleBrowserRow: ToggleBrowserRow(target.m_Index); break;
	case Action::SelectPropertyRow: SelectPropertyRow(target.m_Index); break;
	case Action::SelectComponentTab: m_EditorTab = EditorTab::Components; break;
	case Action::SelectControlPointTab: m_EditorTab = EditorTab::ControlPoints; break;
	case Action::PreviousComponentCategory:
		m_ComponentCategoryIndex = (m_ComponentCategoryIndex + ParticleEditorVGui::ComponentCategories.size() - 1) %
			ParticleEditorVGui::ComponentCategories.size();
		break;
	case Action::NextComponentCategory:
		m_ComponentCategoryIndex = (m_ComponentCategoryIndex + 1) % ParticleEditorVGui::ComponentCategories.size();
		break;
	case Action::AddComponent: AddComponent(); break;
	case Action::RemoveComponent: RemoveComponent(); break;
	case Action::MoveComponentUp: MoveComponent(-1); break;
	case Action::MoveComponentDown: MoveComponent(1); break;
	case Action::SelectComponentPreset:
		if (target.m_Index < ParticleEditorVGui::ComponentPresets.size())
		{
			const ParticleEditorVGui::ComponentPreset& preset = ParticleEditorVGui::ComponentPresets[target.m_Index];
			const auto iterator = std::find(ParticleEditorVGui::ComponentCategories.begin(),
				ParticleEditorVGui::ComponentCategories.end(), preset.m_Category);
			if (iterator != ParticleEditorVGui::ComponentCategories.end())
				m_ComponentCategoryIndex = static_cast<std::size_t>(iterator - ParticleEditorVGui::ComponentCategories.begin());
			m_ComponentFunctionText = preset.m_Function;
			SetStatus("Preset selected; click Add to create the component");
		}
		break;
	case Action::PreviousAttributeType:
	{
		const std::size_t index = ParticleEditorVGui::TypeIndex(m_AttributeType);
		m_AttributeType = static_cast<DmAttributeType_t>((index + ParticleEditorVGui::AttributeTypeNames.size() - 1) %
			ParticleEditorVGui::AttributeTypeNames.size() + 1);
		break;
	}
	case Action::NextAttributeType:
	{
		const std::size_t index = ParticleEditorVGui::TypeIndex(m_AttributeType);
		m_AttributeType = static_cast<DmAttributeType_t>((index + 1) % ParticleEditorVGui::AttributeTypeNames.size() + 1);
		break;
	}
	case Action::AddAttribute: AddAttribute(); break;
	case Action::ApplyAttribute: ApplyAttribute(); break;
	case Action::RemoveAttribute: RemoveAttribute(); break;
	case Action::SelectControlPoint: SelectControlPoint(target.m_Index); break;
	case Action::AddControlPoint: AddControlPoint(); break;
	case Action::ApplyControlPoint: ApplyControlPoint(); break;
	case Action::RemoveControlPoint: RemoveControlPoint(); break;
	case Action::EditField: BeginTextEdit(target.m_Field); return;
	default: break;
	}
	m_OpenMenu = OpenMenu::None;
}

void CParticleEditorWorkspace::BeginTextEdit(TextField field)
{
	if (m_ActiveTextField != TextField::None && m_ActiveTextField != field)
		EndTextEdit(true);
	m_ActiveTextField = field;
	if (std::string* value = GetTextField(field))
		m_EditOriginalText = *value;
	else
		m_EditOriginalText.clear();
	m_SelectAllText = true;
}

void CParticleEditorWorkspace::EndTextEdit(bool accept)
{
	if (m_ActiveTextField == TextField::None)
		return;
	const TextField field = m_ActiveTextField;
	if (!accept)
	{
		if (std::string* value = GetTextField(field))
			*value = m_EditOriginalText;
	}
	else if (field == TextField::EffectName && !m_EffectNameText.empty())
	{
		if (DmxElement* system = CurrentSystem(); system && system->m_Name != m_EffectNameText)
		{
			system->m_Name = m_EffectNameText;
			MarkDirty();
			RebuildBrowserRows();
		}
	}
	m_ActiveTextField = TextField::None;
	m_SelectAllText = false;
	m_EditOriginalText.clear();
}

std::string* CParticleEditorWorkspace::GetTextField(TextField field)
{
	switch (field)
	{
	case TextField::EffectName: return &m_EffectNameText;
	case TextField::AttributeName: return &m_AttributeNameText;
	case TextField::AttributeValue: return &m_AttributeValueText;
	case TextField::ComponentFunction: return &m_ComponentFunctionText;
	case TextField::ControlPointIndex: return &m_ControlPointIndexText;
	case TextField::ControlPointPositionX: return &m_ControlPointPositionText[0];
	case TextField::ControlPointPositionY: return &m_ControlPointPositionText[1];
	case TextField::ControlPointPositionZ: return &m_ControlPointPositionText[2];
	case TextField::ControlPointPitch: return &m_ControlPointAnglesText[0];
	case TextField::ControlPointYaw: return &m_ControlPointAnglesText[1];
	case TextField::ControlPointRoll: return &m_ControlPointAnglesText[2];
	default: return nullptr;
	}
}

void CParticleEditorWorkspace::PasteClipboardText()
{
	std::string* destination = GetTextField(m_ActiveTextField);
	if (!destination || !OpenClipboard(nullptr))
		return;
	HANDLE handle = GetClipboardData(CF_UNICODETEXT);
	if (handle)
	{
		const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(handle));
		if (text)
		{
			if (m_SelectAllText)
				destination->clear();
			destination->append(ParticleEditorVGui::ToUtf8(text));
			m_SelectAllText = false;
			GlobalUnlock(handle);
		}
	}
	CloseClipboard();
}

bool CParticleEditorWorkspace::ConfirmSaveChanges()
{
	if (!m_Dirty)
		return true;

	const std::wstring assetName = m_Path.empty() ? ParticleEditorVGui::ToWide(m_EffectNameText + ".pcf") :
		m_Path.filename().wstring();
	const std::wstring prompt = L"Save changes to \"" + assetName + L"\"?";
	const HWND owner = g_gameHWND ? *g_gameHWND : nullptr;
	const int result = MessageBoxW(owner, prompt.c_str(), L"Particle Editor",
		MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1);
	RestoreEditorFocus();
	if (result == IDYES)
		return SaveDocument(false);
	return result == IDNO;
}

std::optional<std::filesystem::path> CParticleEditorWorkspace::ShowPcfFileDialog(bool save)
{
	EndTextEdit(true);
	m_PressedHitTarget = -1;
	m_ToolSystem.SetEditorMouseCapture(false);

	std::filesystem::path initialDirectory =
		!m_Path.empty() && !m_Path.parent_path().empty() ? m_Path.parent_path() : GetParticleEditorDirectory();
	std::error_code directoryError;
	std::filesystem::create_directories(initialDirectory, directoryError);
	if (directoryError)
		initialDirectory.clear();

	std::array<wchar_t, 32768> fileName{};
	if (save)
	{
		const std::filesystem::path suggestedPath = m_Path.empty() ? DefaultAssetPath(m_EffectNameText) : m_Path;
		const std::wstring suggestedName = suggestedPath.filename().wstring();
		std::copy_n(suggestedName.c_str(), std::min(suggestedName.size(), fileName.size() - 1), fileName.data());
	}

	OPENFILENAMEW dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = g_gameHWND ? *g_gameHWND : nullptr;
	dialog.lpstrFilter = ParticleEditorVGui::PcfDialogFilter;
	dialog.nFilterIndex = 1;
	dialog.lpstrFile = fileName.data();
	dialog.nMaxFile = static_cast<DWORD>(fileName.size());
	dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
	dialog.lpstrTitle = save ? L"Save Particle Configuration File" : L"Choose Particle Configuration File";
	dialog.lpstrDefExt = L"pcf";
	dialog.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
		(save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

	const BOOL accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
	const DWORD dialogError = accepted ? 0 : CommDlgExtendedError();
	RestoreEditorFocus();
	if (!accepted)
	{
		if (dialogError != 0)
		{
			std::ostringstream message;
			message << "File dialog failed: 0x" << std::hex << std::uppercase << dialogError;
			SetStatus(message.str());
		}
		return std::nullopt;
	}
	return std::filesystem::path(fileName.data());
}

void CParticleEditorWorkspace::RestoreEditorFocus()
{
	const HWND gameWindow = g_gameHWND ? *g_gameHWND : nullptr;
	if (gameWindow)
	{
		SetForegroundWindow(gameWindow);
		SetActiveWindow(gameWindow);
	}
	FocusVGuiPanel();
}

bool CParticleEditorWorkspace::RequestNewDocument()
{
	if (!ConfirmSaveChanges())
		return false;
	CreateNewDocument(m_EffectNameText, true);
	return true;
}

bool CParticleEditorWorkspace::RequestOpenDocument()
{
	if (!ConfirmSaveChanges())
		return false;
	const std::optional<std::filesystem::path> path = ShowPcfFileDialog(false);
	return path && OpenDocument(*path);
}

bool CParticleEditorWorkspace::RequestCloseDocument()
{
	if (!ConfirmSaveChanges())
		return false;
	CloseDocument();
	return true;
}

void CParticleEditorWorkspace::CloseDocument()
{
	StopPreview();
	m_Document = ParticleDocument::CreateEmpty();
	m_Path.clear();
	m_PathText.clear();
	m_SelectedElementId.reset();
	m_SelectedSystemId.reset();
	m_PreviewSystemId.reset();
	m_SelectedCategory.reset();
	m_SelectedAttributeIndex.reset();
	m_SelectedElementName = false;
	m_BrowserRows.clear();
	m_BrowserScroll = 0;
	m_CollapsedBrowserNodes.clear();
	m_PropertyScroll = 0;
	m_ControlPoints.clear();
	m_SelectedControlPoint.reset();
	m_Dirty = false;
	SetStatus("No particle document open");
}

void CParticleEditorWorkspace::CreateNewDocument(std::string effectName, bool logCreation)
{
	if (effectName.empty())
		effectName = "northstar_particle";
	ParticleDocument document = ParticleDocument::CreateEmpty();
	const DmObjectId_t rootId = document.CreateElement("DmElement", "untitled").m_Id;
	DmxElement* root = document.FindElement(rootId);
	if (!root)
	{
		SetStatus("Could not create the native PCF document");
		return;
	}
	root->m_Attributes.push_back({"particleSystemDefinitions", AT_ELEMENT_ARRAY, {}, {}});

	m_Document = std::move(document);
	m_EffectNameText = effectName;
	m_Path.clear();
	m_PathText.clear();
	m_BrowserScroll = 0;
	m_SelectedSystemId.reset();
	m_PreviewSystemId.reset();
	m_PreviewRunning = false;
	m_SelectedElementId = rootId;
	m_SelectedCategory.reset();
	m_SelectedAttributeIndex.reset();
	m_SelectedElementName = true;
	m_PropertyScroll = 0;
	m_ComponentCategoryIndex = 3;
	m_ComponentFunctionText = "emit_continuously";
	LoadControlPoints();
	RebuildBrowserRows();
	SelectPropertyRow(0);
	m_Dirty = false;
	SetStatus("New particle document: untitled");
	if (logCreation)
		spdlog::info("[ParticleTools] Created a new untitled native PCF document");
}

void CParticleEditorWorkspace::CreateParticleSystem()
{
	DmxElement* root = m_Document.Root();
	if (!root)
	{
		SetStatus("Create failed: no particle document is open");
		return;
	}
	const std::string name = m_EffectNameText.empty() ? "New Particle System" : m_EffectNameText;
	DmxAttribute* systems = root->FindAttribute("particleSystemDefinitions");
	if (!systems || systems->m_Type != AT_ELEMENT_ARRAY)
	{
		SetStatus("Create failed: the PCF root has no particle-system list");
		return;
	}
	for (const DmObjectId_t& id : systems->m_ElementIds)
	{
		const DmxElement* existing = m_Document.FindElement(id);
		if (existing && existing->m_Name == name)
		{
			SetStatus("Create failed: a particle system with that name already exists");
			return;
		}
	}

	const DmObjectId_t systemId = m_Document.CreateElement("DmeParticleSystemDefinition", name).m_Id;
	DmxElement* system = m_Document.FindElement(systemId);
	if (!system)
	{
		SetStatus("Create failed: could not allocate the particle-system element");
		return;
	}
	for (std::string_view category : ParticleEditorVGui::ComponentCategories)
		system->m_Attributes.push_back({std::string(category), AT_ELEMENT_ARRAY, {}, {}});
	system->m_Attributes.push_back({"preventNameBasedLookup", AT_BOOL, "0", {}});
	system->m_Attributes.push_back({"scripted", AT_BOOL, "0", {}});
	system->m_Attributes.push_back({"max_particles", AT_INT, "256", {}});
	system->m_Attributes.push_back({"material", AT_STRING, "particle\\smoke\\smoke_charge02_lp.vmt", {}});
	system->m_Attributes.push_back({"bounding_box_min", AT_VECTOR3, "-64 -64 -64", {}});
	system->m_Attributes.push_back({"bounding_box_max", AT_VECTOR3, "64 64 64", {}});
	system->m_Attributes.push_back({"radius", AT_FLOAT, "64", {}});

	root = m_Document.Root();
	systems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
	if (!systems)
	{
		m_Document.RemoveElement(systemId);
		SetStatus("Create failed: the PCF root changed unexpectedly");
		return;
	}
	systems->m_ElementIds.push_back(systemId);
	m_SelectedSystemId = systemId;
	if (!m_PreviewRunning)
		m_PreviewSystemId = systemId;
	m_SelectedElementId = systemId;
	m_SelectedCategory.reset();
	m_EffectNameText = name;
	LoadControlPoints();
	RebuildBrowserRows();
	SelectPropertyRow(0);
	MarkDirty();
	SetStatus("Created particle system: " + name);
}

void CParticleEditorWorkspace::DuplicateParticleSystem()
{
	const DmxElement* source = CurrentSystem();
	DmxElement* root = m_Document.Root();
	const DmxAttribute* systems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
	if (!source || !systems)
	{
		SetStatus("Duplicate failed: select a particle system");
		return;
	}
	const DmxElement sourceCopy = *source;
	const std::vector<DmObjectId_t> ownedIds =
		ParticleEditorVGui::CollectOwnedElementIds(m_Document, sourceCopy);
	std::vector<DmxElement> ownedCopies;
	ownedCopies.reserve(ownedIds.size());
	for (const DmObjectId_t& id : ownedIds)
	{
		if (const DmxElement* element = m_Document.FindElement(id))
			ownedCopies.push_back(*element);
	}

	const std::string baseName = sourceCopy.m_Name + "_copy";
	std::string copyName = baseName;
	for (int suffix = 2;; ++suffix)
	{
		bool exists = false;
		for (const DmObjectId_t& id : systems->m_ElementIds)
		{
			const DmxElement* existing = m_Document.FindElement(id);
			if (existing && existing->m_Name == copyName)
			{
				exists = true;
				break;
			}
		}
		if (!exists)
			break;
		copyName = baseName + std::to_string(suffix);
	}

	const DmObjectId_t copiedSystemId =
		m_Document.CreateElement(sourceCopy.m_Type, copyName).m_Id;
	DmxElement* copiedSystem = m_Document.FindElement(copiedSystemId);
	if (!copiedSystem)
	{
		SetStatus("Duplicate failed: could not allocate the copied system");
		return;
	}
	copiedSystem->m_Attributes = sourceCopy.m_Attributes;

	std::vector<std::pair<DmObjectId_t, DmObjectId_t>> idMap;
	idMap.reserve(ownedCopies.size());
	for (const DmxElement& element : ownedCopies)
	{
		const DmObjectId_t copiedId = m_Document.CreateElement(element.m_Type, element.m_Name).m_Id;
		DmxElement* copiedElement = m_Document.FindElement(copiedId);
		if (copiedElement)
		{
			copiedElement->m_Attributes = element.m_Attributes;
			idMap.emplace_back(element.m_Id, copiedId);
		}
	}

	copiedSystem = m_Document.FindElement(copiedSystemId);
	if (copiedSystem)
		ParticleEditorVGui::RemapElementReferences(*copiedSystem, idMap);
	for (const auto& mapping : idMap)
	{
		if (DmxElement* copiedElement = m_Document.FindElement(mapping.second))
			ParticleEditorVGui::RemapElementReferences(*copiedElement, idMap);
	}

	root = m_Document.Root();
	DmxAttribute* mutableSystems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
	if (!mutableSystems)
	{
		for (const auto& mapping : idMap)
			m_Document.RemoveElement(mapping.second);
		m_Document.RemoveElement(copiedSystemId);
		SetStatus("Duplicate failed: the PCF root changed unexpectedly");
		return;
	}
	mutableSystems->m_ElementIds.push_back(copiedSystemId);
	m_SelectedSystemId = copiedSystemId;
	if (!m_PreviewRunning)
		m_PreviewSystemId = copiedSystemId;
	m_SelectedElementId = copiedSystemId;
	m_SelectedCategory.reset();
	m_EffectNameText = copyName;
	LoadControlPoints();
	RebuildBrowserRows();
	SelectPropertyRow(0);
	MarkDirty();
	SetStatus("Duplicated particle system: " + copyName);
}

void CParticleEditorWorkspace::DeleteParticleSystem()
{
	const DmxElement* system = CurrentSystem();
	DmxElement* root = m_Document.Root();
	const DmxAttribute* systems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
	if (!system || !systems)
	{
		SetStatus("Delete failed: select a particle system");
		return;
	}
	const DmObjectId_t systemId = system->m_Id;
	const std::string systemName = system->m_Name;
	const std::vector<DmObjectId_t> ownedIds =
		ParticleEditorVGui::CollectOwnedElementIds(m_Document, *system);

	StopPreview();
	root = m_Document.Root();
	DmxAttribute* mutableSystems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
	if (!mutableSystems)
	{
		SetStatus("Delete failed: the PCF root changed unexpectedly");
		return;
	}
	std::erase(mutableSystems->m_ElementIds, systemId);
	for (const DmObjectId_t& id : ownedIds)
		m_Document.RemoveElement(id);
	m_Document.RemoveElement(systemId);

	m_SelectedSystemId = FirstSystemId();
	m_PreviewSystemId = m_SelectedSystemId;
	m_SelectedElementId = m_SelectedSystemId ? m_SelectedSystemId :
		(m_Document.Root() ? std::optional<DmObjectId_t>(m_Document.Root()->m_Id) : std::nullopt);
	m_SelectedCategory.reset();
	if (const DmxElement* selectedSystem = CurrentSystem())
		m_EffectNameText = selectedSystem->m_Name;
	LoadControlPoints();
	RebuildBrowserRows();
	SelectPropertyRow(0);
	MarkDirty();
	SetStatus("Deleted particle system: " + systemName);
}

bool CParticleEditorWorkspace::OpenDocument(const std::filesystem::path& path)
{
	if (path.empty())
	{
		SetStatus("Open failed: choose a PCF file");
		return false;
	}
	ParticleDocument loaded;
	std::string error;
	if (!loaded.Load(path, error))
	{
		SetStatus("Open failed: " + error);
		spdlog::error("[ParticleTools] Could not open native PCF '{}': {}", ParticleEditorVGui::PathToUtf8(path), error);
		return false;
	}
	m_Document = std::move(loaded);
	m_Path = path;
	m_PathText = ParticleEditorVGui::PathToUtf8(path);
	m_BrowserScroll = 0;
	m_SelectedSystemId = PreferredPreviewSystemId();
	m_PreviewSystemId = m_SelectedSystemId;
	m_PreviewRunning = false;
	m_SelectedElementId = m_SelectedSystemId ? m_SelectedSystemId :
		(m_Document.Root() ? std::optional<DmObjectId_t>(m_Document.Root()->m_Id) : std::nullopt);
	m_SelectedCategory.reset();
	m_SelectedAttributeIndex.reset();
	m_SelectedElementName = true;
	m_PropertyScroll = 0;
	if (const DmxElement* system = CurrentSystem())
		m_EffectNameText = system->m_Name;
	LoadControlPoints();
	RebuildBrowserRows();
	SelectPropertyRow(0);
	m_Dirty = false;
	SetStatus("Opened native R2 PCF: " + ParticleEditorVGui::PathToUtf8(path));
	spdlog::info("[ParticleTools] Opened native PCF '{}' with {} elements; selected effect '{}'",
		ParticleEditorVGui::PathToUtf8(path), m_Document.Elements().size(), m_EffectNameText);
	return true;
}

bool CParticleEditorWorkspace::SaveDocument(bool saveAs)
{
	EndTextEdit(true);
	if (!m_Document.Root())
	{
		SetStatus("Save failed: no particle document is open");
		return false;
	}
	std::filesystem::path path = m_Path;
	if (saveAs || path.empty())
	{
		const std::optional<std::filesystem::path> selectedPath = ShowPcfFileDialog(true);
		if (!selectedPath)
			return false;
		path = *selectedPath;
	}
	if (!path.has_extension())
		path.replace_extension(".pcf");
	std::error_code directoryError;
	if (!path.parent_path().empty())
		std::filesystem::create_directories(path.parent_path(), directoryError);
	if (directoryError)
	{
		SetStatus("Save failed: " + directoryError.message());
		return false;
	}
	std::string error;
	if (!m_Document.Save(path, error))
	{
		SetStatus("Save failed: " + error);
		spdlog::error("[ParticleTools] Could not save native PCF '{}': {}", ParticleEditorVGui::PathToUtf8(path), error);
		return false;
	}
	m_Path = path;
	m_PathText = ParticleEditorVGui::PathToUtf8(path);
	m_Dirty = false;
	std::error_code sizeError;
	const std::uintmax_t size = std::filesystem::file_size(path, sizeError);
	SetStatus("Saved native R2 PCF: " + m_PathText);
	spdlog::info("[ParticleTools] Saved native PCF '{}' ({} bytes, {} elements)",
		m_PathText, sizeError ? 0 : size, m_Document.Elements().size());
	return true;
}

void CParticleEditorWorkspace::PreviewDocument()
{
	EndTextEdit(true);
	const DmxElement* previewSystem = SelectedPreviewSystem();
	if (!previewSystem || previewSystem->m_Type != "DmeParticleSystemDefinition")
	{
		SetStatus("Preview failed: select a particle-system definition");
		return;
	}
	m_PreviewSystemId = previewSystem->m_Id;
	RequestActivePreview(*previewSystem);
}

void CParticleEditorWorkspace::RequestActivePreview(const DmxElement& previewSystem)
{
	m_ToolSystem.RequestPreview(
		m_Document, m_Path, previewSystem.m_Name, ReadControlPoints(previewSystem));
	m_PreviewRunning = true;
}

void CParticleEditorWorkspace::RefreshActivePreview()
{
	if (!m_PreviewRunning)
		return;
	const DmxElement* previewSystem =
		m_PreviewSystemId ? m_Document.FindElement(*m_PreviewSystemId) : nullptr;
	if (!previewSystem || previewSystem->m_Type != "DmeParticleSystemDefinition")
	{
		StopPreview();
		SetStatus("Live preview stopped: the active particle-system definition no longer exists");
		return;
	}
	RequestActivePreview(*previewSystem);
}

void CParticleEditorWorkspace::StopPreview()
{
	m_PreviewRunning = false;
	m_ToolSystem.RequestStopPreview();
}

void CParticleEditorWorkspace::MarkDirty()
{
	m_Dirty = true;
	RefreshActivePreview();
}

void CParticleEditorWorkspace::RebuildBrowserRows()
{
	m_BrowserRows.clear();
	const DmxElement* root = m_Document.Root();
	if (!root)
		return;
	const BrowserRow rootRow{root->m_Name + " [collection]", root->m_Id, std::nullopt, 0, false, true};
	m_BrowserRows.push_back(rootRow);
	if (BrowserRowCollapsed(rootRow))
		return;
	const DmxAttribute* systems = root->FindAttribute("particleSystemDefinitions");
	if (!systems || systems->m_Type != AT_ELEMENT_ARRAY)
		return;

	const std::optional<DmObjectId_t> rootSystemId = PreferredPreviewSystemId();
	const auto appendSystem = [&](const DmObjectId_t& systemId, bool isRoot)
	{
		const DmxElement* system = m_Document.FindElement(systemId);
		if (!system)
			return;
		std::string label = system->m_Name;
		if (isRoot)
			label += " [root]";
		const BrowserRow systemRow{std::move(label), systemId, std::nullopt, 1, false, true};
		m_BrowserRows.push_back(systemRow);
		if (BrowserRowCollapsed(systemRow))
			return;
		for (std::string_view categoryName : ParticleEditorVGui::ComponentCategories)
		{
			const DmxAttribute* category = system->FindAttribute(categoryName);
			if (!category || category->m_Type != AT_ELEMENT_ARRAY)
				continue;
			const bool collapsible = !category->m_ElementIds.empty();
			const BrowserRow categoryRow{std::string(categoryName), systemId, std::string(categoryName), 2, true, collapsible};
			m_BrowserRows.push_back(categoryRow);
			if (!collapsible || BrowserRowCollapsed(categoryRow))
				continue;
			for (const DmObjectId_t& componentId : category->m_ElementIds)
			{
				const DmxElement* component = m_Document.FindElement(componentId);
				m_BrowserRows.push_back({component ? component->m_Name : FormatObjectId(componentId), componentId,
					std::string(categoryName), 3, false, false});
			}
		}
	};

	if (rootSystemId)
		appendSystem(*rootSystemId, true);
	for (const DmObjectId_t& systemId : systems->m_ElementIds)
	{
		if (rootSystemId && systemId == *rootSystemId)
			continue;
		appendSystem(systemId, false);
	}
	m_BrowserScroll = std::min(m_BrowserScroll, m_BrowserRows.empty() ? 0 : m_BrowserRows.size() - 1);
}

void CParticleEditorWorkspace::SelectBrowserRow(std::size_t index)
{
	if (index >= m_BrowserRows.size() || !m_BrowserRows[index].m_ElementId)
		return;
	const BrowserRow& row = m_BrowserRows[index];
	m_SelectedElementId = row.m_ElementId;
	m_SelectedCategory = row.m_Category;
	const DmxElement* selected = SelectedElement();
	if (selected && selected->m_Type == "DmeParticleSystemDefinition")
	{
		m_SelectedSystemId = selected->m_Id;
		LoadControlPoints();
	}
	else if (selected)
	{
		const DmxElement* root = m_Document.Root();
		const DmxAttribute* systems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
		if (systems)
		{
			for (const DmObjectId_t& systemId : systems->m_ElementIds)
			{
				const DmxElement* system = m_Document.FindElement(systemId);
				if (!system)
					continue;
				bool found = false;
				for (std::string_view categoryName : ParticleEditorVGui::ComponentCategories)
				{
					const DmxAttribute* category = system->FindAttribute(categoryName);
					if (category && std::find(category->m_ElementIds.begin(), category->m_ElementIds.end(), selected->m_Id) !=
						category->m_ElementIds.end())
					{
						m_SelectedSystemId = systemId;
						found = true;
						break;
					}
				}
				if (found)
					break;
			}
		}
	}
	if (!m_PreviewRunning)
		m_PreviewSystemId = m_SelectedSystemId;
	if (const DmxElement* currentSystem = CurrentSystem())
		m_EffectNameText = currentSystem->m_Name;
	m_PropertyScroll = 0;
	SelectPropertyRow(0);
}

std::string CParticleEditorWorkspace::BrowserRowKey(const BrowserRow& row) const
{
	if (row.m_Category)
		return "C:" + (row.m_ElementId ? FormatObjectId(*row.m_ElementId) : std::string()) + ":" + *row.m_Category;
	return "S:" + (row.m_ElementId ? FormatObjectId(*row.m_ElementId) : std::string());
}

bool CParticleEditorWorkspace::BrowserRowCollapsed(const BrowserRow& row) const
{
	return m_CollapsedBrowserNodes.count(BrowserRowKey(row)) != 0;
}

void CParticleEditorWorkspace::ToggleBrowserRow(std::size_t index)
{
	if (index >= m_BrowserRows.size() || !m_BrowserRows[index].m_Collapsible)
		return;
	const std::string key = BrowserRowKey(m_BrowserRows[index]);
	if (m_CollapsedBrowserNodes.erase(key) == 0)
		m_CollapsedBrowserNodes.insert(key);
	RebuildBrowserRows();
	Repaint();
}

DmxElement* CParticleEditorWorkspace::SelectedElement()
{
	return m_SelectedElementId ? m_Document.FindElement(*m_SelectedElementId) : nullptr;
}

const DmxElement* CParticleEditorWorkspace::SelectedElement() const
{
	return m_SelectedElementId ? m_Document.FindElement(*m_SelectedElementId) : nullptr;
}

DmxElement* CParticleEditorWorkspace::CurrentSystem()
{
	return m_SelectedSystemId ? m_Document.FindElement(*m_SelectedSystemId) : nullptr;
}

const DmxElement* CParticleEditorWorkspace::CurrentSystem() const
{
	return m_SelectedSystemId ? m_Document.FindElement(*m_SelectedSystemId) : nullptr;
}

const DmxElement* CParticleEditorWorkspace::SelectedPreviewSystem() const
{
	const DmxElement* selected = SelectedElement();
	if (selected && selected->m_Type == "DmeParticleSystemDefinition")
		return selected;
	if (selected && selected->m_Type == "DmeParticleChild")
	{
		const DmxAttribute* child = selected->FindAttribute("child");
		const DmxElement* childSystem =
			child && child->m_Type == AT_ELEMENT && child->m_ElementIds.size() == 1
				? m_Document.FindElement(child->m_ElementIds.front())
				: nullptr;
		if (childSystem && childSystem->m_Type == "DmeParticleSystemDefinition")
			return childSystem;
	}
	return CurrentSystem();
}

std::optional<DmObjectId_t> CParticleEditorWorkspace::FirstSystemId() const
{
	const DmxElement* root = m_Document.Root();
	const DmxAttribute* systems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
	if (!systems || systems->m_Type != AT_ELEMENT_ARRAY || systems->m_ElementIds.empty())
		return std::nullopt;
	return systems->m_ElementIds.front();
}

std::optional<DmObjectId_t> CParticleEditorWorkspace::PreferredPreviewSystemId() const
{
	const DmxElement* root = m_Document.Root();
	const DmxAttribute* systems = root ? root->FindAttribute("particleSystemDefinitions") : nullptr;
	if (!systems || systems->m_Type != AT_ELEMENT_ARRAY)
		return std::nullopt;

	std::optional<DmObjectId_t> preferredSystemId;
	std::size_t preferredChildCount = 0;
	for (const DmObjectId_t& systemId : systems->m_ElementIds)
	{
		const DmxElement* system = m_Document.FindElement(systemId);
		if (!system || system->m_Type != "DmeParticleSystemDefinition")
			continue;

		const DmxAttribute* children = system->FindAttribute("children");
		const std::size_t childCount = children && children->m_Type == AT_ELEMENT_ARRAY ? children->m_ElementIds.size() : 0;
		if (!preferredSystemId || childCount > preferredChildCount)
		{
			preferredSystemId = systemId;
			preferredChildCount = childCount;
		}
	}
	return preferredSystemId;
}


std::filesystem::path CParticleEditorWorkspace::DefaultAssetPath(std::string_view effectName) const
{
	return GetParticleEditorDirectory() /
		(ParticleEditorVGui::SanitizeAssetName(effectName) + ".pcf");
}


void CParticleEditorWorkspace::SelectPropertyRow(std::size_t row)
{
	DmxElement* element = SelectedElement();
	if (!element)
		return;
	if (row == 0)
	{
		m_SelectedElementName = true;
		m_SelectedAttributeIndex.reset();
		m_AttributeNameText = "name";
		m_AttributeType = AT_STRING;
		m_AttributeValueText = element->m_Name;
		return;
	}
	const std::size_t index = row - 1;
	if (index >= element->m_Attributes.size())
		return;
	const DmxAttribute& attribute = element->m_Attributes[index];
	m_SelectedElementName = false;
	m_SelectedAttributeIndex = index;
	m_AttributeNameText = attribute.m_Name;
	m_AttributeType = attribute.m_Type;
	m_AttributeValueText = attribute.IsElementReference() ? ParticleEditorVGui::ReferenceText(attribute) : attribute.m_Value;
}

bool CParticleEditorWorkspace::ReadAttributeEditor(DmxAttribute& attribute, std::string& error) const
{
	attribute = {};
	attribute.m_Name = m_AttributeNameText;
	if (attribute.m_Name.empty())
	{
		error = "Attribute name cannot be empty.";
		return false;
	}
	attribute.m_Type = m_AttributeType;
	if (attribute.IsElementReference())
	{
		if (!ParticleEditorVGui::ParseElementReferences(m_AttributeValueText,
			attribute.m_Type == AT_ELEMENT_ARRAY, attribute.m_ElementIds))
		{
			error = "Expected a GUID or a comma-separated GUID array.";
			return false;
		}
	}
	else
	{
		attribute.m_Value = m_AttributeValueText;
	}
	return ParticleDocument::CanonicalizeAttribute(attribute, error);
}

void CParticleEditorWorkspace::AddAttribute()
{
	DmxElement* element = SelectedElement();
	if (!element)
		return;
	DmxAttribute attribute;
	std::string error;
	if (!ReadAttributeEditor(attribute, error))
	{
		SetStatus("Add attribute failed: " + error);
		return;
	}
	if (element->FindAttribute(attribute.m_Name))
	{
		SetStatus("Add attribute failed: that name already exists");
		return;
	}
	element->m_Attributes.push_back(std::move(attribute));
	m_SelectedElementName = false;
	m_SelectedAttributeIndex = element->m_Attributes.size() - 1;
	MarkDirty();
	SetStatus("Added attribute: " + element->m_Attributes.back().m_Name);
}

void CParticleEditorWorkspace::ApplyAttribute()
{
	DmxElement* element = SelectedElement();
	if (!element)
		return;
	if (m_SelectedElementName)
	{
		if (m_AttributeValueText.empty())
		{
			SetStatus("Element name cannot be empty");
			return;
		}
		element->m_Name = m_AttributeValueText;
		if (element->m_Type == "DmeParticleSystemDefinition")
			m_EffectNameText = element->m_Name;
		MarkDirty();
		RebuildBrowserRows();
		SetStatus("Renamed element: " + element->m_Name);
		return;
	}
	if (!m_SelectedAttributeIndex || *m_SelectedAttributeIndex >= element->m_Attributes.size())
		return;
	DmxAttribute attribute;
	std::string error;
	if (!ReadAttributeEditor(attribute, error))
	{
		SetStatus("Apply attribute failed: " + error);
		return;
	}
	for (std::size_t index = 0; index < element->m_Attributes.size(); ++index)
	{
		if (index != *m_SelectedAttributeIndex && element->m_Attributes[index].m_Name == attribute.m_Name)
		{
			SetStatus("Apply attribute failed: that name already exists");
			return;
		}
	}
	element->m_Attributes[*m_SelectedAttributeIndex] = std::move(attribute);
	m_AttributeNameText = element->m_Attributes[*m_SelectedAttributeIndex].m_Name;
	m_AttributeValueText = element->m_Attributes[*m_SelectedAttributeIndex].IsElementReference()
		? ParticleEditorVGui::ReferenceText(element->m_Attributes[*m_SelectedAttributeIndex])
		: element->m_Attributes[*m_SelectedAttributeIndex].m_Value;
	MarkDirty();
	SetStatus("Updated attribute: " + m_AttributeNameText);
}

void CParticleEditorWorkspace::RemoveAttribute()
{
	DmxElement* element = SelectedElement();
	if (!element || !m_SelectedAttributeIndex || *m_SelectedAttributeIndex >= element->m_Attributes.size())
		return;
	const std::string name = element->m_Attributes[*m_SelectedAttributeIndex].m_Name;
	element->m_Attributes.erase(element->m_Attributes.begin() + static_cast<std::ptrdiff_t>(*m_SelectedAttributeIndex));
	m_SelectedAttributeIndex.reset();
	m_SelectedElementName = true;
	SelectPropertyRow(0);
	MarkDirty();
	SetStatus("Removed attribute: " + name);
}
DmxAttribute* CParticleEditorWorkspace::FindCategoryAttribute(DmxElement& system, std::string_view category, bool create)
{
	DmxAttribute* attribute = system.FindAttribute(category);
	if (attribute)
		return attribute->m_Type == AT_ELEMENT_ARRAY ? attribute : nullptr;
	if (!create)
		return nullptr;
	system.m_Attributes.push_back({std::string(category), AT_ELEMENT_ARRAY, {}, {}});
	return &system.m_Attributes.back();
}

void CParticleEditorWorkspace::AddComponent(std::optional<std::string_view> preset)
{
	DmxElement* system = CurrentSystem();
	if (!system)
	{
		SetStatus("Select a particle system before adding a component");
		return;
	}
	const std::string category = std::string(ParticleEditorVGui::ComponentCategories[m_ComponentCategoryIndex]);
	const std::string functionName = preset ? std::string(*preset) : m_ComponentFunctionText;
	if (functionName.empty())
	{
		SetStatus("Enter a particle function name");
		return;
	}
	if (!FindCategoryAttribute(*system, category, true))
	{
		SetStatus("The selected category is not an element array");
		return;
	}
	const DmObjectId_t systemId = system->m_Id;
	const DmObjectId_t componentId = m_Document.CreateElement("DmeParticleOperator", functionName).m_Id;
	DmxElement* component = m_Document.FindElement(componentId);
	system = m_Document.FindElement(systemId);
	DmxAttribute* categoryAttribute = system ? FindCategoryAttribute(*system, category, true) : nullptr;
	if (!component || !categoryAttribute)
	{
		m_Document.RemoveElement(componentId);
		SetStatus("Could not create the particle component");
		return;
	}
	component->m_Attributes.push_back({"functionName", AT_STRING, functionName, {}});
	if (functionName == "emit_continuously")
		component->m_Attributes.push_back({"emission_rate", AT_FLOAT, "32", {}});
	else if (functionName == "emit_instantaneously")
		component->m_Attributes.push_back({"num_to_emit", AT_INT, "1", {}});
	else if (functionName == "render_animated_sprites")
	{
		component->m_Attributes.push_back({"use animation rate as FPS", AT_BOOL, "1", {}});
		component->m_Attributes.push_back({"animation rate", AT_FLOAT, "15", {}});
	}
	else if (functionName == "Lifetime Random")
	{
		component->m_Attributes.push_back({"lifetime_min", AT_FLOAT, "1", {}});
		component->m_Attributes.push_back({"lifetime_max", AT_FLOAT, "1", {}});
	}
	else if (functionName == "Radius Random")
	{
		component->m_Attributes.push_back({"radius_min", AT_FLOAT, "64", {}});
		component->m_Attributes.push_back({"radius_max", AT_FLOAT, "64", {}});
	}
	categoryAttribute->m_ElementIds.push_back(componentId);
	m_SelectedElementId = componentId;
	m_SelectedCategory = category;
	m_SelectedSystemId = systemId;
	m_SelectedElementName = true;
	m_SelectedAttributeIndex.reset();
	MarkDirty();
	RebuildBrowserRows();
	SelectPropertyRow(0);
	SetStatus("Added " + category + " component: " + functionName);
}

void CParticleEditorWorkspace::RemoveComponent()
{
	if (!m_SelectedElementId || !m_SelectedSystemId || *m_SelectedElementId == *m_SelectedSystemId || !m_SelectedCategory)
		return;
	DmxElement* system = CurrentSystem();
	DmxAttribute* category = system ? FindCategoryAttribute(*system, *m_SelectedCategory, false) : nullptr;
	if (!category)
		return;
	const DmObjectId_t componentId = *m_SelectedElementId;
	std::erase(category->m_ElementIds, componentId);
	m_Document.RemoveElement(componentId);
	m_SelectedElementId = m_SelectedSystemId;
	m_SelectedCategory.reset();
	MarkDirty();
	RebuildBrowserRows();
	SelectPropertyRow(0);
	SetStatus("Removed particle component");
}

void CParticleEditorWorkspace::MoveComponent(int direction)
{
	if (!m_SelectedElementId || !m_SelectedCategory || direction == 0)
		return;
	DmxElement* system = CurrentSystem();
	DmxAttribute* category = system ? FindCategoryAttribute(*system, *m_SelectedCategory, false) : nullptr;
	if (!category)
		return;
	const auto iterator = std::find(category->m_ElementIds.begin(), category->m_ElementIds.end(), *m_SelectedElementId);
	if (iterator == category->m_ElementIds.end())
		return;
	const std::ptrdiff_t index = iterator - category->m_ElementIds.begin();
	const std::ptrdiff_t destination = index + direction;
	if (destination < 0 || destination >= static_cast<std::ptrdiff_t>(category->m_ElementIds.size()))
		return;
	std::swap(category->m_ElementIds[static_cast<std::size_t>(index)],
		category->m_ElementIds[static_cast<std::size_t>(destination)]);
	MarkDirty();
	RebuildBrowserRows();
	SetStatus("Reordered particle component");
}

void CParticleEditorWorkspace::LoadControlPoints()
{
	m_ControlPoints.clear();
	const DmxElement* system = CurrentSystem();
	if (system)
		m_ControlPoints = ReadControlPoints(*system);
	m_SelectedControlPoint.reset();
}

std::vector<ParticlePreviewControlPoint> CParticleEditorWorkspace::ReadControlPoints(const DmxElement& system) const
{
	const DmxAttribute* indicesAttribute = system.FindAttribute("northstar_preview_control_point_indices");
	const DmxAttribute* positionsAttribute = system.FindAttribute("northstar_preview_control_point_positions");
	const DmxAttribute* anglesAttribute = system.FindAttribute("northstar_preview_control_point_angles");
	if (!indicesAttribute || !positionsAttribute || !anglesAttribute ||
		indicesAttribute->m_Type != AT_INT_ARRAY ||
		positionsAttribute->m_Type != AT_VECTOR3_ARRAY ||
		anglesAttribute->m_Type != AT_QANGLE_ARRAY)
		return {};
	const std::vector<int> indices = ParticleEditorVGui::ParseIntegerList(indicesAttribute->m_Value);
	const std::vector<float> positions = ParticleEditorVGui::ParseNumberList(positionsAttribute->m_Value);
	const std::vector<float> angles = ParticleEditorVGui::ParseNumberList(anglesAttribute->m_Value);
	if (positions.size() != indices.size() * 3 || angles.size() != indices.size() * 3)
		return {};
	std::vector<ParticlePreviewControlPoint> result(indices.size());
	for (std::size_t index = 0; index < indices.size(); ++index)
	{
		result[index].m_Index = indices[index];
		std::copy_n(positions.data() + index * 3, 3, result[index].m_Position.Base());
		std::copy_n(angles.data() + index * 3, 3, result[index].m_Angles.Base());
	}
	return result;
}

bool CParticleEditorWorkspace::ReadControlPointEditor(ParticlePreviewControlPoint& controlPoint, std::string& error) const
{
	if (!ParticleEditorVGui::ParseInt(m_ControlPointIndexText, controlPoint.m_Index) ||
		controlPoint.m_Index < 0 || controlPoint.m_Index > 63)
	{
		error = "Control-point index must be an integer from 0 to 63.";
		return false;
	}
	for (std::size_t axis = 0; axis < 3; ++axis)
	{
		if (!ParticleEditorVGui::ParseFloat(m_ControlPointPositionText[axis], controlPoint.m_Position[axis]) ||
			!ParticleEditorVGui::ParseFloat(m_ControlPointAnglesText[axis], controlPoint.m_Angles[axis]))
		{
			error = "Control-point positions and angles must be finite numeric values.";
			return false;
		}
	}
	return true;
}

bool CParticleEditorWorkspace::PersistControlPoints(
	const std::vector<ParticlePreviewControlPoint>& controlPoints, std::string& error)
{
	DmxElement* system = CurrentSystem();
	if (!system)
	{
		error = "No particle-system definition is selected.";
		return false;
	}
	auto upsert = [system](std::string name, DmAttributeType_t type, std::string value)
	{
		DmxAttribute* attribute = system->FindAttribute(name);
		if (!attribute)
		{
			system->m_Attributes.push_back({std::move(name), type, std::move(value), {}});
			return &system->m_Attributes.back();
		}
		attribute->m_Type = type;
		attribute->m_Value = std::move(value);
		attribute->m_ElementIds.clear();
		return attribute;
	};
	upsert("northstar_preview_control_point_indices", AT_INT_ARRAY,
		ParticleEditorVGui::FormatControlPointIndices(controlPoints));
	upsert("northstar_preview_control_point_positions", AT_VECTOR3_ARRAY,
		ParticleEditorVGui::FormatControlPointTuples(controlPoints, false));
	upsert("northstar_preview_control_point_angles", AT_QANGLE_ARRAY,
		ParticleEditorVGui::FormatControlPointTuples(controlPoints, true));
	return ParticleDocument::CanonicalizeAttribute(
			*system->FindAttribute("northstar_preview_control_point_indices"), error) &&
		ParticleDocument::CanonicalizeAttribute(
			*system->FindAttribute("northstar_preview_control_point_positions"), error) &&
		ParticleDocument::CanonicalizeAttribute(
			*system->FindAttribute("northstar_preview_control_point_angles"), error);
}

void CParticleEditorWorkspace::SelectControlPoint(std::size_t index)
{
	if (index >= m_ControlPoints.size())
		return;
	m_SelectedControlPoint = index;
	const ParticlePreviewControlPoint& point = m_ControlPoints[index];
	m_ControlPointIndexText = std::to_string(point.m_Index);
	for (std::size_t axis = 0; axis < 3; ++axis)
	{
		m_ControlPointPositionText[axis] = std::to_string(point.m_Position[axis]);
		m_ControlPointAnglesText[axis] = std::to_string(point.m_Angles[axis]);
	}
}

void CParticleEditorWorkspace::AddControlPoint()
{
	ParticlePreviewControlPoint point;
	std::string error;
	if (!ReadControlPointEditor(point, error))
	{
		SetStatus("Add control point failed: " + error);
		return;
	}
	if (std::any_of(m_ControlPoints.begin(), m_ControlPoints.end(), [&point](const ParticlePreviewControlPoint& existing)
		{ return existing.m_Index == point.m_Index; }))
	{
		SetStatus("A control point with that index already exists");
		return;
	}
	m_ControlPoints.push_back(point);
	std::sort(m_ControlPoints.begin(), m_ControlPoints.end(), [](const ParticlePreviewControlPoint& left,
		const ParticlePreviewControlPoint& right) { return left.m_Index < right.m_Index; });
	if (!PersistControlPoints(m_ControlPoints, error))
	{
		SetStatus("Add control point failed: " + error);
		return;
	}
	const auto iterator = std::find_if(m_ControlPoints.begin(), m_ControlPoints.end(), [&point](const ParticlePreviewControlPoint& existing)
		{ return existing.m_Index == point.m_Index; });
	m_SelectedControlPoint = static_cast<std::size_t>(iterator - m_ControlPoints.begin());
	MarkDirty();
	SetStatus("Added preview control point " + std::to_string(point.m_Index));
}

void CParticleEditorWorkspace::ApplyControlPoint()
{
	if (!m_SelectedControlPoint || *m_SelectedControlPoint >= m_ControlPoints.size())
		return;
	ParticlePreviewControlPoint point;
	std::string error;
	if (!ReadControlPointEditor(point, error))
	{
		SetStatus("Apply control point failed: " + error);
		return;
	}
	for (std::size_t index = 0; index < m_ControlPoints.size(); ++index)
	{
		if (index != *m_SelectedControlPoint && m_ControlPoints[index].m_Index == point.m_Index)
		{
			SetStatus("A control point with that index already exists");
			return;
		}
	}
	m_ControlPoints[*m_SelectedControlPoint] = point;
	std::sort(m_ControlPoints.begin(), m_ControlPoints.end(), [](const ParticlePreviewControlPoint& left,
		const ParticlePreviewControlPoint& right) { return left.m_Index < right.m_Index; });
	if (!PersistControlPoints(m_ControlPoints, error))
	{
		SetStatus("Apply control point failed: " + error);
		return;
	}
	const auto iterator = std::find_if(m_ControlPoints.begin(), m_ControlPoints.end(), [&point](const ParticlePreviewControlPoint& existing)
		{ return existing.m_Index == point.m_Index; });
	m_SelectedControlPoint = static_cast<std::size_t>(iterator - m_ControlPoints.begin());
	MarkDirty();
	SetStatus("Updated preview control point " + std::to_string(point.m_Index));
}

void CParticleEditorWorkspace::RemoveControlPoint()
{
	if (!m_SelectedControlPoint || *m_SelectedControlPoint >= m_ControlPoints.size())
		return;
	const int index = m_ControlPoints[*m_SelectedControlPoint].m_Index;
	m_ControlPoints.erase(m_ControlPoints.begin() + static_cast<std::ptrdiff_t>(*m_SelectedControlPoint));
	std::string error;
	if (!PersistControlPoints(m_ControlPoints, error))
	{
		SetStatus("Remove control point failed: " + error);
		return;
	}
	m_SelectedControlPoint.reset();
	MarkDirty();
	SetStatus("Removed preview control point " + std::to_string(index));
}

} // namespace ParticleTools
