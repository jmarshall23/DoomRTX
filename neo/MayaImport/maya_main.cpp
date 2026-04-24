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

#undef DotProduct
#define FLT_EPSILON 1.19209290e-07F

#include <fbxsdk.h>

// exporter.h was originally authored beside the Maya SDK and stores an MFnDagNode*
// in idExportJoint.  For this FBX-backed build, make that member an FbxNode*.
class MObject;
class MFnSkinCluster;
#define MFnDagNode FbxNode
#include "exporter.h"
#undef MFnDagNode
#include "maya_main.h"

idStr	errorMessage;
bool	initialized = false;

#define DEFAULT_ANIM_EPSILON	0.125f
#define DEFAULT_QUAT_EPSILON	( 1.0f / 8192.0f )

#define SLOP_VERTEX				0.01f			// merge xyz coordinates this far apart
#define	SLOP_TEXCOORD			0.001f			// merge texture coordinates this far apart

const char* componentNames[6] = { "Tx", "Ty", "Tz", "Qx", "Qy", "Qz" };

idSys* sys = NULL;
idCommon* common = NULL;
idCVarSystem* cvarSystem = NULL;

idCVar* idCVar::staticVars = NULL;

/*
=================
MayaError
=================
*/
void MayaError(const char* fmt, ...) {
	va_list	argptr;
	char	text[8192];

	va_start(argptr, fmt);
	idStr::vsnPrintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	throw idException(text);
}

/*
=================
FS_WriteFloatString
=================
*/
#define	MAX_PRINT_MSG	4096
static int WriteFloatString(FILE* file, const char* fmt, ...) {
	long i;
	unsigned long u;
	double f;
	char* str;
	int index;
	idStr tmp, format;
	va_list argPtr;

	va_start(argPtr, fmt);

	index = 0;

	while (*fmt) {
		switch (*fmt) {
		case '%':
			format = "";
			format += *fmt++;
			while ((*fmt >= '0' && *fmt <= '9') ||
				*fmt == '.' || *fmt == '-' || *fmt == '+' || *fmt == '#') {
				format += *fmt++;
			}
			format += *fmt;
			switch (*fmt) {
			case 'f':
			case 'e':
			case 'E':
			case 'g':
			case 'G':
				f = va_arg(argPtr, double);
				if (format.Length() <= 2) {
					// high precision floating point number without trailing zeros
					sprintf(tmp, "%1.10f", f);
					tmp.StripTrailing('0');
					tmp.StripTrailing('.');
					index += fprintf(file, "%s", tmp.c_str());
				}
				else {
					index += fprintf(file, format.c_str(), f);
				}
				break;
			case 'd':
			case 'i':
				i = va_arg(argPtr, long);
				index += fprintf(file, format.c_str(), i);
				break;
			case 'u':
				u = va_arg(argPtr, unsigned long);
				index += fprintf(file, format.c_str(), u);
				break;
			case 'o':
				u = va_arg(argPtr, unsigned long);
				index += fprintf(file, format.c_str(), u);
				break;
			case 'x':
				u = va_arg(argPtr, unsigned long);
				index += fprintf(file, format.c_str(), u);
				break;
			case 'X':
				u = va_arg(argPtr, unsigned long);
				index += fprintf(file, format.c_str(), u);
				break;
			case 'c':
				i = va_arg(argPtr, long);
				index += fprintf(file, format.c_str(), (char)i);
				break;
			case 's':
				str = va_arg(argPtr, char*);
				index += fprintf(file, format.c_str(), str);
				break;
			case '%':
				index += fprintf(file, format.c_str());
				break;
			default:
				MayaError("WriteFloatString: invalid format %s", format.c_str());
				break;
			}
			fmt++;
			break;
		case '\\':
			fmt++;
			switch (*fmt) {
			case 't':
				index += fprintf(file, "\t");
				break;
			case 'n':
				index += fprintf(file, "\n");
			default:
				MayaError("WriteFloatString: unknown escape character \'%c\'", *fmt);
				break;
			}
			fmt++;
			break;
		default:
			index += fprintf(file, "%c", *fmt);
			fmt++;
			break;
		}
	}

	va_end(argPtr);

	return index;
}

/*
================
OSPathToRelativePath

takes a full OS path, as might be found in data from a media creation
program, and converts it to a qpath by stripping off directories

Returns false if the osPath tree doesn't match any of the existing
search paths.
================
*/
bool OSPathToRelativePath(const char* osPath, idStr& qpath, const char* game) {
	char* s, * base;

	// skip a drive letter?

	// search for anything with BASE_GAMEDIR in it
	// Ase files from max may have the form of:
	// "//Purgatory/purgatory/doom/base/models/mapobjects/bitch/hologirl.tga"
	// which won't match any of our drive letter based search paths
	base = (char*)strstr(osPath, BASE_GAMEDIR);

	// _D3XP added mod support
	if (base == NULL && strlen(game) > 0) {

		base = s = (char*)strstr(osPath, game);

		while (s = strstr(s, game)) {
			s += strlen(game);
			if (s[0] == '/' || s[0] == '\\') {
				base = s;
			}
		}
	}

	if (base) {
		s = strstr(base, "/");
		if (!s) {
			s = strstr(base, "\\");
		}
		if (s) {
			qpath = s + 1;
			return true;
		}
	}

	common->Printf("OSPathToRelativePath failed on %s\n", osPath);
	qpath = osPath;

	return false;
}

/*
===============
ConvertFromIdSpace
===============
*/
idMat3 ConvertFromIdSpace(const idMat3& idmat) {
	idMat3 mat;

	mat[0][0] = idmat[0][0];
	mat[0][2] = -idmat[0][1];
	mat[0][1] = idmat[0][2];

	mat[1][0] = idmat[1][0];
	mat[1][2] = -idmat[1][1];
	mat[1][1] = idmat[1][2];

	mat[2][0] = idmat[2][0];
	mat[2][2] = -idmat[2][1];
	mat[2][1] = idmat[2][2];

	return mat;
}

/*
===============
ConvertFromIdSpace
===============
*/
idVec3 ConvertFromIdSpace(const idVec3& idpos) {
	idVec3 pos;

	pos.x = idpos.x;
	pos.z = -idpos.y;
	pos.y = idpos.z;

	return pos;
}

/*
===============
ConvertToIdSpace
===============
*/
idMat3 ConvertToIdSpace(const idMat3& mat) {
	idMat3 idmat;

	idmat[0][0] = mat[0][0];
	idmat[0][1] = -mat[0][2];
	idmat[0][2] = mat[0][1];

	idmat[1][0] = mat[1][0];
	idmat[1][1] = -mat[1][2];
	idmat[1][2] = mat[1][1];

	idmat[2][0] = mat[2][0];
	idmat[2][1] = -mat[2][2];
	idmat[2][2] = mat[2][1];

	return idmat;
}

/*
===============
ConvertToIdSpace
===============
*/
idVec3 ConvertToIdSpace(const idVec3& pos) {
	idVec3 idpos;

	idpos.x = pos.x;
	idpos.y = -pos.z;
	idpos.z = pos.y;

	return idpos;
}

/*
===============
idVec / idMat FBX helpers
===============
*/
static idVec3 idVec(const FbxVector4& point) {
	return idVec3((float)point[0], (float)point[1], (float)point[2]);
}

static idVec3 idVec(const FbxDouble3& point) {
	return idVec3((float)point[0], (float)point[1], (float)point[2]);
}

static idMat3 idMat(const FbxAMatrix& matrix) {
	int		j, k;
	idMat3	mat;

	for (j = 0; j < 3; j++) {
		for (k = 0; k < 3; k++) {
			mat[j][k] = (float)matrix.Get(j, k);
		}
	}

	mat.OrthoNormalizeSelf();
	return mat;
}

static FbxAMatrix FbxGetGeometryTransform(const FbxNode* node) {
	FbxAMatrix geo;
	geo.SetIdentity();
	if (node == NULL) {
		return geo;
	}

	FbxVector4 t = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	FbxVector4 r = node->GetGeometricRotation(FbxNode::eSourcePivot);
	FbxVector4 s = node->GetGeometricScaling(FbxNode::eSourcePivot);
	geo.SetTRS(t, r, s);
	return geo;
}

/*
==============================================================================================

	idTokenizer

==============================================================================================
*/

/*
====================
idTokenizer::SetTokens
====================
*/
int idTokenizer::SetTokens(const char* buffer) {
	const char* cmd;

	Clear();

	// tokenize commandline
	cmd = buffer;
	while (*cmd) {
		// skip whitespace
		while (*cmd && isspace(*cmd)) {
			cmd++;
		}

		if (!*cmd) {
			break;
		}

		idStr& current = tokens.Alloc();
		while (*cmd && !isspace(*cmd)) {
			current += *cmd;
			cmd++;
		}
	}

	return tokens.Num();
}

/*
====================
idTokenizer::NextToken
====================
*/
const char* idTokenizer::NextToken(const char* errorstring) {
	if (currentToken < tokens.Num()) {
		return tokens[currentToken++];
	}

	if (errorstring) {
		MayaError("Error: %s", errorstring);
	}

	return NULL;
}

/*
==============================================================================================

	idExportOptions

==============================================================================================
*/

/*
====================
idExportOptions::Reset
====================
*/
void idExportOptions::Reset(const char* commandline) {
	scale = 1.0f;
	type = WRITE_MESH;
	startframe = -1;
	endframe = -1;
	ignoreMeshes = false;
	clearOrigin = false;
	clearOriginAxis = false;
	framerate = 24;
	align = "";
	rotate = 0.0f;
	commandLine = commandline;
	prefix = "";
	jointThreshold = 0.05f;
	ignoreScale = false;
	xyzPrecision = DEFAULT_ANIM_EPSILON;
	quatPrecision = DEFAULT_QUAT_EPSILON;
	cycleStart = -1;

	src.Clear();
	dest.Clear();

	tokens.SetTokens(commandline);

	keepjoints.Clear();
	renamejoints.Clear();
	remapjoints.Clear();
	exportgroups.Clear();
	skipmeshes.Clear();
	keepmeshes.Clear();
	groups.Clear();
}

