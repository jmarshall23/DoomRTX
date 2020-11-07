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

#include "../renderer/tr_local.h"

extern idStr imageProgramFileName;

/*
PROBLEM: compressed textures may break the zero clamp rule!
*/

static bool FormatIsDXT( int internalFormat ) {
	return false;
}

int MakePowerOfTwo( int num ) {
	int		pot;
	for (pot = 1 ; pot < num ; pot<<=1) {
	}
	return pot;
}

/*
================
BitsForInternalFormat

Used for determining memory utilization
================
*/
int idImage::BitsForInternalFormat( int internalFormat ) const {
	
	return 0;
}

/*
==================
UploadCompressedNormalMap

Create a 256 color palette to be used by compressed normal maps
==================
*/
void idImage::UploadCompressedNormalMap( int width, int height, const byte *rgba, int mipLevel ) {
	
}


//=======================================================================


static byte	mipBlendColors[16][4] = {
	{0,0,0,0},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
};

/*
==================
SetImageFilterAndRepeat
==================
*/
void idImage::SetImageFilterAndRepeat() const {
	
}

/*
================
idImage::Downsize
helper function that takes the current width/height and might make them smaller
================
*/
void idImage::GetDownsize( int &scaled_width, int &scaled_height ) const {
	int size = 0;

	// perform optional picmip operation to save texture memory
	if ( depth == TD_SPECULAR && globalImages->image_downSizeSpecular.GetInteger() ) {
		size = globalImages->image_downSizeSpecularLimit.GetInteger();
		if ( size == 0 ) {
			size = 64;
		}
	} else if ( depth == TD_BUMP && globalImages->image_downSizeBump.GetInteger() ) {
		size = globalImages->image_downSizeBumpLimit.GetInteger();
		if ( size == 0 ) {
			size = 64;
		}
	} else if ( ( allowDownSize || globalImages->image_forceDownSize.GetBool() ) && globalImages->image_downSize.GetInteger() ) {
		size = globalImages->image_downSizeLimit.GetInteger();
		if ( size == 0 ) {
			size = 256;
		}
	}

	if ( size > 0 ) {
		while ( scaled_width > size || scaled_height > size ) {
			if ( scaled_width > 1 ) {
				scaled_width >>= 1;
			}
			if ( scaled_height > 1 ) {
				scaled_height >>= 1;
			}
		}
	}

	// clamp to minimum size
	if ( scaled_width < 1 ) {
		scaled_width = 1;
	}
	if ( scaled_height < 1 ) {
		scaled_height = 1;
	}

	// clamp size to the hardware specific upper limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	// This causes a 512*256 texture to sample down to
	// 256*128 on a voodoo3, even though it could be 256*256
	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}
}

/*
================
GenerateImage

The alpha channel bytes should be 255 if you don't
want the channel.

We need a material characteristic to ask for specific texture modes.

Designed limitations of flexibility:

No support for texture borders.

No support for texture border color.

No support for texture environment colors or GL_BLEND or GL_DECAL
texture environments, because the automatic optimization to single
or dual component textures makes those modes potentially undefined.

No non-power-of-two images.

No palettized textures.

There is no way to specify separate wrap/clamp values for S and T

There is no way to specify explicit mip map levels

================
*/
void idImage::GenerateImage( const byte *pic, int width, int height, 
					   textureFilter_t filterParm, bool allowDownSizeParm, 
					   textureRepeat_t repeatParm, textureDepth_t depthParm ) {
	bool	preserveBorder;
	byte		*scaledBuffer;
	int			scaled_width, scaled_height;
	byte		*shrunk;

	PurgeImage();

	averageColor.Zero();
	if (texnum == TEXTURE_NOT_LOADED) {
		texnum = globalImages->texnum++;
	}

	if (tr.diffuseMegaTexture != NULL && tr.normalMegaTexture != NULL)
	{
		if (depthParm == TD_DIFFUSE) {
			tr.diffuseMegaTexture->RegisterTexture(imgName.c_str(), width, height, (byte*)pic);

			int numNonAlphaPixels = 0;
			for (int i = 0; i < width * height; i+=4)
			{
				if (pic[i + 3] == 255)
				{
					averageColor.x += pic[i + 0];
					averageColor.y += pic[i + 1];
					averageColor.z += pic[i + 2];
					numNonAlphaPixels++;
				}
			}

			averageColor.x /= (float)numNonAlphaPixels;
			averageColor.y /= (float)numNonAlphaPixels;
			averageColor.z /= (float)numNonAlphaPixels;

			averageColor.x /= 255.0f;
			averageColor.y /= 255.0f;
			averageColor.z /= 255.0f;
		}
		else if (depthParm == TD_BUMP) {
			tr.normalMegaTexture->RegisterTexture(imgName.c_str(), width, height, (byte*)pic);
		}
		else {
			tr.diffuseMegaTexture->RegisterTexture(imgName.c_str(), width, height, (byte*)pic);
			GL_Upload32(texnum, (unsigned int*)pic, width, height, false, false);
		}
	}
	else
	{
		GL_Upload32(texnum, (unsigned int*)pic, width, height, false, false);
	}

	filter = filterParm;
	allowDownSize = allowDownSizeParm;
	repeat = repeatParm;
	depth = depthParm;
}


