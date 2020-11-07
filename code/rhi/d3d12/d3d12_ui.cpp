// d3d12_ui.cpp
//

#include "precompiled.h"
#include "d3d12_local.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include <vector>

#define MAX_LOADED_TEXTURES			10000
#define MAX_UI_PASSES				500

idCVar r_uiVertexBufferSize("r_uiVertexBufferSize", "10000", CVAR_INTEGER | CVAR_ROM, "size of the vertex buffer for the UI");

struct glTexture_t {
	int width;
	int height;
	tr_texture* texture;

	glTexture_t() {
		width = -1;
		height = -1;
		texture = NULL;
	}
};

glTexture_t textures[MAX_LOADED_TEXTURES];
float canvas_x = 0;
float canvas_y = 0;

tr_shader_program* simple_ui_shader;
tr_buffer* simple_ui_vertex_buffer;

//
// uiVertex_t
//
struct uiVertex_t {
	idVec4 xyz;
	idVec2 st;
};

uiVertex_t*				uiVertexPool;
int currentUIVertex = 0;

struct uiRenderPass_t {
	int startVertex;
	int numVertexes;
	tr_texture* texture;
	tr_descriptor_set* descriptorset;
	tr_pipeline* pipeline;
};

idList<uiRenderPass_t>  uiRenderPasses;

tr_sampler* m_sampler;

tr_pipeline* simple_ui_pipeline[MAX_UI_PASSES];
tr_descriptor_set* simple_ui_desc_set[MAX_UI_PASSES];

tr_buffer* ui_uniform_buffer;

/*
==============
GL_InitUI
==============
*/
void GL_InitUI(void) {
	byte* shaderFileBuffer;
	int shaderFileLen;

	currentUIVertex = 0;

	// Load the UI shader.
	shaderFileLen = fileSystem->ReadFile("shaders/ui/simple_ui.hlsl", (void **)&shaderFileBuffer);
	if (shaderFileLen <= 0 || shaderFileBuffer == NULL) {
		common->FatalError("Failed to load UI shader!\n");
		return;
	}

	// Compile the UI shader.
	tr_create_shader_program(renderer, shaderFileLen, shaderFileBuffer, "VSMain", shaderFileLen, shaderFileBuffer, "PSMain", &simple_ui_shader);
	if (simple_ui_shader == NULL) {
		common->FatalError("Failed to compile the UI shader!\n");
		return;
	}

	tr_create_sampler(renderer, &m_sampler);

	// Create our descriptor set.
	tr_descriptor descriptors[3];
	memset(descriptors, 0, sizeof(descriptors));
	descriptors[0].type = tr_descriptor_type_uniform_buffer_cbv;
	descriptors[0].count = 1;
	descriptors[0].binding = 0;
	descriptors[0].shader_stages = tr_shader_stage_vert;
	descriptors[1].type = tr_descriptor_type_texture_srv;
	descriptors[1].count = 1;
	descriptors[1].binding = 1;
	descriptors[1].shader_stages = tr_shader_stage_frag;
	descriptors[2].type = tr_descriptor_type_sampler;
	descriptors[2].count = 1;
	descriptors[2].binding = 2;
	descriptors[2].shader_stages = tr_shader_stage_frag;

	for (int i = 0; i < MAX_UI_PASSES; i++)
	{
		tr_create_descriptor_set(renderer, 3, &descriptors[0], &simple_ui_desc_set[i]);

		tr_vertex_layout vertex_layout = {};
		vertex_layout.attrib_count = 2;
		vertex_layout.attribs[0].semantic = tr_semantic_position;
		vertex_layout.attribs[0].format = tr_format_r32g32b32a32_float;
		vertex_layout.attribs[0].binding = 0;
		vertex_layout.attribs[0].location = 0;
		vertex_layout.attribs[0].offset = 0;
		vertex_layout.attribs[1].semantic = tr_semantic_texcoord0;
		vertex_layout.attribs[1].format = tr_format_r32g32_float;
		vertex_layout.attribs[1].binding = 0;
		vertex_layout.attribs[1].location = 1;
		vertex_layout.attribs[1].offset = tr_util_format_stride(tr_format_r32g32b32a32_float);
		tr_pipeline_settings pipeline_settings = { tr_primitive_topo_tri_list };		
		tr_create_pipeline(renderer, simple_ui_shader, &vertex_layout, simple_ui_desc_set[i], uiRenderTarget, &pipeline_settings, &simple_ui_pipeline[i]);
	}

	tr_create_vertex_buffer(renderer, r_uiVertexBufferSize.GetInteger()  * sizeof(uiVertex_t), true, sizeof(uiVertex_t), &simple_ui_vertex_buffer);

	uiVertexPool = (uiVertex_t*)simple_ui_vertex_buffer->cpu_mapped_address;

	tr_create_uniform_buffer(renderer, 16 * sizeof(float), true, &ui_uniform_buffer);

	uiRenderPasses.SetGranularity(1000);

	fileSystem->FreeFile(shaderFileBuffer);

	// Setup the UI matrixes.
	{
		float projectionMatrix[16] = { };
		//float modelViewMatrix[16] = { };

		projectionMatrix[0] = 2.0f / 640.0f;
		projectionMatrix[5] = -2.0f / 480.0f;
		projectionMatrix[10] = -2.0f / 1.0f;
		projectionMatrix[12] = -1.0f;
		projectionMatrix[13] = 1.0f;
		projectionMatrix[14] = -1.0f;
		projectionMatrix[15] = 1.0f;

		memcpy(ui_uniform_buffer->cpu_mapped_address, &projectionMatrix, sizeof(projectionMatrix));
	}
}