/*
====================
idExportOptions::idExportOptions
====================
*/
idExportOptions::idExportOptions(const char* commandline, const char* ospath) {
	idStr		token;
	idNamePair	joints;
	int			i;
	idAnimGroup* group;
	idStr		sourceDir;
	idStr		destDir;

	Reset(commandline);

	token = tokens.NextToken("Missing export command");
	if (token == "mesh") {
		type = WRITE_MESH;
	}
	else if (token == "md3") {
		// MD3 uses the mesh command path internally, but dispatches to WriteMD3 below.
		type = WRITE_MESH;
	}
	else if (token == "anim") {
		type = WRITE_ANIM;
	}
	else if (token == "camera") {
		type = WRITE_CAMERA;
	}
	else {
		MayaError("Unknown export command '%s'", token.c_str());
	}

	src = tokens.NextToken("Missing source filename");
	dest = src;

	for (token = tokens.NextToken(); token != ""; token = tokens.NextToken()) {
		if (token == "-force") {
			// skip
		}
		else if (token == "-game") {
			// parse game name
			game = tokens.NextToken("Expecting game name after -game");

		}
		else if (token == "-rename") {
			// parse joint to rename
			joints.from = tokens.NextToken("Missing joint name for -rename.  Usage: -rename [joint name] [new name]");
			joints.to = tokens.NextToken("Missing new name for -rename.  Usage: -rename [joint name] [new name]");
			renamejoints.Append(joints);

		}
		else if (token == "-prefix") {
			prefix = tokens.NextToken("Missing name for -prefix.  Usage: -prefix [joint prefix]");

		}
		else if (token == "-parent") {
			// parse joint to reparent
			joints.from = tokens.NextToken("Missing joint name for -parent.  Usage: -parent [joint name] [new parent]");
			joints.to = tokens.NextToken("Missing new parent for -parent.  Usage: -parent [joint name] [new parent]");
			remapjoints.Append(joints);

		}
		else if (!token.Icmp("-sourcedir")) {
			// parse source directory
			sourceDir = tokens.NextToken("Missing filename for -sourcedir.  Usage: -sourcedir [directory]");

		}
		else if (!token.Icmp("-destdir")) {
			// parse destination directory
			destDir = tokens.NextToken("Missing filename for -destdir.  Usage: -destdir [directory]");

		}
		else if (token == "-dest") {
			// parse destination filename
			dest = tokens.NextToken("Missing filename for -dest.  Usage: -dest [filename]");

		}
		else if (token == "-range") {
			// parse frame range to export
			token = tokens.NextToken("Missing start frame for -range.  Usage: -range [start frame] [end frame]");
			startframe = atoi(token);
			token = tokens.NextToken("Missing end frame for -range.  Usage: -range [start frame] [end frame]");
			endframe = atoi(token);

			if (startframe > endframe) {
				MayaError("Start frame is greater than end frame.");
			}

		}
		else if (!token.Icmp("-cycleStart")) {
			// parse start frame of cycle
			token = tokens.NextToken("Missing cycle start frame for -cycleStart.  Usage: -cycleStart [first frame of cycle]");
			cycleStart = atoi(token);

		}
		else if (token == "-scale") {
			// parse scale
			token = tokens.NextToken("Missing scale amount for -scale.  Usage: -scale [scale amount]");
			scale = atof(token);

		}
		else if (token == "-align") {
			// parse align joint
			align = tokens.NextToken("Missing joint name for -align.  Usage: -align [joint name]");

		}
		else if (token == "-rotate") {
			// parse angle rotation
			token = tokens.NextToken("Missing value for -rotate.  Usage: -rotate [yaw]");
			rotate = -atof(token);

		}
		else if (token == "-nomesh") {
			ignoreMeshes = true;

		}
		else if (token == "-clearorigin") {
			clearOrigin = true;
			clearOriginAxis = true;

		}
		else if (token == "-clearoriginaxis") {
			clearOriginAxis = true;

		}
		else if (token == "-ignorescale") {
			ignoreScale = true;

		}
		else if (token == "-xyzprecision") {
			// parse quaternion precision
			token = tokens.NextToken("Missing value for -xyzprecision.  Usage: -xyzprecision [precision]");
			xyzPrecision = atof(token);
			if (xyzPrecision < 0.0f) {
				MayaError("Invalid value for -xyzprecision.  Must be >= 0");
			}

		}
		else if (token == "-quatprecision") {
			// parse quaternion precision
			token = tokens.NextToken("Missing value for -quatprecision.  Usage: -quatprecision [precision]");
			quatPrecision = atof(token);
			if (quatPrecision < 0.0f) {
				MayaError("Invalid value for -quatprecision.  Must be >= 0");
			}

		}
		else if (token == "-jointthreshold") {
			// parse joint threshold
			token = tokens.NextToken("Missing weight for -jointthreshold.  Usage: -jointthreshold [minimum joint weight]");
			jointThreshold = atof(token);

		}
		else if (token == "-skipmesh") {
			token = tokens.NextToken("Missing name for -skipmesh.  Usage: -skipmesh [name of mesh to skip]");
			skipmeshes.AddUnique(token);

		}
		else if (token == "-keepmesh") {
			token = tokens.NextToken("Missing name for -keepmesh.  Usage: -keepmesh [name of mesh to keep]");
			keepmeshes.AddUnique(token);

		}
		else if (token == "-jointgroup") {
			token = tokens.NextToken("Missing name for -jointgroup.  Usage: -jointgroup [group name] [joint1] [joint2]...[joint n]");
			group = groups.Ptr();
			for (i = 0; i < groups.Num(); i++, group++) {
				if (group->name == token) {
					break;
				}
			}

			if (i >= groups.Num()) {
				// create a new group
				group = &groups.Alloc();
				group->name = token;
			}

			while (tokens.TokenAvailable()) {
				token = tokens.NextToken();
				if (token[0] == '-') {
					tokens.UnGetToken();
					break;
				}

				group->joints.AddUnique(token);
			}
		}
		else if (token == "-group") {
			// add the list of groups to export (these don't affect the hierarchy)
			while (tokens.TokenAvailable()) {
				token = tokens.NextToken();
				if (token[0] == '-') {
					tokens.UnGetToken();
					break;
				}

				group = groups.Ptr();
				for (i = 0; i < groups.Num(); i++, group++) {
					if (group->name == token) {
						break;
					}
				}

				if (i >= groups.Num()) {
					MayaError("Unknown group '%s'", token.c_str());
				}

				exportgroups.AddUnique(group);
			}
		}
		else if (token == "-keep") {
			// add joints that are kept whether they're used by a mesh or not
			while (tokens.TokenAvailable()) {
				token = tokens.NextToken();
				if (token[0] == '-') {
					tokens.UnGetToken();
					break;
				}
				keepjoints.AddUnique(token);
			}
		}
		else {
			MayaError("Unknown option '%s'", token.c_str());
		}
	}

	token = src;
	src = ospath;
	src.BackSlashesToSlashes();
	src.AppendPath(sourceDir);
	src.AppendPath(token);

	token = dest;
	dest = ospath;
	dest.BackSlashesToSlashes();
	dest.AppendPath(destDir);
	dest.AppendPath(token);

	// Maya only accepts unix style path separators
	src.BackSlashesToSlashes();
	dest.BackSlashesToSlashes();

	if (skipmeshes.Num() && keepmeshes.Num()) {
		MayaError("Can't use -keepmesh and -skipmesh together.");
	}
}

/*
====================
idExportOptions::jointInExportGroup
====================
*/
bool idExportOptions::jointInExportGroup(const char* jointname) {
	int			i;
	int			j;
	idAnimGroup* group;

	if (!exportgroups.Num()) {
		// if we don't have any groups specified as export then export every joint
		return true;
	}

	// search through all exported groups to see if this joint is exported
	for (i = 0; i < exportgroups.Num(); i++) {
		group = exportgroups[i];
		for (j = 0; j < group->joints.Num(); j++) {
			if (group->joints[j] == jointname) {
				return true;
			}
		}
	}

	return false;
}

/*
==============================================================================

idExportJoint

==============================================================================
*/

idExportJoint::idExportJoint() {
	index = 0;
	exportNum = 0;

	mayaNode.SetOwner(this);
	exportNode.SetOwner(this);

	dagnode = NULL;

	t = vec3_zero;
	wm = mat3_default;
	bindpos = vec3_zero;
	bindmat = mat3_default;
	keep = false;
	scale = 1.0f;
	invscale = 1.0f;
	animBits = 0;
	firstComponent = 0;
	baseFrame.q.Set(0.0f, 0.0f, 0.0f);
	baseFrame.t.Zero();
}

idExportJoint& idExportJoint::operator=(const idExportJoint& other) {
	name = other.name;
	realname = other.realname;
	longname = other.longname;
	index = other.index;
	exportNum = other.exportNum;
	keep = other.keep;

	scale = other.scale;
	invscale = other.invscale;

	dagnode = other.dagnode;

	mayaNode = other.mayaNode;
	exportNode = other.exportNode;

	t = other.t;
	idt = other.idt;
	wm = other.wm;
	idwm = other.idwm;
	bindpos = other.bindpos;
	bindmat = other.bindmat;

	animBits = other.animBits;
	firstComponent = other.firstComponent;
	baseFrame = other.baseFrame;

	mayaNode.SetOwner(this);
	exportNode.SetOwner(this);

	return *this;
}

/*
==============================================================================

idExportMesh

==============================================================================
*/

void idExportMesh::ShareVerts(void) {
	int i, j, k;
	exportVertex_t vert;
	idList<exportVertex_t> v;

	v = verts;
	verts.Clear();
	for (i = 0; i < tris.Num(); i++) {
		for (j = 0; j < 3; j++) {
			vert = v[tris[i].indexes[j]];
			vert.texCoords[0] = uv[i].uv[j][0];
			vert.texCoords[1] = 1.0f - uv[i].uv[j][1];

			for (k = 0; k < verts.Num(); k++) {
				if (vert.numWeights != verts[k].numWeights) {
					continue;
				}
				if (vert.startweight != verts[k].startweight) {
					continue;
				}
				if (!vert.pos.Compare(verts[k].pos, SLOP_VERTEX)) {
					continue;
				}
				if (!vert.texCoords.Compare(verts[k].texCoords, SLOP_TEXCOORD)) {
					continue;
				}

				break;
			}

			if (k < verts.Num()) {
				tris[i].indexes[j] = k;
			}
			else {
				tris[i].indexes[j] = verts.Append(vert);
			}
		}
	}
}

void idExportMesh::Merge(idExportMesh* mesh) {
	int i;
	int numverts;
	int numtris;
	int numweights;
	int numuvs;

	// merge name
	sprintf(name, "%s, %s", name.c_str(), mesh->name.c_str());

	// merge verts
	numverts = verts.Num();
	verts.SetNum(numverts + mesh->verts.Num());
	for (i = 0; i < mesh->verts.Num(); i++) {
		verts[numverts + i] = mesh->verts[i];
		verts[numverts + i].startweight += weights.Num();
	}

	// merge triangles
	numtris = tris.Num();
	tris.SetNum(numtris + mesh->tris.Num());
	for (i = 0; i < mesh->tris.Num(); i++) {
		tris[numtris + i].indexes[0] = mesh->tris[i].indexes[0] + numverts;
		tris[numtris + i].indexes[1] = mesh->tris[i].indexes[1] + numverts;
		tris[numtris + i].indexes[2] = mesh->tris[i].indexes[2] + numverts;
	}

	// merge weights
	numweights = weights.Num();
	weights.SetNum(numweights + mesh->weights.Num());
	for (i = 0; i < mesh->weights.Num(); i++) {
		weights[numweights + i] = mesh->weights[i];
	}

	// merge uvs
	numuvs = uv.Num();
	uv.SetNum(numuvs + mesh->uv.Num());
	for (i = 0; i < mesh->uv.Num(); i++) {
		uv[numuvs + i] = mesh->uv[i];
	}
}