/*
==================
Generate3DImage
==================
*/
void idImage::Generate3DImage( const byte *pic, int width, int height, int picDepth,
					   textureFilter_t filterParm, bool allowDownSizeParm, 
					   textureRepeat_t repeatParm, textureDepth_t minDepthParm ) {
	int			scaled_width, scaled_height, scaled_depth;

	PurgeImage();

	filter = filterParm;
	allowDownSize = allowDownSizeParm;
	repeat = repeatParm;
	depth = minDepthParm;

	// if we don't have a rendering context, just return after we
	// have filled in the parms.  We must have the values set, or
	// an image match from a shader before OpenGL starts would miss
	// the generated texture
	if ( !glConfig.isInitialized ) {
		return;
	}

	// make sure it is a power of 2
	scaled_width = MakePowerOfTwo( width );
	scaled_height = MakePowerOfTwo( height );
	scaled_depth = MakePowerOfTwo( picDepth );
	if ( scaled_width != width || scaled_height != height || scaled_depth != picDepth ) {
		common->Error( "R_Create3DImage: not a power of 2 image" );
	}

	// FIXME: allow picmip here

	// generate the texture number
	//qglGenTextures( 1, &texnum );

	// select proper internal format before we resample
	// this function doesn't need to know it is 3D, so just make it very "tall"
	internalFormat = 0; //SelectInternalFormat( &pic, 1, width, height * picDepth, minDepthParm, &isMonochrome );

	uploadHeight = scaled_height;
	uploadWidth = scaled_width;
	uploadDepth = scaled_depth;


	type = TT_3D;

	// upload the main image level
	Bind();

//	//qglTexImage3D(GL_TEXTURE_3D, 0, internalFormat, scaled_width, scaled_height, scaled_depth,
	//	0, GL_RGBA, GL_UNSIGNED_BYTE, pic );

	// create and upload the mip map levels
	int		miplevel;
	byte	*scaledBuffer, *shrunk;

	scaledBuffer = (byte *)R_StaticAlloc( scaled_width * scaled_height * scaled_depth * 4 );
	memcpy( scaledBuffer, pic, scaled_width * scaled_height * scaled_depth * 4 );
	miplevel = 0;
	while ( scaled_width > 1 || scaled_height > 1 || scaled_depth > 1 ) {
		// preserve the border after mip map unless repeating
		shrunk = R_MipMap3D( scaledBuffer, scaled_width, scaled_height, scaled_depth,
			(bool)(repeat != TR_REPEAT) );
		R_StaticFree( scaledBuffer );
		scaledBuffer = shrunk;

		scaled_width >>= 1;
		scaled_height >>= 1;
		scaled_depth >>= 1;
		if ( scaled_width < 1 ) {
			scaled_width = 1;
		}
		if ( scaled_height < 1 ) {
			scaled_height = 1;
		}
		if ( scaled_depth < 1 ) {
			scaled_depth = 1;
		}
		miplevel++;

		// upload the mip map
		//qglTexImage3D(GL_TEXTURE_3D, miplevel, internalFormat, scaled_width, scaled_height, scaled_depth,
		//	0, GL_RGBA, GL_UNSIGNED_BYTE, scaledBuffer );
	}
	R_StaticFree( scaledBuffer );

	// set the minimize / maximize filtering
	//switch( filter ) {
	//case TF_DEFAULT:
	//	qglTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, globalImages->textureMinFilter );
	//	qglTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, globalImages->textureMaxFilter );
	//	break;
	//case TF_LINEAR:
	//	qglTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	//	qglTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	//	break;
	//case TF_NEAREST:
	//	qglTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	//	qglTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	//	break;
	//default:
	//	common->FatalError( "R_CreateImage: bad texture filter" );
	//}
	//
	//// set the wrap/clamp modes
	//switch( repeat ) {
	//case TR_REPEAT:
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT );
	//	break;
	//case TR_CLAMP_TO_BORDER:
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
	//	break;
	//case TR_CLAMP_TO_ZERO:
	//case TR_CLAMP_TO_ZERO_ALPHA:
	//case TR_CLAMP:
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	//	qglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	//	break;
	//default:
	//	common->FatalError( "R_CreateImage: bad texture repeat" );
	//}

	// see if we messed anything up
	GL_CheckErrors();
}


