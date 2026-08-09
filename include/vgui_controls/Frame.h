#pragma once

#include "vgui_controls/EditablePanel.h"

#include <cstddef>
#include <type_traits>

namespace vgui
{
class Frame : public EditablePanel
{
public:
	virtual void FrameSlot255() = 0;
	virtual void FrameSlot256() = 0;
	virtual void FrameSlot257() = 0;
	virtual void FrameSlot258() = 0;
	virtual void FrameSlot259() = 0;
	virtual void FrameSlot260() = 0;
	virtual void FrameSlot261() = 0;
	virtual void FrameSlot262() = 0;
	virtual void FrameSlot263() = 0;
	virtual void FrameSlot264() = 0;
	virtual void FrameSlot265() = 0;
	virtual void FrameSlot266() = 0;
	virtual void FrameSlot267() = 0;
	virtual void FrameSlot268() = 0;
	virtual void FrameSlot269() = 0;
	virtual void FrameSlot270() = 0;
	virtual void FrameSlot271() = 0;
	virtual void FrameSlot272() = 0;
	virtual void FrameSlot273() = 0;
	virtual void FrameSlot274() = 0;
	virtual void FrameSlot275() = 0;
	virtual void FrameSlot276() = 0;
	virtual void FrameSlot277() = 0;
	virtual void FrameSlot278() = 0;
	virtual void FrameSlot279() = 0;
	virtual void FrameSlot280() = 0;
	virtual void FrameSlot281() = 0;
	virtual void FrameSlot282() = 0;
	virtual void FrameSlot283() = 0;
	virtual void FrameSlot284() = 0;
	virtual void FrameSlot285() = 0;
	virtual void FrameSlot286() = 0;
	virtual void FrameSlot287() = 0;
	virtual void FrameSlot288() = 0;
	virtual void FrameSlot289() = 0;
	virtual void FrameSlot290() = 0;
	virtual void FrameSlot291() = 0;
	virtual void FrameSlot292() = 0;
	virtual void FrameSlot293() = 0;
	virtual void FrameSlot294() = 0;
	virtual void FrameSlot295() = 0;
	virtual void FrameSlot296() = 0;
	virtual void FrameSlot297() = 0;

private:
	std::byte m_FrameData[0xE8];
};

static_assert(std::is_base_of_v<EditablePanel, Frame>);
static_assert(sizeof(Frame) == 0x3A0);
} // namespace vgui