void idExportMesh::GetBounds(idBounds& bounds) const {
	int						i;
	int						j;
	idVec3					pos;
	const exportWeight_t* weight;
	const exportVertex_t* vert;

	bounds.Clear();

	weight = weights.Ptr();
	vert = verts.Ptr();
	for (i = 0; i < verts.Num(); i++, vert++) {
		pos.Zero();
		weight = &weights[vert->startweight];
		for (j = 0; j < vert->numWeights; j++, weight++) {
			pos += weight->jointWeight * (weight->joint->idwm * weight->offset + weight->joint->idt);
		}
		bounds.AddPoint(pos);
	}
}

/*
==============================================================================

idExportModel

==============================================================================
*/

/*
====================
idExportModel::idExportModel
====================
*/
ID_INLINE idExportModel::idExportModel() {
	export_joints = 0;
	skipjoints = 0;
	frameRate = 24;
	numFrames = 0;
	exportOrigin = NULL;
}

/*
====================
idExportModel::~idExportModel
====================
*/
ID_INLINE idExportModel::~idExportModel() {
	meshes.DeleteContents(true);
}

idExportJoint* idExportModel::FindJointReal(const char* name) {
	idExportJoint* joint;
	int				i;

	joint = joints.Ptr();
	for (i = 0; i < joints.Num(); i++, joint++) {
		if (joint->realname == name) {
			return joint;
		}
	}

	return NULL;
}

idExportJoint* idExportModel::FindJoint(const char* name) {
	idExportJoint* joint;
	int				i;

	joint = joints.Ptr();
	for (i = 0; i < joints.Num(); i++, joint++) {
		if (joint->name == name) {
			return joint;
		}
	}

	return NULL;
}

bool idExportModel::WriteMesh(const char* filename, idExportOptions& options) {
	int				i, j;
	int				numMeshes;
	idExportMesh* mesh;
	idExportJoint* joint;
	idExportJoint* parent;
	idExportJoint* sibling;
	FILE* file;
	const char* parentName;
	int				parentNum;
	idList<idExportJoint*> jointList;

	file = fopen(filename, "w");
	if (!file) {
		return false;
	}

	for (joint = exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		jointList.Append(joint);
	}

	for (i = 0; i < jointList.Num(); i++) {
		joint = jointList[i];
		sibling = joint->exportNode.GetSibling();
		while (sibling) {
			if (idStr::Cmp(joint->name, sibling->name) > 0) {
				joint->exportNode.MakeSiblingAfter(sibling->exportNode);
				sibling = joint->exportNode.GetSibling();
			}
			else {
				sibling = sibling->exportNode.GetSibling();
			}
		}
	}

	jointList.Clear();
	for (joint = exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		joint->exportNum = jointList.Append(joint);
	}

	numMeshes = 0;
	if (!options.ignoreMeshes) {
		for (i = 0; i < meshes.Num(); i++) {
			if (meshes[i]->keep) {
				numMeshes++;
			}
		}
	}

	// write version info
	WriteFloatString(file, MD5_VERSION_STRING " %d\n", MD5_VERSION);
	WriteFloatString(file, "commandline \"%s\"\n\n", options.commandLine.c_str());

	// write joints
	WriteFloatString(file, "numJoints %d\n", jointList.Num());
	WriteFloatString(file, "numMeshes %d\n\n", numMeshes);

	WriteFloatString(file, "joints {\n");
	for (i = 0; i < jointList.Num(); i++) {
		joint = jointList[i];
		parent = joint->exportNode.GetParent();
		if (parent) {
			parentNum = parent->exportNum;
			parentName = parent->name.c_str();
		}
		else {
			parentNum = -1;
			parentName = "";
		}

		idCQuat	bindQuat = joint->bindmat.ToQuat().ToCQuat();
		WriteFloatString(file, "\t\"%s\"\t%d ( %f %f %f ) ( %f %f %f )\t\t// %s\n", joint->name.c_str(), parentNum,
			joint->bindpos.x, joint->bindpos.y, joint->bindpos.z, bindQuat[0], bindQuat[1], bindQuat[2], parentName);
	}
	WriteFloatString(file, "}\n");

	// write meshes
	for (i = 0; i < meshes.Num(); i++) {
		mesh = meshes[i];
		if (!mesh->keep) {
			continue;
		}

		WriteFloatString(file, "\nmesh {\n");
		WriteFloatString(file, "\t// meshes: %s\n", mesh->name.c_str());
		WriteFloatString(file, "\tshader \"%s\"\n", mesh->shader.c_str());

		WriteFloatString(file, "\n\tnumverts %d\n", mesh->verts.Num());
		for (j = 0; j < mesh->verts.Num(); j++) {
			WriteFloatString(file, "\tvert %d ( %f %f ) %d %d\n", j, mesh->verts[j].texCoords[0], mesh->verts[j].texCoords[1],
				mesh->verts[j].startweight, mesh->verts[j].numWeights);
		}

		WriteFloatString(file, "\n\tnumtris %d\n", mesh->tris.Num());
		for (j = 0; j < mesh->tris.Num(); j++) {
			WriteFloatString(file, "\ttri %d %d %d %d\n", j, mesh->tris[j].indexes[2], mesh->tris[j].indexes[1], mesh->tris[j].indexes[0]);
		}

		WriteFloatString(file, "\n\tnumweights %d\n", mesh->weights.Num());
		for (j = 0; j < mesh->weights.Num(); j++) {
			exportWeight_t* weight;

			weight = &mesh->weights[j];
			WriteFloatString(file, "\tweight %d %d %f ( %f %f %f )\n", j,
				weight->joint->exportNum, weight->jointWeight, weight->offset.x, weight->offset.y, weight->offset.z);
		}

		WriteFloatString(file, "}\n");
	}

	fclose(file);

	return true;
}

bool idExportModel::WriteAnim(const char* filename, idExportOptions& options) {
	int				i, j;
	idExportJoint* joint;
	idExportJoint* parent;
	idExportJoint* sibling;
	jointFrame_t* frame;
	FILE* file;
	int				numAnimatedComponents;
	idList<idExportJoint*> jointList;

	file = fopen(filename, "w");
	if (!file) {
		return false;
	}

	for (joint = exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		jointList.Append(joint);
	}

	for (i = 0; i < jointList.Num(); i++) {
		joint = jointList[i];
		sibling = joint->exportNode.GetSibling();
		while (sibling) {
			if (idStr::Cmp(joint->name, sibling->name) > 0) {
				joint->exportNode.MakeSiblingAfter(sibling->exportNode);
				sibling = joint->exportNode.GetSibling();
			}
			else {
				sibling = sibling->exportNode.GetSibling();
			}
		}
	}

	jointList.Clear();
	for (joint = exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		joint->exportNum = jointList.Append(joint);
	}

	numAnimatedComponents = 0;
	for (i = 0; i < jointList.Num(); i++) {
		joint = jointList[i];
		joint->exportNum = i;
		joint->baseFrame = frames[0][joint->index];
		joint->animBits = 0;
		for (j = 1; j < numFrames; j++) {
			frame = &frames[j][joint->index];
			if (fabs(frame->t[0] - joint->baseFrame.t[0]) > options.xyzPrecision) {
				joint->animBits |= ANIM_TX;
			}
			if (fabs(frame->t[1] - joint->baseFrame.t[1]) > options.xyzPrecision) {
				joint->animBits |= ANIM_TY;
			}
			if (fabs(frame->t[2] - joint->baseFrame.t[2]) > options.xyzPrecision) {
				joint->animBits |= ANIM_TZ;
			}
			if (fabs(frame->q[0] - joint->baseFrame.q[0]) > options.quatPrecision) {
				joint->animBits |= ANIM_QX;
			}
			if (fabs(frame->q[1] - joint->baseFrame.q[1]) > options.quatPrecision) {
				joint->animBits |= ANIM_QY;
			}
			if (fabs(frame->q[2] - joint->baseFrame.q[2]) > options.quatPrecision) {
				joint->animBits |= ANIM_QZ;
			}
			if ((joint->animBits & 63) == 63) {
				break;
			}
		}
		if (joint->animBits) {
			joint->firstComponent = numAnimatedComponents;
			for (j = 0; j < 6; j++) {
				if (joint->animBits & BIT(j)) {
					numAnimatedComponents++;
				}
			}
		}
	}

	// write version info
	WriteFloatString(file, MD5_VERSION_STRING " %d\n", MD5_VERSION);
	WriteFloatString(file, "commandline \"%s\"\n\n", options.commandLine.c_str());

	WriteFloatString(file, "numFrames %d\n", numFrames);
	WriteFloatString(file, "numJoints %d\n", jointList.Num());
	WriteFloatString(file, "frameRate %d\n", frameRate);
	WriteFloatString(file, "numAnimatedComponents %d\n", numAnimatedComponents);

	// write out the hierarchy
	WriteFloatString(file, "\nhierarchy {\n");
	for (i = 0; i < jointList.Num(); i++) {
		joint = jointList[i];
		parent = joint->exportNode.GetParent();
		if (parent) {
			WriteFloatString(file, "\t\"%s\"\t%d %d %d\t// %s", joint->name.c_str(), parent->exportNum, joint->animBits, joint->firstComponent, parent->name.c_str());
		}
		else {
			WriteFloatString(file, "\t\"%s\"\t-1 %d %d\t//", joint->name.c_str(), joint->animBits, joint->firstComponent);
		}

		if (!joint->animBits) {
			WriteFloatString(file, "\n");
		}
		else {
			WriteFloatString(file, " ( ");
			for (j = 0; j < 6; j++) {
				if (joint->animBits & BIT(j)) {
					WriteFloatString(file, "%s ", componentNames[j]);
				}
			}
			WriteFloatString(file, ")\n");
		}
	}
	WriteFloatString(file, "}\n");

	// write the frame bounds
	WriteFloatString(file, "\nbounds {\n");
	for (i = 0; i < numFrames; i++) {
		WriteFloatString(file, "\t( %f %f %f ) ( %f %f %f )\n", bounds[i][0].x, bounds[i][0].y, bounds[i][0].z, bounds[i][1].x, bounds[i][1].y, bounds[i][1].z);
	}
	WriteFloatString(file, "}\n");

	// write the base frame
	WriteFloatString(file, "\nbaseframe {\n");
	for (i = 0; i < jointList.Num(); i++) {
		joint = jointList[i];
		WriteFloatString(file, "\t( %f %f %f ) ( %f %f %f )\n", joint->baseFrame.t[0], joint->baseFrame.t[1], joint->baseFrame.t[2],
			joint->baseFrame.q[0], joint->baseFrame.q[1], joint->baseFrame.q[2]);
	}
	WriteFloatString(file, "}\n");

	// write the frames
	for (i = 0; i < numFrames; i++) {
		WriteFloatString(file, "\nframe %d {\n", i);
		for (j = 0; j < jointList.Num(); j++) {
			joint = jointList[j];
			frame = &frames[i][joint->index];
			if (joint->animBits) {
				WriteFloatString(file, "\t");
				if (joint->animBits & ANIM_TX) {
					WriteFloatString(file, " %f", frame->t[0]);
				}
				if (joint->animBits & ANIM_TY) {
					WriteFloatString(file, " %f", frame->t[1]);
				}
				if (joint->animBits & ANIM_TZ) {
					WriteFloatString(file, " %f", frame->t[2]);
				}
				if (joint->animBits & ANIM_QX) {
					WriteFloatString(file, " %f", frame->q[0]);
				}
				if (joint->animBits & ANIM_QY) {
					WriteFloatString(file, " %f", frame->q[1]);
				}
				if (joint->animBits & ANIM_QZ) {
					WriteFloatString(file, " %f", frame->q[2]);
				}
				WriteFloatString(file, "\n");
			}
		}
		WriteFloatString(file, "}\n");
	}

	fclose(file);

	return true;
}