/*
====================
GenerateCubeImage

Non-square cube sides are not allowed
====================
*/
void idImage::GenerateCubeImage( const byte *pic[6], int size, 
					   textureFilter_t filterParm, bool allowDownSizeParm, 
					   textureDepth_t depthParm ) {
	int			scaled_width, scaled_height;
	int			width, height;
	int			i;

	PurgeImage();

	filter = filterParm;
	allowDownSize = allowDownSizeParm;
	depth = depthParm;

	type = TT_CUBIC;

	// if we don't have a rendering context, just return after we
	// have filled in the parms.  We must have the values set, or
	// an image match from a shader before OpenGL starts would miss
	// the generated texture
	if ( !glConfig.isInitialized ) {
		return;
	}

	if ( ! glConfig.cubeMapAvailable ) {
		return;
	}

	width = height = size;

	// generate the texture number
	//qglGenTextures( 1, &texnum );

	// select proper internal format before we resample
	internalFormat = 0; //SelectInternalFormat( pic, 6, width, height, depth, &isMonochrome );

	// don't bother with downsample for now
	scaled_width = width;
	scaled_height = height;

	uploadHeight = scaled_height;
	uploadWidth = scaled_width;

	Bind();

	// no other clamp mode makes sense
//	qglTexParameteri(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//	qglTexParameteri(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//
//	// set the minimize / maximize filtering
//	switch( filter ) {
//	case TF_DEFAULT:
//		qglTexParameterf(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_MIN_FILTER, globalImages->textureMinFilter );
//		qglTexParameterf(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_MAG_FILTER, globalImages->textureMaxFilter );
//		break;
//	case TF_LINEAR:
//		qglTexParameterf(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
//		qglTexParameterf(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
//		break;
//	case TF_NEAREST:
//		qglTexParameterf(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
//		qglTexParameterf(GL_TEXTURE_CUBE_MAP_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
//		break;
//	default:
//		common->FatalError( "R_CreateImage: bad texture filter" );
//	}
//
//	// upload the base level
//	// FIXME: support GL_COLOR_INDEX8_EXT?
//	for ( i = 0 ; i < 6 ; i++ ) {
//		qglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT+i, 0, internalFormat, scaled_width, scaled_height, 0, 
//			GL_RGBA, GL_UNSIGNED_BYTE, pic[i] );
//	}


	// create and upload the mip map levels
	int		miplevel;
	byte	*shrunk[6];

	for ( i = 0 ; i < 6 ; i++ ) {
		shrunk[i] = R_MipMap( pic[i], scaled_width, scaled_height, false );
	}

	miplevel = 1;
	while ( scaled_width > 1 ) {
		for ( i = 0 ; i < 6 ; i++ ) {
			byte	*shrunken;

			//qglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT+i, miplevel, internalFormat, 
			//	scaled_width / 2, scaled_height / 2, 0, 
			//	GL_RGBA, GL_UNSIGNED_BYTE, shrunk[i] );

			if ( scaled_width > 2 ) {
				shrunken = R_MipMap( shrunk[i], scaled_width/2, scaled_height/2, false );
			} else {
				shrunken = NULL;
			}

			R_StaticFree( shrunk[i] );
			shrunk[i] = shrunken;
		}

		scaled_width >>= 1;
		scaled_height >>= 1;
		miplevel++;
	}

	// see if we messed anything up
	GL_CheckErrors();
}


/*
================
ImageProgramStringToFileCompressedFileName
================
*/
void idImage::ImageProgramStringToCompressedFileName( const char *imageProg, char *fileName ) const {
	const char	*s;
	char	*f;

	strcpy( fileName, "dds/" );
	f = fileName + strlen( fileName );

	int depth = 0;

	// convert all illegal characters to underscores
	// this could conceivably produce a duplicated mapping, but we aren't going to worry about it
	for ( s = imageProg ; *s ; s++ ) {
		if ( *s == '/' || *s == '\\' || *s == '(') {
			if ( depth < 4 ) {
				*f = '/';
				depth ++;
			} else {
				*f = ' ';
			}
			f++;
		} else if ( *s == '<' || *s == '>' || *s == ':' || *s == '|' || *s == '"' || *s == '.' ) {
			*f = '_';
			f++;
		} else if ( *s == ' ' && *(f-1) == '/' ) {	// ignore a space right after a slash
		} else if ( *s == ')' || *s == ',' ) {		// always ignore these
		} else {
			*f = *s;
			f++;
		}
	}
	*f++ = 0;
	strcat( fileName, ".dds" );
}

/*
==================
NumLevelsForImageSize
==================
*/
int	idImage::NumLevelsForImageSize( int width, int height ) const {
	int	numLevels = 1;

	while ( width > 1 || height > 1 ) {
		numLevels++;
		width >>= 1;
		height >>= 1;
	}

	return numLevels;
}