/*
==============
GL_SetUICanvas
==============
*/
void GL_SetUICanvas(float x, float y, float width, float height) {
	canvas_x = x;
	canvas_y = y;
}

/*
===============
GL_Upload32
===============
*/
void GL_RenderUISurface(srfTriangles_t* tri, const idMaterial* material) {
	uiRenderPass_t pass;

	const shaderStage_t* stage = material->GetAlbedoStage();
	if (stage == NULL) {
		return;
	}
	if (stage->texture.image == NULL) {
		return;
	}

	if (uiRenderPasses.Num() >= MAX_UI_PASSES) {
		return;
	}

	pass.startVertex = currentUIVertex;
	pass.numVertexes = tri->numIndexes;

	for (int i = 0; i < tri->numIndexes; i++) {
		int triIndex = tri->indexes[i];
		uiVertexPool[currentUIVertex + i].xyz.x = tri->verts[triIndex].xyz.x;
		uiVertexPool[currentUIVertex + i].xyz.y = tri->verts[triIndex].xyz.y;
		uiVertexPool[currentUIVertex + i].xyz.z = tri->verts[triIndex].xyz.z;
		uiVertexPool[currentUIVertex + i].xyz.w = 1;
		uiVertexPool[currentUIVertex + i].st = tri->verts[triIndex].st;
	}
	int texNum = stage->texture.image->texnum;
	pass.texture = textures[texNum].texture;

	pass.descriptorset = simple_ui_desc_set[uiRenderPasses.Num()];
	pass.descriptorset->descriptors[0].uniform_buffers[0] = ui_uniform_buffer;
	pass.descriptorset->descriptors[1].textures[0] = pass.texture;
	pass.descriptorset->descriptors[2].samplers[0] = m_sampler;	
	pass.pipeline = simple_ui_pipeline[uiRenderPasses.Num()];
		
	
	uiRenderPasses.Append(pass);

	currentUIVertex += tri->numIndexes;
}

/*
===============
GL_UpdateUI
===============
*/
void GL_UpdateUI(void) {
	for (int i = 0; i < uiRenderPasses.Num(); i++) {
		tr_update_descriptor_set(renderer, uiRenderPasses[i].descriptorset);
	}
}

/*
===============
GL_RenderUI
===============
*/
void GL_RenderUI(ID3D12GraphicsCommandList4* cmdList, ID3D12CommandAllocator* commandAllocator) {
	tr_cmd cmd = { };
	tr_cmd_pool pool = { };

	pool.dx_cmd_alloc = commandAllocator;
	pool.renderer = renderer;
	cmd.cmd_pool = &pool;
	cmd.dx_cmd_list = cmdList;

	tr_cmd_bind_vertex_buffers(&cmd, 1, &simple_ui_vertex_buffer);

	//tr_cmd_render_target_transition(&cmd, uiRenderTarget, tr_texture_usage_sampled_image, tr_texture_usage_color_attachment);
	tr_internal_dx_cmd_image_transition(&cmd, uiRenderTarget->color_attachments[0], tr_texture_usage_sampled_image, tr_texture_usage_color_attachment);
	tr_cmd_begin_render(&cmd, uiRenderTarget);
	tr_clear_value color_clear_value = { 0.0f, 0.0f, 0.0f, 0.0f };
	tr_cmd_clear_color_attachment(&cmd, 0, &color_clear_value);

	for (int i = 0; i < uiRenderPasses.Num(); i++) {
		tr_cmd_bind_pipeline(&cmd, uiRenderPasses[i].pipeline);		
		tr_cmd_bind_descriptor_sets(&cmd, uiRenderPasses[i].pipeline, uiRenderPasses[i].descriptorset);
		tr_cmd_draw(&cmd, uiRenderPasses[i].numVertexes, uiRenderPasses[i].startVertex);		
	}
	tr_cmd_end_render(&cmd);
	//tr_cmd_render_target_transition(&cmd, uiRenderTarget, tr_texture_usage_color_attachment, tr_texture_usage_sampled_image);
	tr_internal_dx_cmd_image_transition(&cmd, uiRenderTarget->color_attachments[0], tr_texture_usage_color_attachment, tr_texture_usage_sampled_image);

	uiRenderPasses.Clear();
	currentUIVertex = 0;
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32(int textureId, unsigned* data, int width, int height, bool mipmap, bool alpha)
{
	if (renderer == NULL) {
		return;
	}

	textures[textureId].width = width;
	textures[textureId].height = height;

	if (textures[textureId].texture != NULL) {
		tr_destroy_texture(renderer, textures[textureId].texture);
		textures[textureId].texture = NULL;
	}

	tr_create_texture_2d(renderer, width, height, tr_sample_count_1, tr_format_r8g8b8a8_unorm, 1, NULL, false, tr_texture_usage_sampled_image | tr_texture_usage_storage_image, &textures[textureId].texture);
	tr_util_update_texture_uint8(renderer->graphics_queue, width, height, width * 4, (byte *)data, 4, textures[textureId].texture, NULL, NULL);
}