bool idExportModel::WriteCamera(const char* filename, idExportOptions& options) {
	int		i;
	FILE* file;

	file = fopen(filename, "w");
	if (!file) {
		return false;
	}

	// write version info
	WriteFloatString(file, MD5_VERSION_STRING " %d\n", MD5_VERSION);
	WriteFloatString(file, "commandline \"%s\"\n\n", options.commandLine.c_str());

	WriteFloatString(file, "numFrames %d\n", camera.Num());
	WriteFloatString(file, "frameRate %d\n", frameRate);
	WriteFloatString(file, "numCuts %d\n", cameraCuts.Num());

	// write out the cuts
	WriteFloatString(file, "\ncuts {\n");
	for (i = 0; i < cameraCuts.Num(); i++) {
		WriteFloatString(file, "\t%d\n", cameraCuts[i]);
	}
	WriteFloatString(file, "}\n");

	// write out the frames
	WriteFloatString(file, "\ncamera {\n");
	cameraFrame_t* frame = camera.Ptr();
	for (i = 0; i < camera.Num(); i++, frame++) {
		WriteFloatString(file, "\t( %f %f %f ) ( %f %f %f ) %f\n", frame->t.x, frame->t.y, frame->t.z, frame->q[0], frame->q[1], frame->q[2], frame->fov);
	}
	WriteFloatString(file, "}\n");

	fclose(file);

	return true;
}

/*
==============================================================================

FBX scene extraction

==============================================================================
*/

static bool IsMD3Export(idExportOptions& options) {
	idTokenizer tokens;
	tokens.SetTokens(options.commandLine.c_str());
	idStr first = tokens.NextToken();
	if (first == "md3") {
		return true;
	}
	if (options.dest.Length() >= 4) {
		idStr ext = options.dest.Right(4);
		if (!ext.Icmp(".md3")) {
			return true;
		}
	}
	return false;
}

static const char* FbxSafeName(const char* name) {
	return (name && name[0]) ? name : "unnamed";
}

static void MD3_CopyString(char* dst, int dstSize, const char* src) {
	memset(dst, 0, dstSize);
	if (src == NULL) {
		return;
	}
	strncpy(dst, src, dstSize - 1);
}

static int MD3_ClampShort(int v) {
	if (v < -32768) {
		return -32768;
	}
	if (v > 32767) {
		return 32767;
	}
	return v;
}

static short MD3_EncodeNormal(const idVec3& normal) {
	const float twoPi = 6.28318530717958647692f;
	idVec3 n = normal;
	if (n.Normalize() == 0.0f) {
		return 0;
	}

	int lat = (int)(atan2(n.y, n.x) * 255.0f / twoPi);
	int lng = (int)(acos(n.z) * 255.0f / twoPi);
	return (short)(((lat & 0xff) << 8) | (lng & 0xff));
}

static void MD3_BuildWorldPose(idExportModel& model, jointFrame_t* frame) {
	idExportJoint* joint;
	idExportJoint* parent;
	idVec3 jointpos;
	idMat3 jointaxis;

	for (joint = model.exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		jointpos = frame[joint->index].t;
		jointaxis = frame[joint->index].q.ToQuat().ToMat3();

		parent = joint->exportNode.GetParent();
		if (parent) {
			joint->idwm = jointaxis * parent->idwm;
			joint->idt = parent->idt + jointpos * parent->idwm;
		}
		else {
			joint->idwm = jointaxis;
			joint->idt = jointpos;
		}
	}
}

static void MD3_BuildMeshPositions(idExportMesh* mesh, idList<idVec3>& positions) {
	int i, j;
	exportVertex_t* vert;
	exportWeight_t* weight;
	idVec3 pos;

	positions.SetNum(mesh->verts.Num());
	for (i = 0; i < mesh->verts.Num(); i++) {
		vert = &mesh->verts[i];
		pos.Zero();
		weight = &mesh->weights[vert->startweight];
		for (j = 0; j < vert->numWeights; j++, weight++) {
			pos += weight->jointWeight * (weight->joint->idwm * weight->offset + weight->joint->idt);
		}
		positions[i] = pos;
	}
}

static void MD3_BuildMeshNormals(idExportMesh* mesh, const idList<idVec3>& positions, idList<idVec3>& normals) {
	int i, j;
	idVec3 a, b, n;
	int indexes[3];

	normals.SetNum(mesh->verts.Num());
	for (i = 0; i < normals.Num(); i++) {
		normals[i].Zero();
	}

	for (i = 0; i < mesh->tris.Num(); i++) {
		indexes[0] = mesh->tris[i].indexes[2];
		indexes[1] = mesh->tris[i].indexes[1];
		indexes[2] = mesh->tris[i].indexes[0];
		a = positions[indexes[1]] - positions[indexes[0]];
		b = positions[indexes[2]] - positions[indexes[0]];
		n = a.Cross(b);
		n.Normalize();
		for (j = 0; j < 3; j++) {
			normals[indexes[j]] += n;
		}
	}

	for (i = 0; i < normals.Num(); i++) {
		normals[i].Normalize();
	}
}

static void MD3_BuildTagList(idExportModel& model, idList<idExportJoint*>& tags) {
	idExportJoint* joint;

	tags.Clear();
	for (joint = model.exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		if (!joint->name.Icmpn("tag_", 4)) {
			if (tags.Num() >= MD3_MAX_TAGS) {
				MayaError("MD3 export supports only %d tags", MD3_MAX_TAGS);
			}
			tags.Append(joint);
		}
	}
}

static bool WriteMD3Model(idExportModel& model, const char* filename, idExportOptions& options) {
	int i, j, frameNum;
	int numSurfaces;
	idBounds bounds;
	idList<idExportJoint*> tags;
	idList<md3Frame_t> md3Frames;
	idList<idVec3> positions;
	idList<idVec3> normals;
	idExportMesh* mesh;
	FILE* file;

	MD3_BuildTagList(model, tags);

	if (model.numFrames < 1 || model.numFrames > MD3_MAX_FRAMES) {
		MayaError("MD3 export requires 1..%d frames, got %d", MD3_MAX_FRAMES, model.numFrames);
	}

	numSurfaces = 0;
	for (i = 0; i < model.meshes.Num(); i++) {
		if (model.meshes[i]->keep) {
			numSurfaces++;
		}
	}
	if (numSurfaces < 1) {
		MayaError("MD3 export has no mesh surfaces");
	}
	if (numSurfaces > MD3_MAX_SURFACES) {
		MayaError("MD3 export supports only %d surfaces, got %d", MD3_MAX_SURFACES, numSurfaces);
	}

	md3Frames.SetNum(model.numFrames);
	for (frameNum = 0; frameNum < model.numFrames; frameNum++) {
		MD3_BuildWorldPose(model, model.frames[frameNum]);

		bounds.Clear();
		for (i = 0; i < model.meshes.Num(); i++) {
			mesh = model.meshes[i];
			if (!mesh->keep) {
				continue;
			}
			MD3_BuildMeshPositions(mesh, positions);
			for (j = 0; j < positions.Num(); j++) {
				bounds.AddPoint(positions[j]);
			}
		}

		memset(&md3Frames[frameNum], 0, sizeof(md3Frames[frameNum]));
		md3Frames[frameNum].bounds[0] = bounds[0];
		md3Frames[frameNum].bounds[1] = bounds[1];
		md3Frames[frameNum].localOrigin = (bounds[0] + bounds[1]) * 0.5f;
		md3Frames[frameNum].radius = 0.0f;
		for (i = 0; i < 8; i++) {
			idVec3 corner;
			corner.x = (i & 1) ? bounds[1].x : bounds[0].x;
			corner.y = (i & 2) ? bounds[1].y : bounds[0].y;
			corner.z = (i & 4) ? bounds[1].z : bounds[0].z;
			float radius = (corner - md3Frames[frameNum].localOrigin).Length();
			if (radius > md3Frames[frameNum].radius) {
				md3Frames[frameNum].radius = radius;
			}
		}
		sprintf(md3Frames[frameNum].name, "frame%d", frameNum);
	}

	file = fopen(filename, "wb");
	if (!file) {
		return false;
	}

	md3Header_t header;
	memset(&header, 0, sizeof(header));
	header.ident = MD3_IDENT;
	header.version = MD3_VERSION;
	MD3_CopyString(header.name, sizeof(header.name), filename);
	header.numFrames = model.numFrames;
	header.numTags = tags.Num();
	header.numSurfaces = numSurfaces;
	header.numSkins = 0;
	header.ofsFrames = sizeof(md3Header_t);
	header.ofsTags = header.ofsFrames + sizeof(md3Frame_t) * header.numFrames;
	header.ofsSurfaces = header.ofsTags + sizeof(md3Tag_t) * header.numFrames * header.numTags;
	header.ofsEnd = header.ofsSurfaces;
	for (i = 0; i < model.meshes.Num(); i++) {
		mesh = model.meshes[i];
		if (!mesh->keep) {
			continue;
		}
		if (mesh->verts.Num() > MD3_MAX_VERTS) {
			MayaError("MD3 surface '%s' has %d verts, max is %d", mesh->name.c_str(), mesh->verts.Num(), MD3_MAX_VERTS);
		}
		if (mesh->tris.Num() > MD3_MAX_TRIANGLES) {
			MayaError("MD3 surface '%s' has %d tris, max is %d", mesh->name.c_str(), mesh->tris.Num(), MD3_MAX_TRIANGLES);
		}
		header.ofsEnd += sizeof(md3Surface_t) + sizeof(md3Shader_t) + sizeof(md3Triangle_t) * mesh->tris.Num() + sizeof(md3St_t) * mesh->verts.Num() + sizeof(md3XyzNormal_t) * mesh->verts.Num() * model.numFrames;
	}

	fwrite(&header, sizeof(header), 1, file);
	fwrite(md3Frames.Ptr(), sizeof(md3Frame_t), md3Frames.Num(), file);

	for (frameNum = 0; frameNum < model.numFrames; frameNum++) {
		MD3_BuildWorldPose(model, model.frames[frameNum]);
		for (i = 0; i < tags.Num(); i++) {
			md3Tag_t tag;
			memset(&tag, 0, sizeof(tag));
			MD3_CopyString(tag.name, sizeof(tag.name), tags[i]->name.c_str());
			tag.origin = tags[i]->idt;
			tag.axis[0] = tags[i]->idwm[0];
			tag.axis[1] = tags[i]->idwm[1];
			tag.axis[2] = tags[i]->idwm[2];
			fwrite(&tag, sizeof(tag), 1, file);
		}
	}

	for (i = 0; i < model.meshes.Num(); i++) {
		mesh = model.meshes[i];
		if (!mesh->keep) {
			continue;
		}

		md3Surface_t surface;
		memset(&surface, 0, sizeof(surface));
		surface.ident = MD3_IDENT;
		MD3_CopyString(surface.name, sizeof(surface.name), mesh->name.c_str());
		surface.flags = 0;
		surface.numFrames = model.numFrames;
		surface.numShaders = 1;
		surface.numVerts = mesh->verts.Num();
		surface.numTriangles = mesh->tris.Num();
		surface.ofsShaders = sizeof(md3Surface_t);
		surface.ofsTriangles = surface.ofsShaders + sizeof(md3Shader_t) * surface.numShaders;
		surface.ofsSt = surface.ofsTriangles + sizeof(md3Triangle_t) * surface.numTriangles;
		surface.ofsXyzNormals = surface.ofsSt + sizeof(md3St_t) * surface.numVerts;
		surface.ofsEnd = surface.ofsXyzNormals + sizeof(md3XyzNormal_t) * surface.numVerts * surface.numFrames;
		fwrite(&surface, sizeof(surface), 1, file);

		md3Shader_t shader;
		memset(&shader, 0, sizeof(shader));
		if (mesh->shader.Length()) {
			MD3_CopyString(shader.name, sizeof(shader.name), mesh->shader.c_str());
		}
		else {
			MD3_CopyString(shader.name, sizeof(shader.name), mesh->name.c_str());
		}
		shader.shaderIndex = 0;
		fwrite(&shader, sizeof(shader), 1, file);

		for (j = 0; j < mesh->tris.Num(); j++) {
			md3Triangle_t tri;
			tri.indexes[0] = mesh->tris[j].indexes[2];
			tri.indexes[1] = mesh->tris[j].indexes[1];
			tri.indexes[2] = mesh->tris[j].indexes[0];
			fwrite(&tri, sizeof(tri), 1, file);
		}

		for (j = 0; j < mesh->verts.Num(); j++) {
			md3St_t st;
			st.st[0] = mesh->verts[j].texCoords[0];
			st.st[1] = mesh->verts[j].texCoords[1];
			fwrite(&st, sizeof(st), 1, file);
		}

		for (frameNum = 0; frameNum < model.numFrames; frameNum++) {
			MD3_BuildWorldPose(model, model.frames[frameNum]);
			MD3_BuildMeshPositions(mesh, positions);
			MD3_BuildMeshNormals(mesh, positions, normals);
			for (j = 0; j < positions.Num(); j++) {
				md3XyzNormal_t xyz;
				xyz.xyz[0] = (short)MD3_ClampShort((int)floor(positions[j].x / MD3_XYZ_SCALE + 0.5f));
				xyz.xyz[1] = (short)MD3_ClampShort((int)floor(positions[j].y / MD3_XYZ_SCALE + 0.5f));
				xyz.xyz[2] = (short)MD3_ClampShort((int)floor(positions[j].z / MD3_XYZ_SCALE + 0.5f));
				xyz.normal = MD3_EncodeNormal(normals[j]);
				fwrite(&xyz, sizeof(xyz), 1, file);
			}
		}
	}

	fclose(file);
	return true;
}