/*
================
WritePrecompressedImage

When we are happy with our source data, we can write out precompressed
versions of everything to speed future load times.
================
*/
void idImage::WritePrecompressedImage() {

	// Always write the precompressed image if we're making a build
	if ( !com_makingBuild.GetBool() ) {
		if ( !globalImages->image_writePrecompressedTextures.GetBool() || !globalImages->image_usePrecompressedTextures.GetBool() ) {
			return;
		}
	}

	if ( !glConfig.isInitialized ) {
		return;
	}

	char filename[MAX_IMAGE_NAME];
	ImageProgramStringToCompressedFileName( imgName, filename );



	int numLevels = NumLevelsForImageSize( uploadWidth, uploadHeight );
	if ( numLevels > MAX_TEXTURE_LEVELS ) {
		common->Warning( "R_WritePrecompressedImage: level > MAX_TEXTURE_LEVELS for image %s", filename );
		return;
	}

	// glGetTexImage only supports a small subset of all the available internal formats
	// We have to use BGRA because DDS is a windows based format
	int altInternalFormat = 0;
	int bitSize = 0;
	//switch ( internalFormat ) {
	//	case GL_COLOR_INDEX8_EXT:
	//	case GL_COLOR_INDEX:
	//		// this will not work with dds viewers but we need it in this format to save disk
	//		// load speed ( i.e. size ) 
	//		altInternalFormat = GL_COLOR_INDEX;
	//		bitSize = 24;
	//	break;
	//	case 1:
	//	case GL_INTENSITY8:
	//	case GL_LUMINANCE8:
	//	case 3:
	//	case GL_RGB8:
	//		altInternalFormat = GL_BGR_EXT;
	//		bitSize = 24;
	//	break;
	//	case GL_LUMINANCE8_ALPHA8:
	//	case 4:
	//	case GL_RGBA8:
	//		altInternalFormat = GL_BGRA_EXT;
	//		bitSize = 32;
	//	break;
	//	case GL_ALPHA8:
	//		altInternalFormat = GL_ALPHA;
	//		bitSize = 8;
	//	break;
	//	default:
	//		if ( FormatIsDXT( internalFormat ) ) {
	//			altInternalFormat = internalFormat;
	//		} else {
	//			common->Warning("Unknown or unsupported format for %s", filename);
	//			return;
	//		}
	//}

	//if ( globalImages->image_useOffLineCompression.GetBool() && FormatIsDXT( altInternalFormat ) ) {
	//	idStr outFile = fileSystem->RelativePathToOSPath( filename, "fs_basepath" );
	//	idStr inFile = outFile;
	//	inFile.StripFileExtension();
	//	inFile.SetFileExtension( "tga" );
	//	idStr format;
	//	if ( depth == TD_BUMP ) {
	//		format = "RXGB +red 0.0 +green 0.5 +blue 0.5";
	//	} else {
	//		switch ( altInternalFormat ) {
	//			case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
	//				format = "DXT1";
	//				break;
	//			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
	//				format = "DXT1 -alpha_threshold";
	//				break;
	//			case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
	//				format = "DXT3";
	//				break;
	//			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
	//				format = "DXT5";
	//				break;
	//		}
	//	}
	//	globalImages->AddDDSCommand( va( "z:/d3xp/compressonator/thecompressonator -convert \"%s\" \"%s\" %s -mipmaps\n", inFile.c_str(), outFile.c_str(), format.c_str() ) );
	//	return;
	//}


	ddsFileHeader_t header;
	memset( &header, 0, sizeof(header) );
	header.dwSize = sizeof(header);
	header.dwFlags = DDSF_CAPS | DDSF_PIXELFORMAT | DDSF_WIDTH | DDSF_HEIGHT;
	header.dwHeight = uploadHeight;
	header.dwWidth = uploadWidth;

	// hack in our monochrome flag for the NV20 optimization
	if ( isMonochrome ) {
		header.dwFlags |= DDSF_ID_MONOCHROME;
	}

	//if ( FormatIsDXT( altInternalFormat ) ) {
	//	// size (in bytes) of the compressed base image
	//	header.dwFlags |= DDSF_LINEARSIZE;
	//	header.dwPitchOrLinearSize = ( ( uploadWidth + 3 ) / 4 ) * ( ( uploadHeight + 3 ) / 4 )*
	//		(altInternalFormat <= GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16);
	//}
	//else {
	//	// 4 Byte aligned line width (from nv_dds)
	//	header.dwFlags |= DDSF_PITCH;
	//	header.dwPitchOrLinearSize = ( ( uploadWidth * bitSize + 31 ) & -32 ) >> 3;
	//}

	header.dwCaps1 = DDSF_TEXTURE;

	if ( numLevels > 1 ) {
		header.dwMipMapCount = numLevels;
		header.dwFlags |= DDSF_MIPMAPCOUNT;
		header.dwCaps1 |= DDSF_MIPMAP | DDSF_COMPLEX;
	}

	header.ddspf.dwSize = sizeof(header.ddspf);
	//if ( FormatIsDXT( altInternalFormat ) ) {
	//	header.ddspf.dwFlags = DDSF_FOURCC;
	//	switch ( altInternalFormat ) {
	//	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
	//		header.ddspf.dwFourCC = DDS_MAKEFOURCC('D','X','T','1');
	//		break;
	//	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
	//		header.ddspf.dwFlags |= DDSF_ALPHAPIXELS;
	//		header.ddspf.dwFourCC = DDS_MAKEFOURCC('D','X','T','1');
	//		break;
	//	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
	//		header.ddspf.dwFourCC = DDS_MAKEFOURCC('D','X','T','3');
	//		break;
	//	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
	//		header.ddspf.dwFourCC = DDS_MAKEFOURCC('D','X','T','5');
	//		break;
	//	}
	//} else {
	//	header.ddspf.dwFlags = ( internalFormat == GL_COLOR_INDEX8_EXT ) ? DDSF_RGB | DDSF_ID_INDEXCOLOR : DDSF_RGB;
	//	header.ddspf.dwRGBBitCount = bitSize;
	//	switch ( altInternalFormat ) {
	//	case GL_BGRA_EXT:
	//	case GL_LUMINANCE_ALPHA:
	//		header.ddspf.dwFlags |= DDSF_ALPHAPIXELS;
	//		header.ddspf.dwABitMask = 0xFF000000;
	//		// Fall through
	//	case GL_BGR_EXT:
	//	case GL_LUMINANCE:
	//	case GL_COLOR_INDEX:
	//		header.ddspf.dwRBitMask = 0x00FF0000;
	//		header.ddspf.dwGBitMask = 0x0000FF00;
	//		header.ddspf.dwBBitMask = 0x000000FF;
	//		break;
	//	case GL_ALPHA:
	//		header.ddspf.dwFlags = DDSF_ALPHAPIXELS;
	//		header.ddspf.dwABitMask = 0xFF000000;
	//		break;
	//	default:
	//		common->Warning( "Unknown or unsupported format for %s", filename );
	//		return;
	//	}
	//}

	idFile *f = fileSystem->OpenFileWrite( filename );
	if ( f == NULL ) {
		common->Warning( "Could not open %s trying to write precompressed image", filename );
		return;
	}
	common->Printf( "Writing precompressed image: %s\n", filename );

	f->Write( "DDS ", 4 );
	f->Write( &header, sizeof(header) );

	// bind to the image so we can read back the contents
	Bind();

	//qglPixelStorei( GL_PACK_ALIGNMENT, 1 );	// otherwise small rows get padded to 32 bits

	int uw = uploadWidth;
	int uh = uploadHeight;

	// Will be allocated first time through the loop
	byte *data = NULL;

	//for ( int level = 0 ; level < numLevels ; level++ ) {
	//
	//	int size = 0;
	//	if ( FormatIsDXT( altInternalFormat ) ) {
	//		size = ( ( uw + 3 ) / 4 ) * ( ( uh + 3 ) / 4 ) *
	//			(altInternalFormat <= GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16);
	//	} else {
	//		size = uw * uh * (bitSize / 8);
	//	}
	//
	//	if (data == NULL) {
	//		data = (byte *)R_StaticAlloc( size );
	//	}
	//
	//	if ( FormatIsDXT( altInternalFormat ) ) {
	//		qglGetCompressedTexImageARB( GL_TEXTURE_2D, level, data );
	//	} else {
	//		qglGetTexImage( GL_TEXTURE_2D, level, altInternalFormat, GL_UNSIGNED_BYTE, data );
	//	}
	//
	//	f->Write( data, size );
	//
	//	uw /= 2;
	//	uh /= 2;
	//	if (uw < 1) {
	//		uw = 1;
	//	}
	//	if (uh < 1) {
	//		uh = 1;
	//	}
	//}

	if (data != NULL) {
		R_StaticFree( data );
	}

	fileSystem->CloseFile( f );
}

