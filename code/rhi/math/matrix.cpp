// matrix.cpp
//


#include "precompiled.h"
#include "../../renderer/tr_local.h"

void create_brush_matrix(float matrix[16], renderEntity_t* e)
{
	idVec3 origin;
	origin[0] = e->origin[0]; // (1.f - e->backlerp)* e->origin[0] + e->backlerp * e->oldorigin[0];
	origin[1] = e->origin[1]; // (1.f - e->backlerp)* e->origin[1] + e->backlerp * e->oldorigin[1];
	origin[2] = e->origin[2]; // (1.f - e->backlerp)* e->origin[2] + e->backlerp * e->oldorigin[2];

	matrix[0] = e->axis[0][0];
	matrix[4] = e->axis[1][0];
	matrix[8] = e->axis[2][0];
	matrix[12] = origin[0];

	matrix[1] = e->axis[0][1];
	matrix[5] = e->axis[1][1];
	matrix[9] = e->axis[2][1];
	matrix[13] = origin[1];

	matrix[2]  = e->axis[0][2];
	matrix[6]  = e->axis[1][2];
	matrix[10] = e->axis[2][2];
	matrix[14] = origin[2];

	matrix[3] = 0.0f;
	matrix[7] = 0.0f;
	matrix[11] = 0.0f;
	matrix[15] = 1.0f;
}


void create_entity_matrix(float matrix[16], renderEntity_t* e)
{
	//vec3_t axis[3];
	idVec3 origin;
	origin[0] = e->origin[0]; // (1.f - e->backlerp)* e->origin[0] + e->backlerp * e->oldorigin[0];
	origin[1] = e->origin[1]; // (1.f - e->backlerp)* e->origin[1] + e->backlerp * e->oldorigin[1];
	origin[2] = e->origin[2]; // (1.f - e->backlerp)* e->origin[2] + e->backlerp * e->oldorigin[2];

	//AnglesToAxis(e->axis, axis);
	//
	//aliashdr_t *paliashdr = (aliashdr_t*)Mod_Extradata(e->model);

	float entScale = 1.0f; // (e->scale > 0.f) ? e->scale : 1.f;
	idVec3 scales;
	idVec3 translate;
	float xyfact, zfact;

	scales[0] = 1.0f;
	scales[1] = 1.0f;
	scales[2] = 1.0f;
	translate[0] = origin[0];
	translate[1] = origin[1];
	translate[2] = origin[2];

	idMat4 _matrix;
	float* _m = (float *)&_matrix;

	_m[0] = e->axis[0][0];
	_m[4] = e->axis[1][0];
	_m[8] = e->axis[2][0];
	_m[12] = origin[0];

	_m[1] = e->axis[0][1];
	_m[5] = e->axis[1][1];
	_m[9] = e->axis[2][1];
	_m[13] = origin[1];

	_m[2] = e->axis[0][2];
	_m[6] = e->axis[1][2];
	_m[10] = e->axis[2][2];
	_m[14] = origin[2];

	_m[3] = 0.0f;
	_m[7] = 0.0f;
	_m[11] = 0.0f;
	_m[15] = 1.0f;

	//_matrix = _matrix * float4x4Translation(translate[0], translate[1], translate[2]);
	//_matrix = _matrix * float4x4Scale(scales[0], scales[1], scales[2]);

	memcpy(matrix, _m, sizeof(float) * 16);
}


void create_projection_matrix(float matrix[16], float znear, float zfar, float fov_x, float fov_y)
{
	float xmin, xmax, ymin, ymax;
	float width, height, depth;

	ymax = znear * tan(fov_y * idMath::PI / 360.0);
	ymin = -ymax;

	xmax = znear * tan(fov_x * idMath::PI / 360.0);
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zfar - znear;

	matrix[0] = 2 * znear / width;
	matrix[4] = 0;
	matrix[8] = (xmax + xmin) / width;
	matrix[12] = 0;

	matrix[1] = 0;
	matrix[5] = -2 * znear / height;
	matrix[9] = (ymax + ymin) / height;
	matrix[13] = 0;

	matrix[2] = 0;
	matrix[6] = 0;
	matrix[10] = (zfar + znear) / depth;
	matrix[14] = 2 * zfar * znear / depth;

	matrix[3] = 0;
	matrix[7] = 0;
	matrix[11] = 1;
	matrix[15] = 0;
}


void create_orthographic_matrix(float matrix[16], float xmin, float xmax, float ymin, float ymax, float znear, float zfar)
{
	float width, height, depth;

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zfar - znear;

	matrix[0] = 2 / width;
	matrix[4] = 0;
	matrix[8] = 0;
	matrix[12] = -(xmax + xmin) / width;

	matrix[1] = 0;
	matrix[5] = 2 / height;
	matrix[9] = 0;
	matrix[13] = -(ymax + ymin) / height;

	matrix[2] = 0;
	matrix[6] = 0;
	matrix[10] = 1 / depth;
	matrix[14] = -znear / depth;

	matrix[3] = 0;
	matrix[7] = 0;
	matrix[11] = 0;
	matrix[15] = 1;
}

void create_view_matrix(float* matrix, float* vieworg, idMat3 viewaxis)
{
	matrix[0] = -viewaxis[1][0];
	matrix[4] = -viewaxis[1][1];
	matrix[8] = -viewaxis[1][2];
	matrix[12] = DotProduct(viewaxis[1], vieworg);

	matrix[1] = viewaxis[2][0];
	matrix[5] = viewaxis[2][1];
	matrix[9] = viewaxis[2][2];
	matrix[13] = -DotProduct(viewaxis[2], vieworg);

	matrix[2] = viewaxis[0][0];
	matrix[6] = viewaxis[0][1];
	matrix[10] = viewaxis[0][2];
	matrix[14] = -DotProduct(viewaxis[0], vieworg);

	matrix[3] = 0;
	matrix[7] = 0;
	matrix[11] = 0;
	matrix[15] = 1;
}