class idFbxExport {
private:
	idExportModel		model;
	idExportOptions& options;
	FbxManager* sdkManager;
	FbxScene* scene;
	FbxAnimStack* animStack;
	FbxTime			currentTime;

	void			LoadScene(void);
	void			SetDefaultFrameRange(void);
	float			TimeForFrame(int num) const;
	int				GetFbxFrameNum(int num) const;
	FbxTime			TimeForFbxFrame(int num) const;
	void			SetFrame(int num);
	void			AddNodesRecursive(FbxNode* node);
	void			CreateJoints(float scale);
	idExportJoint* FindJointForNode(FbxNode* node);
	void			SetJointBindPose(idExportJoint* joint, const FbxAMatrix& matrix, float scale);
	void			GetWorldTransform(idExportJoint* joint, idVec3& pos, idMat3& mat, float scale);
	void			PruneJoints(idStrList& keepjoints, idStr& prefix);
	void			RenameJoints(idList<idNamePair>& renamejoints, idStr& prefix);
	bool			RemapParents(idList<idNamePair>& remapjoints);
	void			GetTextureForMesh(idExportMesh* mesh, FbxNode* node);
	idExportMesh* CopyMesh(FbxNode* node, FbxMesh* fbxMesh, float scale);
	void			CreateMeshRecursive(FbxNode* node, int& count);
	void			CreateMesh(float scale);
	void			CombineMeshes(void);
	void			GetAlignment(idStr& alignName, idMat3& align, float rotate, int startframe);
	idExportJoint* FindFirstCamera(void);
	float			GetCameraFov(idExportJoint* joint);
	void			GetCameraFrame(idExportJoint* camera, idMat3& align, cameraFrame_t* cam);
	void			CreateCameraAnim(idMat3& align);
	void			GetDefaultPose(idMat3& align);
	void			CreateAnimation(idMat3& align);

public:
	idFbxExport(idExportOptions& exportOptions) : options(exportOptions) {
		sdkManager = NULL;
		scene = NULL;
		animStack = NULL;
		currentTime.SetFrame(0, FbxTime::eFrames24);
	};
	~idFbxExport() {
		if (sdkManager != NULL) {
			sdkManager->Destroy();
			sdkManager = NULL;
			scene = NULL;
			animStack = NULL;
		}
	};
	void ConvertModel(void);
};

void idFbxExport::LoadScene(void) {
	sdkManager = FbxManager::Create();
	if (sdkManager == NULL) {
		MayaError("FBX SDK manager creation failed");
	}

	FbxIOSettings* ios = FbxIOSettings::Create(sdkManager, IOSROOT);
	sdkManager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(sdkManager, "");
	if (importer == NULL) {
		MayaError("FBX importer creation failed");
	}

	if (!importer->Initialize(options.src.c_str(), -1, sdkManager->GetIOSettings())) {
		const char* err = importer->GetStatus().GetErrorString();
		importer->Destroy();
		MayaError("Error opening FBX '%s': '%s'", options.src.c_str(), err ? err : "unknown error");
	}

	scene = FbxScene::Create(sdkManager, "doom3_fbx_scene");
	if (scene == NULL) {
		importer->Destroy();
		MayaError("FBX scene creation failed");
	}

	if (!importer->Import(scene)) {
		const char* err = importer->GetStatus().GetErrorString();
		importer->Destroy();
		MayaError("Error importing FBX '%s': '%s'", options.src.c_str(), err ? err : "unknown error");
	}
	importer->Destroy();

	// Match the old Maya exporter assumptions before applying ConvertToIdSpace().
	FbxAxisSystem::MayaYUp.ConvertScene(scene);
	FbxSystemUnit::cm.ConvertScene(scene);

	FbxGeometryConverter converter(sdkManager);
	converter.Triangulate(scene, true);

	animStack = scene->GetSrcObject<FbxAnimStack>(0);
	if (animStack != NULL) {
		scene->SetCurrentAnimationStack(animStack);
	}
}

FbxTime idFbxExport::TimeForFbxFrame(int num) const {
	FbxTime time;
	time.SetFrame(num, FbxTime::eFrames24);
	return time;
}

float idFbxExport::TimeForFrame(int num) const {
	return (float)TimeForFbxFrame(num).GetSecondDouble();
}

void idFbxExport::SetDefaultFrameRange(void) {
	if (options.startframe < 0 || options.endframe < 0) {
		FbxTime start;
		FbxTime stop;
		start.SetFrame(0, FbxTime::eFrames24);
		stop.SetFrame(0, FbxTime::eFrames24);

		if (animStack != NULL) {
			FbxTimeSpan span = animStack->GetLocalTimeSpan();
			start = span.GetStart();
			stop = span.GetStop();
		}
		else if (scene != NULL && scene->GetRootNode() != NULL) {
			FbxTimeSpan span;
			if (scene->GetRootNode()->GetAnimationInterval(span, animStack)) {
				start = span.GetStart();
				stop = span.GetStop();
			}
		}

		if (options.startframe < 0) {
			options.startframe = (int)start.GetFrameCount(FbxTime::eFrames24);
		}
		if (options.endframe < 0) {
			options.endframe = (int)stop.GetFrameCount(FbxTime::eFrames24);
		}
	}

	if (options.cycleStart < 0) {
		options.cycleStart = options.startframe;
	}
	else if ((options.cycleStart < options.startframe) || (options.cycleStart > options.endframe)) {
		MayaError("cycleStart (%d) out of frame range (%d to %d)\n", options.cycleStart, options.startframe, options.endframe);
	}
	else if (options.cycleStart == options.endframe) {
		options.cycleStart = options.startframe;
	}
}

int idFbxExport::GetFbxFrameNum(int num) const {
	int frameNum;

	if (options.cycleStart > options.startframe) {
		frameNum = options.cycleStart + num;
		if (frameNum > options.endframe) {
			frameNum -= options.endframe - options.startframe;
		}
		if (frameNum < options.startframe) {
			frameNum = options.startframe + 1;
		}
	}
	else {
		frameNum = options.startframe + num;
		if (frameNum > options.endframe) {
			frameNum -= options.endframe + 1 - options.startframe;
		}
		if (frameNum < options.startframe) {
			frameNum = options.startframe;
		}
	}

	return frameNum;
}

void idFbxExport::SetFrame(int num) {
	currentTime = TimeForFbxFrame(GetFbxFrameNum(num));
}

void idFbxExport::AddNodesRecursive(FbxNode* node) {
	int i;
	idExportJoint* joint;

	if (node == NULL) {
		return;
	}

	if (node != scene->GetRootNode()) {
		joint = &model.joints.Alloc();
		joint->index = model.joints.Num() - 1;
		joint->dagnode = node;
		joint->name = FbxSafeName(node->GetName());
		joint->realname = joint->name;
	}

	for (i = 0; i < node->GetChildCount(); i++) {
		AddNodesRecursive(node->GetChild(i));
	}
}