/*
================
ShouldImageBePartialCached

Returns true if there is a precompressed image, and it is large enough
to be worth caching
================
*/
bool idImage::ShouldImageBePartialCached() {
	if ( !glConfig.textureCompressionAvailable ) {
		return false;
	}

	if ( !globalImages->image_useCache.GetBool() ) {
		return false;
	}

	// the allowDownSize flag does double-duty as don't-partial-load
	if ( !allowDownSize ) {
		return false;
	}

	if ( globalImages->image_cacheMinK.GetInteger() <= 0 ) {
		return false;
	}

	// if we are doing a copyFiles, make sure the original images are referenced
	if ( fileSystem->PerformingCopyFiles() ) {
		return false;
	}

	char	filename[MAX_IMAGE_NAME];
	ImageProgramStringToCompressedFileName( imgName, filename );

	// get the file timestamp
	fileSystem->ReadFile( filename, NULL, &timestamp );

	if ( timestamp == FILE_NOT_FOUND_TIMESTAMP ) {
		return false;
	}

	// open it and get the file size
	idFile *f;

	f = fileSystem->OpenFileRead( filename );
	if ( !f ) {
		return false;
	}

	int	len = f->Length();
	fileSystem->CloseFile( f );

	if ( len <= globalImages->image_cacheMinK.GetInteger() * 1024 ) {
		return false;
	}

	// we do want to do a partial load
	return true;
}

/*
================
CheckPrecompressedImage

If fullLoad is false, only the small mip levels of the image will be loaded
================
*/
bool idImage::CheckPrecompressedImage( bool fullLoad ) {
	if ( !glConfig.isInitialized || !glConfig.textureCompressionAvailable ) {
		return false;
	}

#if 1 // ( _D3XP had disabled ) - Allow grabbing of DDS's from original Doom pak files
	// if we are doing a copyFiles, make sure the original images are referenced
	if ( fileSystem->PerformingCopyFiles() ) {
		return false;
	}
#endif

	if ( depth == TD_BUMP && globalImages->image_useNormalCompression.GetInteger() != 2 ) {
		return false;
	}

	// god i love last minute hacks :-)
	if ( com_machineSpec.GetInteger() >= 1 && com_videoRam.GetInteger() >= 128 && imgName.Icmpn( "lights/", 7 ) == 0 ) {
		return false;
	}

	char filename[MAX_IMAGE_NAME];
	ImageProgramStringToCompressedFileName( imgName, filename );

	// get the file timestamp
	ID_TIME_T precompTimestamp;
	fileSystem->ReadFile( filename, NULL, &precompTimestamp );


	if ( precompTimestamp == FILE_NOT_FOUND_TIMESTAMP ) {
		return false;
	}

	if ( !generatorFunction && timestamp != FILE_NOT_FOUND_TIMESTAMP ) {
		if ( precompTimestamp < timestamp ) {
			// The image has changed after being precompressed
			return false;
		}
	}

	timestamp = precompTimestamp;

	// open it and just read the header
	idFile *f;

	f = fileSystem->OpenFileRead( filename );
	if ( !f ) {
		return false;
	}

	int	len = f->Length();
	if ( len < sizeof( ddsFileHeader_t ) ) {
		fileSystem->CloseFile( f );
		return false;
	}

	if ( !fullLoad && len > globalImages->image_cacheMinK.GetInteger() * 1024 ) {
		len = globalImages->image_cacheMinK.GetInteger() * 1024;
	}

	byte *data = (byte *)R_StaticAlloc( len );

	f->Read( data, len );

	fileSystem->CloseFile( f );

	unsigned long magic = LittleLong( *(unsigned long *)data );
	ddsFileHeader_t	*_header = (ddsFileHeader_t *)(data + 4);
	int ddspf_dwFlags = LittleLong( _header->ddspf.dwFlags );

	if ( magic != DDS_MAKEFOURCC('D', 'D', 'S', ' ')) {
		common->Printf( "CheckPrecompressedImage( %s ): magic != 'DDS '\n", imgName.c_str() );
		R_StaticFree( data );
		return false;
	}

	// if we don't support color index textures, we must load the full image
	// should we just expand the 256 color image to 32 bit for upload?
	if ( ddspf_dwFlags & DDSF_ID_INDEXCOLOR && !glConfig.sharedTexturePaletteAvailable ) {
		R_StaticFree( data );
		return false;
	}

	// upload all the levels
	UploadPrecompressedImage( data, len );

	R_StaticFree( data );

	return true;
}

