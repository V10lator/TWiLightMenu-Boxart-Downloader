#include "common.hpp"

void Msg::DisplayMsg(std::string text) {
	Gui::clearTextBufs();
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(Top, TRANSPARENT);
	C2D_TargetClear(TopRight, TRANSPARENT);
	C2D_TargetClear(Bottom, TRANSPARENT);
	Gui::ScreenDraw(Bottom);
	GFX::DrawSprite(sprites_BS_loading_background_idx, 0, 0);
	Gui::DrawString(24, 32, 0.45f, BLACK, text);
	C3D_FrameEnd(0);
}