idExportJoint* idFbxExport::FindJointForNode(FbxNode* node) {
	int i;
	if (node == NULL) {
		return NULL;
	}
	for (i = 0; i < model.joints.Num(); i++) {
		if (model.joints[i].dagnode == node) {
			return &model.joints[i];
		}
	}
	return NULL;
}

void idFbxExport::SetJointBindPose(idExportJoint* joint, const FbxAMatrix& matrix, float scale) {
	if (joint == NULL) {
		return;
	}
	joint->bindmat = ConvertToIdSpace(idMat(matrix));
	joint->bindmat.OrthoNormalizeSelf();
	joint->bindpos = ConvertToIdSpace(idVec(matrix.GetT())) * scale;
	joint->scale = 1.0f;
	joint->invscale = 1.0f;
}

void idFbxExport::CreateJoints(float scale) {
	int i, j;
	idExportJoint* joint;
	idExportJoint* parent;

	SetFrame(0);
	AddNodesRecursive(scene->GetRootNode());

	model.exportOrigin = &model.joints.Alloc();
	model.exportOrigin->index = model.joints.Num() - 1;
	model.exportOrigin->dagnode = NULL;

	for (i = 0; i < model.joints.Num(); i++) {
		joint = &model.joints[i];
		if (!joint->dagnode) {
			continue;
		}

		joint->mayaNode.ParentTo(model.mayaHead);
		joint->exportNode.ParentTo(model.exportHead);

		FbxNode* parentNode = joint->dagnode->GetParent();
		while (parentNode != NULL && parentNode != scene->GetRootNode()) {
			for (j = 0; j < model.joints.Num(); j++) {
				if (model.joints[j].dagnode == parentNode) {
					joint->mayaNode.ParentTo(model.joints[j].mayaNode);
					joint->exportNode.ParentTo(model.joints[j].exportNode);
					break;
				}
			}
			if (j < model.joints.Num()) {
				break;
			}
			parentNode = parentNode->GetParent();
		}

		parent = joint->mayaNode.GetParent();
		if (parent) {
			joint->longname = parent->longname + "/" + joint->name;
		}
		else {
			joint->longname = joint->name;
		}

		SetJointBindPose(joint, joint->dagnode->EvaluateGlobalTransform(currentTime), scale);
	}
}

void idFbxExport::GetWorldTransform(idExportJoint* joint, idVec3& pos, idMat3& mat, float scale) {
	pos.Zero();
	mat.Identity();
	if (joint == NULL || joint->dagnode == NULL) {
		return;
	}

	FbxAMatrix global = joint->dagnode->EvaluateGlobalTransform(currentTime, FbxNode::eSourcePivot, false, true);
	pos = idVec(global.GetT()) * scale;
	mat = idMat(global);
	mat.OrthoNormalizeSelf();
}

void idFbxExport::PruneJoints(idStrList& keepjoints, idStr& prefix) {
	int				i;
	int				j;
	idExportMesh* mesh;
	idExportJoint* joint;
	idExportJoint* joint2;
	idExportJoint* parent;
	int				num_weights;

	if (!keepjoints.Num() && !prefix.Length()) {
		if (!model.meshes.Num() || options.ignoreMeshes) {
			joint = model.joints.Ptr();
			for (i = 0; i < model.joints.Num(); i++, joint++) {
				joint->keep = true;
			}
		}
		else {
			for (i = 0; i < model.meshes.Num(); i++) {
				mesh = model.meshes[i];
				for (j = 0; j < mesh->weights.Num(); j++) {
					mesh->weights[j].joint->keep = true;
				}
			}
		}
	}
	else {
		for (i = 0; i < keepjoints.Num(); i++) {
			joint = model.FindJoint(keepjoints[i]);
			if (joint) {
				joint->keep = true;
			}
		}

		for (i = 0; i < model.meshes.Num(); i++) {
			mesh = model.meshes[i];
			num_weights = 0;
			for (j = 0; j < mesh->weights.Num(); j++) {
				if (mesh->weights[j].joint->keep) {
					num_weights++;
				}
				else if (prefix.Length() && !mesh->weights[j].joint->realname.Cmpn(prefix, prefix.Length())) {
					mesh->weights[j].joint->keep = true;
					num_weights++;
				}
			}

			if (num_weights != mesh->weights.Num()) {
				mesh->keep = false;
			}
		}
	}

	model.export_joints = 0;
	joint = model.joints.Ptr();
	for (i = 0; i < model.joints.Num(); i++, joint++) {
		if (!joint->keep) {
			joint->exportNode.RemoveFromHierarchy();
		}
		else {
			joint->index = model.export_joints;
			model.export_joints++;

			for (parent = joint->exportNode.GetParent(); parent != NULL; parent = parent->exportNode.GetParent()) {
				if (parent->keep) {
					break;
				}
			}

			if (parent != NULL) {
				joint->exportNode.ParentTo(parent->exportNode);
			}
			else {
				joint->exportNode.ParentTo(model.exportHead);
			}
		}
	}

	for (joint = model.exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		if (!joint->keep) {
			MayaError("Non-kept joint in export tree ('%s')", joint->name.c_str());
		}

		for (joint2 = model.exportHead.GetNext(); joint2 != NULL; joint2 = joint2->exportNode.GetNext()) {
			if ((joint2 != joint) && (joint2->name == joint->name)) {
				MayaError("Two joints found with the same name ('%s')", joint->name.c_str());
			}
		}
	}
}

void idFbxExport::RenameJoints(idList<idNamePair>& renamejoints, idStr& prefix) {
	int				i;
	idExportJoint* joint;

	if (prefix.Length()) {
		joint = model.joints.Ptr();
		for (i = 0; i < model.joints.Num(); i++, joint++) {
			if (!joint->name.Cmpn(prefix, prefix.Length())) {
				joint->name = joint->name.Right(joint->name.Length() - prefix.Length());
			}
		}
	}

	for (i = 0; i < renamejoints.Num(); i++) {
		joint = model.FindJoint(renamejoints[i].from);
		if (joint) {
			joint->name = renamejoints[i].to;
		}
	}
}

bool idFbxExport::RemapParents(idList<idNamePair>& remapjoints) {
	int				i;
	idExportJoint* joint;
	idExportJoint* parent;
	idExportJoint* origin;
	idExportJoint* sibling;

	for (i = 0; i < remapjoints.Num(); i++) {
		joint = model.FindJoint(remapjoints[i].from);
		if (!joint) {
			MayaError("Couldn't find joint '%s' to reparent\n", remapjoints[i].from.c_str());
		}

		parent = model.FindJoint(remapjoints[i].to);
		if (!parent) {
			MayaError("Couldn't find joint '%s' to be new parent for '%s'\n", remapjoints[i].to.c_str(), remapjoints[i].from.c_str());
		}

		if (parent->exportNode.ParentedBy(joint->exportNode)) {
			MayaError("Joint '%s' is a child of joint '%s' and can't become the parent.", joint->name.c_str(), parent->name.c_str());
		}
		joint->exportNode.ParentTo(parent->exportNode);
	}

	origin = model.FindJoint("origin");
	if (!origin) {
		origin = model.exportOrigin;
		origin->dagnode = NULL;
		origin->name = "origin";
		origin->realname = "origin";
		origin->bindmat.Identity();
		origin->bindpos.Zero();
	}

	origin->exportNode.ParentTo(model.exportHead);
	origin->keep = true;

	joint = model.exportHead.GetChild();
	while (joint) {
		sibling = joint->exportNode.GetSibling();
		if (joint != origin) {
			joint->exportNode.ParentTo(origin->exportNode);
		}
		joint = sibling;
	}

	return true;
}

void idFbxExport::GetTextureForMesh(idExportMesh* mesh, FbxNode* node) {
	int i;
	FbxSurfaceMaterial* material;
	FbxProperty prop;

	mesh->shader = "";
	if (node == NULL) {
		return;
	}

	for (i = 0; i < node->GetMaterialCount(); i++) {
		material = node->GetMaterial(i);
		if (material == NULL) {
			continue;
		}

		prop = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (prop.IsValid()) {
			int textureCount = prop.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount > 0) {
				FbxFileTexture* texture = prop.GetSrcObject<FbxFileTexture>(0);
				if (texture != NULL && texture->GetFileName() != NULL && texture->GetFileName()[0]) {
					OSPathToRelativePath(texture->GetFileName(), mesh->shader, options.game);
					mesh->shader.StripFileExtension();
					return;
				}
			}
		}

		if (material->GetName() && material->GetName()[0]) {
			mesh->shader = material->GetName();
			return;
		}
	}
}

