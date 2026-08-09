#pragma once

#include "vgui_controls/Panel.h"

#include <cstddef>
#include <type_traits>

namespace vgui
{
class EditablePanel : public Panel
{
public:
	virtual void EditablePanelSlot230() = 0;
	virtual void EditablePanelSlot231() = 0;
	virtual void EditablePanelSlot232() = 0;
	virtual void EditablePanelSlot233() = 0;
	virtual void EditablePanelSlot234() = 0;
	virtual void EditablePanelSlot235() = 0;
	virtual void EditablePanelSlot236() = 0;
	virtual void EditablePanelSlot237() = 0;
	virtual void EditablePanelSlot238() = 0;
	virtual void EditablePanelSlot239() = 0;
	virtual void EditablePanelSlot240() = 0;
	virtual void EditablePanelSlot241() = 0;
	virtual void EditablePanelSlot242() = 0;
	virtual void EditablePanelSlot243() = 0;
	virtual void EditablePanelSlot244() = 0;
	virtual void EditablePanelSlot245() = 0;
	virtual void EditablePanelSlot246() = 0;
	virtual void EditablePanelSlot247() = 0;
	virtual void EditablePanelSlot248() = 0;
	virtual void EditablePanelSlot249() = 0;
	virtual void EditablePanelSlot250() = 0;
	virtual void EditablePanelSlot251() = 0;
	virtual void EditablePanelSlot252() = 0;
	virtual void EditablePanelSlot253() = 0;
	virtual void EditablePanelSlot254() = 0;

private:
	std::byte m_EditablePanelData[0x50];
};

static_assert(std::is_base_of_v<Panel, EditablePanel>);
static_assert(sizeof(EditablePanel) == 0x2B8);
} // namespace vgui