void inverse(const float* m, float* inv)
{
	inv[0] = m[5] * m[10] * m[15] -
		m[5] * m[11] * m[14] -
		m[9] * m[6] * m[15] +
		m[9] * m[7] * m[14] +
		m[13] * m[6] * m[11] -
		m[13] * m[7] * m[10];

	inv[1] = -m[1] * m[10] * m[15] +
		m[1] * m[11] * m[14] +
		m[9] * m[2] * m[15] -
		m[9] * m[3] * m[14] -
		m[13] * m[2] * m[11] +
		m[13] * m[3] * m[10];

	inv[2] = m[1] * m[6] * m[15] -
		m[1] * m[7] * m[14] -
		m[5] * m[2] * m[15] +
		m[5] * m[3] * m[14] +
		m[13] * m[2] * m[7] -
		m[13] * m[3] * m[6];

	inv[3] = -m[1] * m[6] * m[11] +
		m[1] * m[7] * m[10] +
		m[5] * m[2] * m[11] -
		m[5] * m[3] * m[10] -
		m[9] * m[2] * m[7] +
		m[9] * m[3] * m[6];

	inv[4] = -m[4] * m[10] * m[15] +
		m[4] * m[11] * m[14] +
		m[8] * m[6] * m[15] -
		m[8] * m[7] * m[14] -
		m[12] * m[6] * m[11] +
		m[12] * m[7] * m[10];

	inv[5] = m[0] * m[10] * m[15] -
		m[0] * m[11] * m[14] -
		m[8] * m[2] * m[15] +
		m[8] * m[3] * m[14] +
		m[12] * m[2] * m[11] -
		m[12] * m[3] * m[10];

	inv[6] = -m[0] * m[6] * m[15] +
		m[0] * m[7] * m[14] +
		m[4] * m[2] * m[15] -
		m[4] * m[3] * m[14] -
		m[12] * m[2] * m[7] +
		m[12] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] -
		m[0] * m[7] * m[10] -
		m[4] * m[2] * m[11] +
		m[4] * m[3] * m[10] +
		m[8] * m[2] * m[7] -
		m[8] * m[3] * m[6];

	inv[8] = m[4] * m[9] * m[15] -
		m[4] * m[11] * m[13] -
		m[8] * m[5] * m[15] +
		m[8] * m[7] * m[13] +
		m[12] * m[5] * m[11] -
		m[12] * m[7] * m[9];

	inv[9] = -m[0] * m[9] * m[15] +
		m[0] * m[11] * m[13] +
		m[8] * m[1] * m[15] -
		m[8] * m[3] * m[13] -
		m[12] * m[1] * m[11] +
		m[12] * m[3] * m[9];

	inv[10] = m[0] * m[5] * m[15] -
		m[0] * m[7] * m[13] -
		m[4] * m[1] * m[15] +
		m[4] * m[3] * m[13] +
		m[12] * m[1] * m[7] -
		m[12] * m[3] * m[5];

	inv[11] = -m[0] * m[5] * m[11] +
		m[0] * m[7] * m[9] +
		m[4] * m[1] * m[11] -
		m[4] * m[3] * m[9] -
		m[8] * m[1] * m[7] +
		m[8] * m[3] * m[5];

	inv[12] = -m[4] * m[9] * m[14] +
		m[4] * m[10] * m[13] +
		m[8] * m[5] * m[14] -
		m[8] * m[6] * m[13] -
		m[12] * m[5] * m[10] +
		m[12] * m[6] * m[9];

	inv[13] = m[0] * m[9] * m[14] -
		m[0] * m[10] * m[13] -
		m[8] * m[1] * m[14] +
		m[8] * m[2] * m[13] +
		m[12] * m[1] * m[10] -
		m[12] * m[2] * m[9];

	inv[14] = -m[0] * m[5] * m[14] +
		m[0] * m[6] * m[13] +
		m[4] * m[1] * m[14] -
		m[4] * m[2] * m[13] -
		m[12] * m[1] * m[6] +
		m[12] * m[2] * m[5];

	inv[15] = m[0] * m[5] * m[10] -
		m[0] * m[6] * m[9] -
		m[4] * m[1] * m[10] +
		m[4] * m[2] * m[9] +
		m[8] * m[1] * m[6] -
		m[8] * m[2] * m[5];

	float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	det = 1.0f / det;

	for (int i = 0; i < 16; i++)
		inv[i] = inv[i] * det;
}

void mult_matrix_matrix(float* p, const float* a, const float* b)
{
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			p[i * 4 + j] =
				a[0 * 4 + j] * b[i * 4 + 0] +
				a[1 * 4 + j] * b[i * 4 + 1] +
				a[2 * 4 + j] * b[i * 4 + 2] +
				a[3 * 4 + j] * b[i * 4 + 3];
		}
	}
}

void mult_matrix_vector(float* p, const float* a, const float* b)
{
	int j;
	for (j = 0; j < 4; j++) {
		p[j] =
			a[0 * 4 + j] * b[0] +
			a[1 * 4 + j] * b[1] +
			a[2 * 4 + j] * b[2] +
			a[3 * 4 + j] * b[3];
	}
}