idExportMesh* idFbxExport::CopyMesh(FbxNode* node, FbxMesh* fbxMesh, float scale) {
	int i, j, k;
	int cpCount;
	int polygonCount;
	idStr name, altname;
	idExportMesh* mesh;
	bool allowUnskinned;
	bool hasSkin;
	FbxAMatrix meshBindMatrix;
	FbxAMatrix geometryMatrix;
	FbxStringList uvSets;
	const char* uvSetName;

	if (node == NULL || fbxMesh == NULL) {
		return NULL;
	}

	name = FbxSafeName(fbxMesh->GetName());
	altname = FbxSafeName(node->GetName());
	if (!name.Length() || name == "unnamed") {
		name = altname;
	}

	name.StripLeadingOnce(options.prefix);
	altname.StripLeadingOnce(options.prefix);
	if (options.keepmeshes.Num()) {
		if (!options.keepmeshes.Find(name) && !options.keepmeshes.Find(altname)) {
			if (altname != name) {
				common->Printf("Skipping mesh '%s' ('%s')\n", name.c_str(), altname.c_str());
			}
			else {
				common->Printf("Skipping mesh '%s'\n", name.c_str());
			}
			return NULL;
		}
	}

	if (options.skipmeshes.Find(name) || options.skipmeshes.Find(altname)) {
		common->Printf("Skipping mesh '%s' ('%s')\n", name.c_str(), altname.c_str());
		return NULL;
	}

	allowUnskinned = IsMD3Export(options);
	hasSkin = fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
	if (!hasSkin && !allowUnskinned) {
		MayaError("Mesh '%s': No skin deformers found.  MD5 export expects a skinned FBX mesh.", altname.c_str());
	}

	mesh = new idExportMesh();
	model.meshes.Append(mesh);
	mesh->name = altname.Length() ? altname : name;
	GetTextureForMesh(mesh, node);

	cpCount = fbxMesh->GetControlPointsCount();
	mesh->verts.SetNum(cpCount);
	geometryMatrix = FbxGetGeometryTransform(node);
	meshBindMatrix = node->EvaluateGlobalTransform(currentTime) * geometryMatrix;

	for (i = 0; i < cpCount; i++) {
		memset(&mesh->verts[i], 0, sizeof(mesh->verts[i]));
		FbxVector4 p = meshBindMatrix.MultT(fbxMesh->GetControlPointAt(i));
		mesh->verts[i].pos = ConvertToIdSpace(idVec(p)) * scale;
	}

	polygonCount = fbxMesh->GetPolygonCount();
	mesh->tris.SetNum(polygonCount);
	mesh->uv.SetNum(polygonCount);

	fbxMesh->GetUVSetNames(uvSets);
	uvSetName = (uvSets.GetCount() > 0) ? uvSets[0] : NULL;
	for (i = 0; i < polygonCount; i++) {
		if (fbxMesh->GetPolygonSize(i) != 3) {
			MayaError("CopyMesh: FBX triangulation failed; polygon %d on '%s' has %d vertices", i, mesh->name.c_str(), fbxMesh->GetPolygonSize(i));
		}
		for (j = 0; j < 3; j++) {
			mesh->tris[i].indexes[j] = fbxMesh->GetPolygonVertex(i, j);
			mesh->uv[i].uv[j][0] = 0.0f;
			mesh->uv[i].uv[j][1] = 0.0f;
			if (uvSetName != NULL) {
				FbxVector2 uv;
				bool unmapped = false;
				if (fbxMesh->GetPolygonVertexUV(i, j, uvSetName, uv, unmapped) && !unmapped) {
					mesh->uv[i].uv[j][0] = (float)uv[0];
					mesh->uv[i].uv[j][1] = (float)uv[1];
				}
			}
		}
	}

	idList< idList<exportWeight_t> > cpWeights;
	cpWeights.SetNum(cpCount);

	if (hasSkin) {
		bool meshBindSet = false;
		for (i = 0; i < fbxMesh->GetDeformerCount(FbxDeformer::eSkin); i++) {
			FbxSkin* skin = (FbxSkin*)fbxMesh->GetDeformer(i, FbxDeformer::eSkin);
			if (skin == NULL) {
				continue;
			}

			for (j = 0; j < skin->GetClusterCount(); j++) {
				FbxCluster* cluster = skin->GetCluster(j);
				if (cluster == NULL || cluster->GetLink() == NULL) {
					continue;
				}

				idExportJoint* joint = FindJointForNode(cluster->GetLink());
				if (joint == NULL) {
					MayaError("Mesh '%s': joint '%s' not found", mesh->name.c_str(), FbxSafeName(cluster->GetLink()->GetName()));
				}

				FbxAMatrix linkMatrix;
				cluster->GetTransformLinkMatrix(linkMatrix);
				SetJointBindPose(joint, linkMatrix, scale);

				if (!meshBindSet) {
					cluster->GetTransformMatrix(meshBindMatrix);
					meshBindMatrix = meshBindMatrix * geometryMatrix;
					for (k = 0; k < cpCount; k++) {
						FbxVector4 p = meshBindMatrix.MultT(fbxMesh->GetControlPointAt(k));
						mesh->verts[k].pos = ConvertToIdSpace(idVec(p)) * scale;
					}
					meshBindSet = true;
				}

				int* indices = cluster->GetControlPointIndices();
				double* weights = cluster->GetControlPointWeights();
				int weightCount = cluster->GetControlPointIndicesCount();
				for (k = 0; k < weightCount; k++) {
					int cpIndex = indices[k];
					if (cpIndex < 0 || cpIndex >= cpCount) {
						continue;
					}
					if (weights[k] <= 0.0) {
						continue;
					}
					exportWeight_t weight;
					weight.joint = joint;
					weight.jointWeight = (float)weights[k];
					weight.offset.Zero();
					cpWeights[cpIndex].Append(weight);
				}
			}
		}
	}
	else {
		idExportJoint* joint = FindJointForNode(node);
		if (joint == NULL) {
			joint = model.exportOrigin;
			joint->name = "origin";
			joint->realname = "origin";
			joint->bindmat.Identity();
			joint->bindpos.Zero();
			joint->keep = true;
		}
		else {
			SetJointBindPose(joint, node->EvaluateGlobalTransform(currentTime), scale);
		}

		for (i = 0; i < cpCount; i++) {
			exportWeight_t weight;
			weight.joint = joint;
			weight.jointWeight = 1.0f;
			weight.offset.Zero();
			cpWeights[i].Append(weight);
		}
	}

	for (i = 0; i < cpCount; i++) {
		exportVertex_t* vert = &mesh->verts[i];
		vert->startweight = mesh->weights.Num();
		float totalWeight = 0.0f;
		int nonZeroWeights = 0;

		for (j = 0; j < cpWeights[i].Num(); j++) {
			if (cpWeights[i][j].jointWeight > 0.0f) {
				nonZeroWeights++;
			}
			if (cpWeights[i][j].jointWeight <= options.jointThreshold) {
				continue;
			}

			exportWeight_t weight = cpWeights[i][j];
			weight.joint->bindmat.ProjectVector(vert->pos - weight.joint->bindpos, weight.offset);
			mesh->weights.Append(weight);
			totalWeight += weight.jointWeight;
		}

		vert->numWeights = mesh->weights.Num() - vert->startweight;
		if (!vert->numWeights) {
			if (nonZeroWeights) {
				MayaError("Error on mesh '%s': Vertex %d doesn't have any joint weights exceeding jointThreshold (%f).", mesh->name.c_str(), i, options.jointThreshold);
			}
			else {
				MayaError("Error on mesh '%s': Vertex %d doesn't have any joint weights.", mesh->name.c_str(), i);
			}
		}
		else if (!totalWeight) {
			MayaError("Error on mesh '%s': Combined weight of 0 on vertex %d.", mesh->name.c_str(), i);
		}

		for (j = 0; j < vert->numWeights; j++) {
			mesh->weights[vert->startweight + j].jointWeight /= totalWeight;
		}
	}

	return mesh;
}

void idFbxExport::CreateMeshRecursive(FbxNode* node, int& count) {
	if (node == NULL) {
		return;
	}

	FbxMesh* mesh = node->GetMesh();
	if (mesh != NULL) {
		if (CopyMesh(node, mesh, options.scale) != NULL) {
			count++;
		}
	}

	for (int i = 0; i < node->GetChildCount(); i++) {
		CreateMeshRecursive(node->GetChild(i), count);
	}
}

void idFbxExport::CreateMesh(float scale) {
	int count = 0;
	CreateMeshRecursive(scene->GetRootNode(), count);
	if (!count && !options.ignoreMeshes) {
		MayaError("CreateMesh: No FBX meshes found in this scene.\n");
	}
}

void idFbxExport::CombineMeshes(void) {
	int						i, j;
	int						count;
	idExportMesh* mesh;
	idExportMesh* combine;
	idList<idExportMesh*>	oldmeshes;

	oldmeshes = model.meshes;
	model.meshes.Clear();

	count = 0;
	for (i = 0; i < oldmeshes.Num(); i++) {
		mesh = oldmeshes[i];
		if (!mesh->keep) {
			delete mesh;
			continue;
		}

		combine = NULL;
		for (j = 0; j < model.meshes.Num(); j++) {
			if (model.meshes[j]->shader == mesh->shader) {
				combine = model.meshes[j];
				break;
			}
		}

		if (combine) {
			combine->Merge(mesh);
			delete mesh;
			count++;
		}
		else {
			model.meshes.Append(mesh);
		}
	}

	for (i = 0; i < model.meshes.Num(); i++) {
		model.meshes[i]->ShareVerts();
	}

	common->Printf("Merged %d meshes\n", count);
}

void idFbxExport::GetAlignment(idStr& alignName, idMat3& align, float rotate, int startframe) {
	idVec3			pos;
	idExportJoint* joint;
	idAngles		ang(0, rotate, 0);
	idMat3			mat;

	align.Identity();

	if (alignName.Length()) {
		SetFrame(0);

		joint = model.FindJoint(alignName);
		if (!joint) {
			MayaError("could not find joint '%s' to align model to.\n", alignName.c_str());
		}

		GetWorldTransform(joint, pos, mat, 1.0f);
		align[0][0] = mat[2][0];
		align[0][1] = -mat[2][2];
		align[0][2] = mat[2][1];

		align[1][0] = mat[0][0];
		align[1][1] = -mat[0][2];
		align[1][2] = mat[0][1];

		align[2][0] = mat[1][0];
		align[2][1] = -mat[1][2];
		align[2][2] = mat[1][1];

		if (rotate) {
			align *= ang.ToMat3();
		}
	}
	else if (rotate) {
		align = ang.ToMat3();
	}

	align.TransposeSelf();
}

idExportJoint* idFbxExport::FindFirstCamera(void) {
	int i;
	idExportJoint* joint;

	joint = model.FindJoint(MAYA_DEFAULT_CAMERA);
	if (joint && joint->dagnode && joint->dagnode->GetCamera()) {
		return joint;
	}

	for (i = 0; i < model.joints.Num(); i++) {
		joint = &model.joints[i];
		if (joint->dagnode && joint->dagnode->GetCamera()) {
			return joint;
		}
	}

	return NULL;
}

float idFbxExport::GetCameraFov(idExportJoint* joint) {
	if (joint == NULL || joint->dagnode == NULL || joint->dagnode->GetCamera() == NULL) {
		return 90.0f;
	}

	FbxCamera* camera = joint->dagnode->GetCamera();
	if (camera->GetApertureMode() == FbxCamera::eFocalLength) {
		return (float)camera->ComputeFieldOfView(camera->FocalLength.Get());
	}
	if (camera->GetApertureMode() == FbxCamera::eHorizAndVert) {
		return (float)camera->FieldOfViewX.Get();
	}
	return (float)camera->FieldOfView.Get();
}

void idFbxExport::GetCameraFrame(idExportJoint* camera, idMat3& align, cameraFrame_t* cam) {
	idMat3 mat;
	idMat3 axis;
	idVec3 pos;

	GetWorldTransform(camera, pos, axis, 1.0f);

	cam->t = ConvertToIdSpace(pos) * align;

	axis = ConvertToIdSpace(axis) * align;
	mat[0] = -axis[2];
	mat[1] = -axis[0];
	mat[2] = axis[1];
	cam->q = mat.ToQuat().ToCQuat();
	cam->fov = GetCameraFov(camera);
}

void idFbxExport::CreateCameraAnim(idMat3& align) {
	float			start, end;
	int				frameNum;
	idExportJoint* camJoint;

	start = TimeForFrame(options.startframe);
	end = TimeForFrame(options.endframe);

	model.numFrames = options.endframe + 1 - options.startframe;
	model.frameRate = options.framerate;

	common->Printf("start frame = %d\n  end frame = %d\n", options.startframe, options.endframe);
	common->Printf(" start time = %f\n   end time = %f\n total time = %f\n", start, end, end - start);

	if (start > end) {
		MayaError("Start frame is greater than end frame.");
	}

	model.camera.Clear();
	model.cameraCuts.Clear();
	camJoint = FindFirstCamera();
	if (camJoint == NULL) {
		MayaError("Couldn't find an FBX camera node");
	}

	for (frameNum = 0; frameNum < model.numFrames; frameNum++) {
		common->Printf("\rFrame %d/%d...", options.startframe + frameNum, options.endframe);
		SetFrame(frameNum);
		GetCameraFrame(camJoint, align, &model.camera.Alloc());
	}

	common->Printf("\n");
}

