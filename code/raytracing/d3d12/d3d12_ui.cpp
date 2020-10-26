// d3d12_ui.cpp
//

#include "precompiled.h"
#include "d3d12_local.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include <vector>

template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
	assert(!(hi < lo));
	return (v < lo) ? lo : (hi < v) ? hi : v;
}

#define MAX_LOADED_TEXTURES			10000

extern byte* uiTextureBuffer;

struct glTexture_t {
	int width;
	int height;
	byte* data;

	glTexture_t() {
		width = -1;
		height = -1;
		data = NULL;
	}
};



glTexture_t textures[MAX_LOADED_TEXTURES];
float canvas_x = 0;
float canvas_y = 0;

/*
==============
GL_SetUICanvas
==============
*/
void GL_SetUICanvas(float x, float y, float width, float height) {
	canvas_x = x;
	canvas_y = y;
}

#ifdef _DEBUG
#pragma optimize( "", on )
#endif

/*
==============
R_CopyImage
==============
*/
void R_CopyImage(byte* source, int sourceX, int sourceY, int sourceWidth, byte* dest, int destX, int destY, int destWidth, int width, int height)
{
	destX = destX + canvas_x;
	destY = destY + canvas_y;

	for (int y = 0; y < height; y++)
	{
		int _x = 0;
		int _y = y * 4;
		int destPos = (destWidth * (_y + (destY * 4))) + (_x + (destX * 4));
		int sourcePos = (sourceWidth * (_y + (sourceY * 4))) + (_x + (sourceX * 4));

		memcpy(&dest[destPos], &source[sourcePos], width * 4);		
	}
}

/*
==============
R_Dropsample
==============
*/
unsigned char* R_Dropsample(const unsigned char* in, int inwidth, int inheight, int outwidth, int outheight) {
	int		i, j, k;
	const unsigned char* inrow;
	const unsigned char* pix1;
	unsigned char* out, * out_p;
	static unsigned char ViewportPixelBuffer[4096 * 4096 * 4];

	out = &ViewportPixelBuffer[0];
	out_p = out;

	int bpp = 4;	
	for (i = 0; i < outheight; i++, out_p += outwidth * bpp) {
		inrow = in + bpp * inwidth * (int)((i + 0.25) * inheight / outheight);
		for (j = 0; j < outwidth; j++) {
			k = j * inwidth / outwidth;
			pix1 = inrow + k * bpp;
			out_p[j * 4 + 0] = pix1[0];
			out_p[j * 4 + 1] = pix1[1];
			out_p[j * 4 + 2] = pix1[2];
			out_p[j * 4 + 3] = pix1[3];
			//out_p[j * 3 + 1] = pix1[1];
			//out_p[j * 3 + 2] = pix1[2];
		}
	}

	return out;
}

#ifdef _DEBUG
#pragma optimize( "", off )
#endif

/*
=================
GL_BlitUIImage
=================
*/
void GL_BlitUIImage(int texnum, int srcx, int srcy, int destx, int desty) {
	byte* src = textures[texnum].data;
	int width = textures[texnum].width; 
	int height = textures[texnum].height;
	if (destx < 0 || desty < 0 || destx + width > glConfig.vidWidth || desty + height > glConfig.vidHeight)
		return;

	R_CopyImage(src, srcx, srcy, width, (byte *)uiTextureBuffer, destx, desty, glConfig.vidWidth, width, height);
}

/*
=================
GL_BlitUIImage
=================
*/
void GL_BlitUIImageUV(int texnum, float u, float v, float u2, float v2, int destx, int desty, int w, int h) {
	static byte blit_temp[4096 * 4096 * 4];

	if (destx < 0 || desty < 0 || destx + w > glConfig.vidWidth || desty + h > glConfig.vidHeight || w <= 0 || h <= 0)
		return;

	byte* src = textures[texnum].data;
	int width = textures[texnum].width;
	int height = textures[texnum].height;

	// Texture not loaded yet. 
	if (width == -1 || height == -1 || w == -1 || h == -1) {
		common->Warning( "Tried to render a texture that hasn't been registered yet!\n");
		return;
	}

	u = clamp<float>(u, 0.0, 1.0f);
	v = clamp<float>(v, 0.0, 1.0f);

	u2 = clamp<float>(u2, 0.0, 1.0f);
	v2 = clamp<float>(v2, 0.0, 1.0f);

	float target_x = u * width;
	float target_y = v * height;
	float target_x2 = u2 * width;
	float target_y2 = v2 * height;

	int target_width = target_x2 - target_x;
	int target_height = target_y2 - target_y;

	if (target_width == 0 || target_height == 0)
		return;
	
	// Copy the segment of the texture to blit_temp.
	R_CopyImage(src, target_x, target_y, width, (byte*)blit_temp, 0, 0, target_width, target_width, target_height);
	unsigned char* new_src = R_Dropsample(blit_temp, target_width, target_height, w, h);
	R_CopyImage(new_src, 0, 0, w, (byte*)uiTextureBuffer, destx, desty, glConfig.vidWidth, w, h);
}

/*
=================
GL_BlitUIImage
=================
*/
void GL_BlitUIImageUVNoScale(int texnum, float u, float v, int destx, int desty, int w, int h) {
	if (destx < 0 || desty < 0 || destx + w > glConfig.vidWidth || desty + h > glConfig.vidHeight)
		return;

	byte* src = textures[texnum].data;	
	int width = textures[texnum].width;
	R_CopyImage(src, u, v, width, (byte*)uiTextureBuffer, destx, desty, glConfig.vidWidth, w, h);
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32(int textureId, unsigned* data, int width, int height, bool mipmap, bool alpha)
{
	textures[textureId].width = width;
	textures[textureId].height = height;

	if (textures[textureId].data != NULL)
		delete textures[textureId].data;

	textures[textureId].data = new byte[width * height * 4];
	memcpy(textures[textureId].data, data, width * height * 4);
}