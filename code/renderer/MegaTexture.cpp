/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"

idCVar r_megaTextureSize("r_megaTextureSize", "16384", CVAR_INTEGER | CVAR_ROM, "size of the megatexture");

/*
==============
R_CopyImage
==============
*/
void R_CopyImage(byte* source, int sourceX, int sourceY, int sourceWidth, byte* dest, int destX, int destY, int destWidth, int width, int height)
{	
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
=====================
iceMegaTexture::iceMegaTexture
=====================
*/
iceMegaTexture::iceMegaTexture() {
    isRegistered = false;
    megaTextureBuffer = NULL;
    imagePacker = NULL;
}
/*
=====================
iceMegaTexture::iceMegaTexture
=====================
*/
iceMegaTexture::~iceMegaTexture() {
    for (int i = 0; i < megaEntries.Num(); i++)
    {
        delete megaEntries[i].data_copy;
        megaEntries[i].data_copy = NULL;
    }
    megaEntries.Clear();

    if (megaTextureBuffer != NULL) {
        delete megaTextureBuffer;
        megaTextureBuffer = NULL;
    }

    if (imagePacker != NULL) {
        delete imagePacker;
    }
}

/*
=======================
iceMegaTexture::InitTexture
=======================
*/
void iceMegaTexture::InitTexture(void) {
	int megaSize = r_megaTextureSize.GetInteger();

	common->Printf("Init MegaTexture %dx%d\n", megaSize, megaSize);
    imagePacker = new idImagePacker(megaSize, megaSize);

    megaTextureBuffer = new byte[megaSize * megaSize * 4];
    memset(megaTextureBuffer, 0, megaSize * megaSize * 4);
}

/*
=======================
iceMegaTexture::RegisterTexture
=======================
*/
void iceMegaTexture::RegisterTexture(const char* texturePath, int width, int height, byte* data) {
    int tileId = megaEntries.Num();

    if (isRegistered) {
        common->Warning("iceMegaTexture::RegisterTexture: %s trying to be registered outside of registration!\n", texturePath);
        return;
    }

    //common->Printf("RegisterTexture: %s\n", texturePath);

    idSubImage subImage = imagePacker->PackImage(width, height, false);

    // Check to make sure we don't have any megaEntries
    iceMegaEntry newEntry;
    newEntry.texturePath = texturePath;
    newEntry.width = width;
    newEntry.height = height;
    newEntry.x = subImage.x;
    newEntry.y = subImage.y;
    newEntry.data_copy = new byte[width * height * 4];
    memcpy(newEntry.data_copy, data, width * height * 4);

    megaEntries.Append(newEntry);
}

/*
=======================
iceMegaTexture::BuildMegaTexture
=======================
*/
void iceMegaTexture::BuildMegaTexture(void) {
	int megaSize = r_megaTextureSize.GetInteger();

    // Update all of our megatexture entries.
    common->Printf("Updating %d entries...\n", megaEntries.Num());
    for (int i = 0; i < megaEntries.Num(); i++)
    {
		if (megaEntries[i].x + megaEntries[i].width > r_megaTextureSize.GetInteger()) {
			continue;
		}

		if (megaEntries[i].y + megaEntries[i].height > r_megaTextureSize.GetInteger()) {
			continue;
		}
        R_CopyImage(megaEntries[i].data_copy, 0, 0, megaEntries[i].width, megaTextureBuffer, megaEntries[i].x, megaEntries[i].y, r_megaTextureSize.GetInteger(), megaEntries[i].width, megaEntries[i].height);
    }

   // R_WriteTGA("testme.tga", megaTextureBuffer, r_megaTextureSize.GetInteger(), r_megaTextureSize.GetInteger());

    isRegistered = true;
}

/*
=======================
iceMegaTexture::FindMegaTile
=======================
*/
void iceMegaTexture::FindMegaTile(const char* name, float& x, float& y, float& width, float& height)
{
	for (int i = 0; i < megaEntries.Num(); i++) {
		if (megaEntries[i].texturePath == name) {
			x = megaEntries[i].x;
			y = megaEntries[i].y;
			width = megaEntries[i].width;
			height = megaEntries[i].height;
			return;
		}
	}

	for (int i = 0; i < megaEntries.Num(); i++) {
		if (strstr(megaEntries[i].texturePath.c_str(), name)) {
			x = megaEntries[i].x;
			y = megaEntries[i].y;
			width = megaEntries[i].width;
			height = megaEntries[i].height;
			return;
		}
	}
	x = -1;
	y = -1;
	width = -1;
	height = -1;
}