/*
===================
UploadPrecompressedImage

This can be called by the front end during nromal loading,
or by the backend after a background read of the file
has completed
===================
*/
void idImage::UploadPrecompressedImage( byte *data, int len ) {
	ddsFileHeader_t	*header = (ddsFileHeader_t *)(data + 4);

	// ( not byte swapping dwReserved1 dwReserved2 )
	header->dwSize = LittleLong( header->dwSize );
	header->dwFlags = LittleLong( header->dwFlags );
	header->dwHeight = LittleLong( header->dwHeight );
	header->dwWidth = LittleLong( header->dwWidth );
	header->dwPitchOrLinearSize = LittleLong( header->dwPitchOrLinearSize );
	header->dwDepth = LittleLong( header->dwDepth );
	header->dwMipMapCount = LittleLong( header->dwMipMapCount );
	header->dwCaps1 = LittleLong( header->dwCaps1 );
	header->dwCaps2 = LittleLong( header->dwCaps2 );

	header->ddspf.dwSize = LittleLong( header->ddspf.dwSize );
	header->ddspf.dwFlags = LittleLong( header->ddspf.dwFlags );
	header->ddspf.dwFourCC = LittleLong( header->ddspf.dwFourCC );
	header->ddspf.dwRGBBitCount = LittleLong( header->ddspf.dwRGBBitCount );
	header->ddspf.dwRBitMask = LittleLong( header->ddspf.dwRBitMask );
	header->ddspf.dwGBitMask = LittleLong( header->ddspf.dwGBitMask );
	header->ddspf.dwBBitMask = LittleLong( header->ddspf.dwBBitMask );
	header->ddspf.dwABitMask = LittleLong( header->ddspf.dwABitMask );

	// generate the texture number
	//qglGenTextures( 1, &texnum );

	int externalFormat = 0;

	precompressedFile = true;

	uploadWidth = header->dwWidth;
	uploadHeight = header->dwHeight;
    //if ( header->ddspf.dwFlags & DDSF_FOURCC ) {
    //    switch ( header->ddspf.dwFourCC ) {
    //    case DDS_MAKEFOURCC( 'D', 'X', 'T', '1' ):
	//		if ( header->ddspf.dwFlags & DDSF_ALPHAPIXELS ) {
	//			internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	//		} else {
	//			internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
	//		}
    //        break;
    //    case DDS_MAKEFOURCC( 'D', 'X', 'T', '3' ):
    //        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    //        break;
    //    case DDS_MAKEFOURCC( 'D', 'X', 'T', '5' ):
    //        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    //        break;
	//	case DDS_MAKEFOURCC( 'R', 'X', 'G', 'B' ):
	//		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	//		break;
    //    default:
    //        common->Warning( "Invalid compressed internal format\n" );
    //        return;
    //    }
    //} else if ( ( header->ddspf.dwFlags & DDSF_RGBA ) && header->ddspf.dwRGBBitCount == 32 ) {
	//	externalFormat = GL_BGRA_EXT;
	//	internalFormat = GL_RGBA8;
    //} else if ( ( header->ddspf.dwFlags & DDSF_RGB ) && header->ddspf.dwRGBBitCount == 32 ) {
    //    externalFormat = GL_BGRA_EXT;
	//	internalFormat = GL_RGBA8;
    //} else if ( ( header->ddspf.dwFlags & DDSF_RGB ) && header->ddspf.dwRGBBitCount == 24 ) {
	//	if ( header->ddspf.dwFlags & DDSF_ID_INDEXCOLOR ) { 
	//		externalFormat = GL_COLOR_INDEX;
	//		internalFormat = GL_COLOR_INDEX8_EXT;
	//	} else {
	//		externalFormat = GL_BGR_EXT;
	//		internalFormat = GL_RGB8;
	//	}
	//} else if ( header->ddspf.dwRGBBitCount == 8 ) {
	//	externalFormat = GL_ALPHA;
	//	internalFormat = GL_ALPHA8;
	//} else {
	//	common->Warning( "Invalid uncompressed internal format\n" );
	//	return;
	//}

	// we need the monochrome flag for the NV20 optimized path
	if ( header->dwFlags & DDSF_ID_MONOCHROME ) {
		isMonochrome = true;
	}

	type = TT_2D;			// FIXME: we may want to support pre-compressed cube maps in the future

	Bind();

	int numMipmaps = 1;
	if ( header->dwFlags & DDSF_MIPMAPCOUNT ) {
		numMipmaps = header->dwMipMapCount;
	}

	int uw = uploadWidth;
	int uh = uploadHeight;

	// We may skip some mip maps if we are downsizing
	int skipMip = 0;
	GetDownsize( uploadWidth, uploadHeight );

	byte *imagedata = data + sizeof(ddsFileHeader_t) + 4;

	//for ( int i = 0 ; i < numMipmaps; i++ ) {
	//	int size = 0;
	//	if ( FormatIsDXT( internalFormat ) ) {
	//		size = ( ( uw + 3 ) / 4 ) * ( ( uh + 3 ) / 4 ) *
	//			(internalFormat <= GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16);
	//	} else {
	//		size = uw * uh * (header->ddspf.dwRGBBitCount / 8);
	//	}
	//
	//	if ( uw > uploadWidth || uh > uploadHeight ) {
	//		skipMip++;
	//	} else {
	//		if ( FormatIsDXT( internalFormat ) ) {
	//			qglCompressedTexImage2DARB( GL_TEXTURE_2D, i - skipMip, internalFormat, uw, uh, 0, size, imagedata );
	//		} else {
	//			qglTexImage2D( GL_TEXTURE_2D, i - skipMip, internalFormat, uw, uh, 0, externalFormat, GL_UNSIGNED_BYTE, imagedata );
	//		}
	//	}
	//
	//	imagedata += size;
	//	uw /= 2;
	//	uh /= 2;
	//	if (uw < 1) {
	//		uw = 1;
	//	}
	//	if (uh < 1) {
	//		uh = 1;
	//	}
	//}

	SetImageFilterAndRepeat();
}