void idFbxExport::GetDefaultPose(idMat3& align) {
	float			start;
	idMat3			jointaxis;
	idVec3			jointpos;
	idExportJoint* joint, * parent;
	idList<jointFrame_t> frame;

	start = TimeForFrame(options.startframe);

	common->Printf("default pose frame = %d\n", options.startframe);
	common->Printf(" default pose time = %f\n", start);

	frame.SetNum(model.joints.Num());
	SetFrame(0);

	for (joint = model.exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		if (!joint->dagnode) {
			joint->idwm.Identity();
			joint->idt.Zero();
			frame[joint->index].t.Zero();
			frame[joint->index].q.Set(0.0f, 0.0f, 0.0f);
			continue;
		}

		GetWorldTransform(joint, jointpos, jointaxis, options.scale);

		jointaxis = ConvertToIdSpace(jointaxis) * align;
		jointpos = ConvertToIdSpace(jointpos) * align;

		joint->idwm = jointaxis;
		joint->idt = jointpos;

		parent = joint->exportNode.GetParent();
		if (parent) {
			jointpos = (jointpos - parent->idt) * parent->idwm.Transpose();
			jointaxis = jointaxis * parent->idwm.Transpose();
		}
		else if (joint->name == "origin") {
			if (options.clearOrigin) {
				jointpos.Zero();
			}
			if (options.clearOriginAxis) {
				jointaxis.Identity();
			}
		}

		frame[joint->index].t = jointpos;
		frame[joint->index].q = jointaxis.ToQuat().ToCQuat();
	}

	joint = model.FindJoint("origin");
	if (joint) {
		frame[joint->index].t.Zero();
	}

	for (joint = model.exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
		jointpos = frame[joint->index].t;
		jointaxis = frame[joint->index].q.ToQuat().ToMat3();

		parent = joint->exportNode.GetParent();
		if (parent) {
			joint->idwm = jointaxis * parent->idwm;
			joint->idt = parent->idt + jointpos * parent->idwm;
		}
		else {
			joint->idwm = jointaxis;
			joint->idt = jointpos;
		}

		joint->bindmat = joint->idwm;
		joint->bindpos = joint->idt;
	}

	common->Printf("\n");
}

void idFbxExport::CreateAnimation(idMat3& align) {
	int				i;
	float			start, end;
	idMat3			jointaxis;
	idVec3			jointpos;
	int				frameNum;
	idExportJoint* joint, * parent;
	idBounds		bnds;
	idBounds		meshBounds;
	jointFrame_t* frame;
	int				cycleStart;
	idVec3			totalDelta;
	idList<jointFrame_t> copyFrames;

	start = TimeForFrame(options.startframe);
	end = TimeForFrame(options.endframe);

	model.numFrames = options.endframe + 1 - options.startframe;
	model.frameRate = options.framerate;

	common->Printf("start frame = %d\n  end frame = %d\n", options.startframe, options.endframe);
	common->Printf(" start time = %f\n   end time = %f\n total time = %f\n", start, end, end - start);

	if (start > end) {
		MayaError("Start frame is greater than end frame.");
	}

	model.bounds.SetNum(model.numFrames);
	model.jointFrames.SetNum(model.numFrames * model.joints.Num());
	model.frames.SetNum(model.numFrames);
	for (i = 0; i < model.numFrames; i++) {
		model.frames[i] = &model.jointFrames[model.joints.Num() * i];
	}

	cycleStart = options.cycleStart;
	options.cycleStart = options.startframe;

	for (frameNum = 0; frameNum < model.numFrames; frameNum++) {
		common->Printf("\rFrame %d/%d...", options.startframe + frameNum, options.endframe);

		frame = model.frames[frameNum];
		SetFrame(frameNum);

		for (joint = model.exportHead.GetNext(); joint != NULL; joint = joint->exportNode.GetNext()) {
			if (!joint->dagnode) {
				joint->idwm.Identity();
				joint->idt.Zero();
				frame[joint->index].t.Zero();
				frame[joint->index].q.Set(0.0f, 0.0f, 0.0f);
				continue;
			}

			GetWorldTransform(joint, jointpos, jointaxis, options.scale);
			jointaxis = ConvertToIdSpace(jointaxis) * align;
			jointpos = ConvertToIdSpace(jointpos) * align;

			joint->idwm = jointaxis;
			joint->idt = jointpos;

			parent = joint->exportNode.GetParent();
			if (parent) {
				jointpos = (jointpos - parent->idt) * parent->idwm.Transpose();
				jointaxis = jointaxis * parent->idwm.Transpose();
			}
			else if (joint->name == "origin") {
				if (options.clearOrigin) {
					jointpos.Zero();
				}
				if (options.clearOriginAxis) {
					jointaxis.Identity();
				}
			}

			frame[joint->index].t = jointpos;
			frame[joint->index].q = jointaxis.ToQuat().ToCQuat();
		}
	}

	options.cycleStart = cycleStart;
	totalDelta.Zero();

	joint = model.FindJoint("origin");
	if (joint) {
		frame = model.frames[0];
		idVec3 origin = frame[joint->index].t;

		frame = model.frames[model.numFrames - 1];
		totalDelta = frame[joint->index].t - origin;
	}

	if (options.cycleStart > options.startframe) {
		copyFrames = model.jointFrames;
		for (i = 0; i < model.numFrames; i++) {
			bool shiftorigin = false;
			frameNum = i + (options.cycleStart - options.startframe);
			if (frameNum >= model.numFrames) {
				frameNum -= model.numFrames - 1;
				shiftorigin = true;
			}

			memcpy(&model.jointFrames[model.joints.Num() * i], &copyFrames[model.joints.Num() * frameNum], model.joints.Num() * sizeof(copyFrames[0]));

			if (joint && shiftorigin) {
				model.frames[i][joint->index].t += totalDelta;
			}
		}
	}

	if (joint) {
		frame = model.frames[0];
		idVec3 origin = frame[joint->index].t;
		for (i = 0; i < model.numFrames; i++) {
			frame = model.frames[i];
			frame[joint->index].t -= origin;
		}
	}

	for (frameNum = 0; frameNum < model.numFrames; frameNum++) {
		frame = model.frames[frameNum];
		MD3_BuildWorldPose(model, frame);

		bnds.Clear();
		for (i = 0; i < model.meshes.Num(); i++) {
			if (model.meshes[i]->keep) {
				model.meshes[i]->GetBounds(meshBounds);
				bnds.AddBounds(meshBounds);
			}
		}
		model.bounds[frameNum][0] = bnds[0];
		model.bounds[frameNum][1] = bnds[1];
	}

	common->Printf("\n");
}

void idFbxExport::ConvertModel(void) {
	idMat3 align;

	common->Printf("Converting %s to %s...\n", options.src.c_str(), options.dest.c_str());

	FILE* file = fopen(options.dest, "r");
	if (file) {
		fclose(file);
		FILE* file = fopen(options.dest, "r+");
		if (!file) {
			MayaError("Unable to write to the file '%s'", options.dest.c_str());
		}
		fclose(file);
	}

	common->Printf("Loading FBX file...\n");
	LoadScene();
	SetDefaultFrameRange();

	common->Printf("Creating joints...\n");
	CreateJoints(options.scale);

	if (options.type != WRITE_CAMERA) {
		common->Printf("Creating meshes...\n");
		CreateMesh(options.scale);
		common->Printf("Renaming joints...\n");
		RenameJoints(options.renamejoints, options.prefix);
		common->Printf("Remapping parents...\n");
		RemapParents(options.remapjoints);
		common->Printf("Pruning joints...\n");
		PruneJoints(options.keepjoints, options.prefix);
		common->Printf("Combining meshes...\n");
		CombineMeshes();
	}

	common->Printf("Align model...\n");
	GetAlignment(options.align, align, options.rotate, 0);

	if (IsMD3Export(options)) {
		common->Printf("Creating MD3 vertex frames:\n");
		CreateAnimation(align);
		common->Printf("Writing MD3 file...\n");
		if (!WriteMD3Model(model, options.dest, options)) {
			MayaError("error writing to '%s'", options.dest.c_str());
		}
		common->Printf("done\n\n");
		return;
	}

	switch (options.type) {
	case WRITE_MESH:
		common->Printf("Grabbing default pose:\n");
		GetDefaultPose(align);
		common->Printf("Writing file...\n");
		if (!model.WriteMesh(options.dest, options)) {
			MayaError("error writing to '%s'", options.dest.c_str());
		}
		break;

	case WRITE_ANIM:
		common->Printf("Creating animation frames:\n");
		CreateAnimation(align);
		common->Printf("Writing file...\n");
		if (!model.WriteAnim(options.dest, options)) {
			MayaError("error writing to '%s'", options.dest.c_str());
		}
		break;

	case WRITE_CAMERA:
		common->Printf("Creating camera frames:\n");
		CreateCameraAnim(align);
		common->Printf("Writing file...\n");
		if (!model.WriteCamera(options.dest, options)) {
			MayaError("error writing to '%s'", options.dest.c_str());
		}
		break;
	}

	common->Printf("done\n\n");
}

/*
==============================================================================

dll setup

==============================================================================
*/

/*
===============
Maya_Shutdown
===============
*/
void Maya_Shutdown(void) {
	if (initialized) {
		errorMessage.Clear();
		initialized = false;

		// FbxManager instances are owned per conversion and destroyed by idFbxExport.
	}
}

/*
===============
Maya_ConvertModel
===============
*/
const char* Maya_ConvertModel(const char* ospath, const char* commandline) {

	errorMessage = "Ok";

	try {
		idExportOptions options(commandline, ospath);
		idFbxExport	exportM(options);

		exportM.ConvertModel();
	}

	catch (idException& exception) {
		errorMessage = exception.error;
	}

	return errorMessage;
}

/*
===============
dllEntry
===============
*/
bool dllEntry(int version, idCommon* common, idSys* sys) {

	if (!common || !sys) {
		return false;
	}

	::common = common;
	::sys = sys;
	::cvarSystem = NULL;

	idLib::sys = sys;
	idLib::common = common;
	idLib::cvarSystem = NULL;
	idLib::fileSystem = NULL;

	idLib::Init();

	if (version != MD5_VERSION) {
		common->Printf("Error initializing FBX exporter: DLL version %d different from .exe version %d\n", MD5_VERSION, version);
		return false;
	}

	initialized = true;

	return true;
}

// Force type checking on the interface functions to help ensure that they match the ones in the .exe
const exporterDLLEntry_t	ValidateEntry = &dllEntry;
const exporterInterface_t	ValidateConvert = &Maya_ConvertModel;
const exporterShutdown_t	ValidateShutdown = &Maya_Shutdown;
