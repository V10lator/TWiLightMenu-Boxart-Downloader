#include "common.hpp"

// 3D offsets.
typedef struct _Offset3D {
	float topbg;
	float twinkle3;
	float twinkle2;
	float twinkle1;
	float updater;
	float logo;
} Offset3D;
Offset3D offset3D[2] = {0.0f, 0.0f};

extern C2D_SpriteSheet sprites;

void GFX::DrawSprite(int img, int x, int y, float ScaleX, float ScaleY)
{
	C2D_Image image = C2D_SpriteSheetGetImage(sprites, img);
	C3D_TexSetFilter(image.tex, GPU_LINEAR, GPU_LINEAR);
	C2D_DrawImageAt(image, x, y, 0.5f, NULL, ScaleX, ScaleY);
}