/*
===============
ActuallyLoadImage

Absolutely every image goes through this path
On exit, the idImage will have a valid OpenGL texture number that can be bound
===============
*/
void	idImage::ActuallyLoadImage( bool checkForPrecompressed, bool fromBackEnd ) {
	int		width, height;
	byte	*pic;

	// this is the ONLY place generatorFunction will ever be called
	if ( generatorFunction ) {
		generatorFunction( this );
		return;
	}

	// if we are a partial image, we are only going to load from a compressed file
	if ( isPartialImage ) {
		if ( CheckPrecompressedImage( false ) ) {
			return;
		}
		// this is an error -- the partial image failed to load
		MakeDefault();
		return;
	}

	//
	// load the image from disk
	//
	if ( cubeFiles != CF_2D ) {
		byte	*pics[6];

		// we don't check for pre-compressed cube images currently
		R_LoadCubeImages( imgName, cubeFiles, pics, &width, &timestamp );

		if ( pics[0] == NULL ) {
			common->Warning( "Couldn't load cube image: %s", imgName.c_str() );
			MakeDefault();
			return;
		}

		GenerateCubeImage( (const byte **)pics, width, filter, allowDownSize, depth );
		precompressedFile = false;

		for ( int i = 0 ; i < 6 ; i++ ) {
			if ( pics[i] ) {
				R_StaticFree( pics[i] );
			}
		}
	} else {
		// see if we have a pre-generated image file that is
		// already image processed and compressed
		//if ( checkForPrecompressed && globalImages->image_usePrecompressedTextures.GetBool() ) {
		//	if ( CheckPrecompressedImage( true ) ) {
		//		// we got the precompressed image
		//		return;
		//	}
		//	// fall through to load the normal image
		//}

		R_LoadImageProgram( imgName, &pic, &width, &height, &timestamp, &depth );
		if ( pic == NULL ) {
			common->Warning( "Couldn't load image: %s", imgName.c_str() );
			MakeDefault();
			return;
		}
/*
		// swap the red and alpha for rxgb support
		// do this even on tga normal maps so we only have to use
		// one fragment program
		// if the image is precompressed ( either in palletized mode or true rxgb mode )
		// then it is loaded above and the swap never happens here
		if ( depth == TD_BUMP && globalImages->image_useNormalCompression.GetInteger() != 1 ) {
			for ( int i = 0; i < width * height * 4; i += 4 ) {
				pic[ i + 3 ] = pic[ i ];
				pic[ i ] = 0;
			}
		}
*/
		// build a hash for checking duplicate image files
		// NOTE: takes about 10% of image load times (SD)
		// may not be strictly necessary, but some code uses it, so let's leave it in
		//imageHash = MD4_BlockChecksum( pic, width * height * 4 );

		GenerateImage( pic, width, height, filter, allowDownSize, repeat, depth );
		timestamp = timestamp;
		precompressedFile = false;

		R_StaticFree( pic );

		// write out the precompressed version of this file if needed
		WritePrecompressedImage();
	}
}

//=========================================================================================================

/*
===============
PurgeImage
===============
*/
void idImage::PurgeImage() {
	if ( texnum != TEXTURE_NOT_LOADED ) {
		// sometimes is NULL when exiting with an error
		//if ( qglDeleteTextures ) {
		//	qglDeleteTextures( 1, &texnum );	// this should be the ONLY place it is ever called!
		//}
		texnum = TEXTURE_NOT_LOADED;
	}

	// clear all the current binding caches, so the next bind will do a real one
	for ( int i = 0 ; i < MAX_MULTITEXTURE_UNITS ; i++ ) {
		backEnd.glState.tmu[i].current2DMap = -1;
		backEnd.glState.tmu[i].current3DMap = -1;
		backEnd.glState.tmu[i].currentCubeMap = -1;
	}
}

/*
==============
Bind

Automatically enables 2D mapping, cube mapping, or 3D texturing if needed
==============
*/
void idImage::Bind() {
	if ( tr.logFile ) {
		RB_LogComment( "idImage::Bind( %s )\n", imgName.c_str() );
	}

	// if this is an image that we are caching, move it to the front of the LRU chain
	if ( partialImage ) {
		if ( cacheUsageNext ) {
			// unlink from old position
			cacheUsageNext->cacheUsagePrev = cacheUsagePrev;
			cacheUsagePrev->cacheUsageNext = cacheUsageNext;
		}
		// link in at the head of the list
		cacheUsageNext = globalImages->cacheLRU.cacheUsageNext;
		cacheUsagePrev = &globalImages->cacheLRU;

		cacheUsageNext->cacheUsagePrev = this;
		cacheUsagePrev->cacheUsageNext = this;
	}

	// load the image if necessary (FIXME: not SMP safe!)
	if ( texnum == TEXTURE_NOT_LOADED ) {
		if ( partialImage ) {
			// if we have a partial image, go ahead and use that
			this->partialImage->Bind();

			// start a background load of the full thing if it isn't already in the queue
			if ( !backgroundLoadInProgress ) {
				StartBackgroundImageLoad();
			}
			return;
		}

		// load the image on demand here, which isn't our normal game operating mode
		//ActuallyLoadImage( true, true );	// check for precompressed, load is from back end
	}


	// bump our statistic counters
	frameUsed = backEnd.frameCount;
	bindCount++;

	tmu_t			*tmu = &backEnd.glState.tmu[backEnd.glState.currenttmu];

	// enable or disable apropriate texture modes
	//if ( tmu->textureType != type && ( backEnd.glState.currenttmu <	glConfig.maxTextureUnits ) ) {
	//	if ( tmu->textureType == TT_CUBIC ) {
	//		qglDisable( GL_TEXTURE_CUBE_MAP_EXT );
	//	} else if ( tmu->textureType == TT_3D ) {
	//		qglDisable( GL_TEXTURE_3D );
	//	} else if ( tmu->textureType == TT_2D ) {
	//		qglDisable( GL_TEXTURE_2D );
	//	}
	//
	//	if ( type == TT_CUBIC ) {
	//		qglEnable( GL_TEXTURE_CUBE_MAP_EXT );
	//	} else if ( type == TT_3D ) {
	//		qglEnable( GL_TEXTURE_3D );
	//	} else if ( type == TT_2D ) {
	//		qglEnable( GL_TEXTURE_2D );
	//	}
	//	tmu->textureType = type;
	//}
	//
	//// bind the texture
	//if ( type == TT_2D ) {
	//	if ( tmu->current2DMap != texnum ) {
	//		tmu->current2DMap = texnum;
	//		qglBindTexture( GL_TEXTURE_2D, texnum );
	//	}
	//} else if ( type == TT_CUBIC ) {
	//	if ( tmu->currentCubeMap != texnum ) {
	//		tmu->currentCubeMap = texnum;
	//		qglBindTexture( GL_TEXTURE_CUBE_MAP_EXT, texnum );
	//	}
	//} else if ( type == TT_3D ) {
	//	if ( tmu->current3DMap != texnum ) {
	//		tmu->current3DMap = texnum;
	//		qglBindTexture( GL_TEXTURE_3D, texnum );
	//	}
	//}
	//
	//if ( com_purgeAll.GetBool() ) {
	//	GLclampf priority = 1.0f;
	//	qglPrioritizeTextures( 1, &texnum, &priority );
	//}
}

/*
==============
BindFragment

Fragment programs explicitly say which type of map they want, so we don't need to
do any enable / disable changes
==============
*/
void idImage::BindFragment() {
	if ( tr.logFile ) {
		RB_LogComment( "idImage::BindFragment %s )\n", imgName.c_str() );
	}

	// if this is an image that we are caching, move it to the front of the LRU chain
	if ( partialImage ) {
		if ( cacheUsageNext ) {
			// unlink from old position
			cacheUsageNext->cacheUsagePrev = cacheUsagePrev;
			cacheUsagePrev->cacheUsageNext = cacheUsageNext;
		}
		// link in at the head of the list
		cacheUsageNext = globalImages->cacheLRU.cacheUsageNext;
		cacheUsagePrev = &globalImages->cacheLRU;

		cacheUsageNext->cacheUsagePrev = this;
		cacheUsagePrev->cacheUsageNext = this;
	}

	// load the image if necessary (FIXME: not SMP safe!)
	if ( texnum == TEXTURE_NOT_LOADED ) {
		if ( partialImage ) {
			// if we have a partial image, go ahead and use that
			this->partialImage->BindFragment();

			// start a background load of the full thing if it isn't already in the queue
			if ( !backgroundLoadInProgress ) {
				StartBackgroundImageLoad();
			}
			return;
		}

		// load the image on demand here, which isn't our normal game operating mode
		ActuallyLoadImage( true, true );	// check for precompressed, load is from back end
	}


	// bump our statistic counters
	frameUsed = backEnd.frameCount;
	bindCount++;

	// bind the texture
	//if ( type == TT_2D ) {
	//	qglBindTexture( GL_TEXTURE_2D, texnum );
	//} else if ( type == TT_RECT ) {
	//	qglBindTexture( GL_TEXTURE_RECTANGLE_NV, texnum );
	//} else if ( type == TT_CUBIC ) {
	//	qglBindTexture( GL_TEXTURE_CUBE_MAP_EXT, texnum );
	//} else if ( type == TT_3D ) {
	//	qglBindTexture( GL_TEXTURE_3D, texnum );
	//}
}


/*
====================
CopyFramebuffer
====================
*/
void idImage::CopyFramebuffer( int x, int y, int imageWidth, int imageHeight, bool useOversizedBuffer ) {
	
}

/*
====================
CopyDepthbuffer

This should just be part of copyFramebuffer once we have a proper image type field
====================
*/
void idImage::CopyDepthbuffer( int x, int y, int imageWidth, int imageHeight ) {
	
}

/*
=============
RB_UploadScratchImage

if rows = cols * 6, assume it is a cube map animation
=============
*/
void idImage::UploadScratch( const byte *data, int cols, int rows ) {
	
}


void idImage::SetClassification( int tag ) {
	classification = tag;
}

/*
==================
StorageSize
==================
*/
int idImage::StorageSize() const {
	int		baseSize;

	if ( texnum == TEXTURE_NOT_LOADED ) {
		return 0;
	}

	switch ( type ) {
	default:
	case TT_2D:
		baseSize = uploadWidth*uploadHeight;
		break;
	case TT_3D:
		baseSize = uploadWidth*uploadHeight*uploadDepth;
		break;
	case TT_CUBIC:
		baseSize = 6 * uploadWidth*uploadHeight;
		break;
	}

	baseSize *= BitsForInternalFormat( internalFormat );

	baseSize /= 8;

	// account for mip mapping
	baseSize = baseSize * 4 / 3;

	return baseSize;
}

/*
==================
Print
==================
*/
void idImage::Print() const {

}
