

#include "workbench_private.h"

#include "BIF_gl.h"

#include "BLI_dynstr.h"
#include "BLI_hash.h"

#define HSV_SATURATION 0.5
#define HSV_VALUE 0.8

void workbench_material_update_data(WORKBENCH_PrivateData *wpd, Object *ob, Material *mat, WORKBENCH_MaterialData *data)
{
	/* When V3D_SHADING_TEXTURE_COLOR is active, use V3D_SHADING_MATERIAL_COLOR as fallback when no texture could be determined */
	int color_type = wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR ? V3D_SHADING_MATERIAL_COLOR : wpd->shading.color_type;
	copy_v3_fl3(data->diffuse_color, 0.8f, 0.8f, 0.8f);
	copy_v3_v3(data->base_color, data->diffuse_color);
	copy_v3_fl3(data->specular_color, 0.05f, 0.05f, 0.05f); /* Dielectric: 5% reflective. */
	data->metallic = 0.0f;
	data->roughness = 0.5f; /* sqrtf(0.25f); */

	if (color_type == V3D_SHADING_SINGLE_COLOR) {
		copy_v3_v3(data->diffuse_color, wpd->shading.single_color);
		copy_v3_v3(data->base_color, data->diffuse_color);
	}
	else if (color_type == V3D_SHADING_RANDOM_COLOR) {
		uint hash = BLI_ghashutil_strhash_p_murmur(ob->id.name);
		if (ob->id.lib) {
			hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob->id.lib->name);
		}

		float hue = BLI_hash_int_01(hash);
		float hsv[3] = {hue, HSV_SATURATION, HSV_VALUE};
		hsv_to_rgb_v(hsv, data->diffuse_color);
		copy_v3_v3(data->base_color, data->diffuse_color);
	}
	else {
		/* V3D_SHADING_MATERIAL_COLOR */
		if (mat) {
			if (SPECULAR_HIGHLIGHT_ENABLED(wpd)) {
				copy_v3_v3(data->base_color, &mat->r);
				mul_v3_v3fl(data->diffuse_color, &mat->r, 1.0f - mat->metallic);
				mul_v3_v3fl(data->specular_color, &mat->r, mat->metallic);
				add_v3_fl(data->specular_color, 0.05f * (1.0f - mat->metallic));
				data->metallic = mat->metallic;
				data->roughness = sqrtf(mat->roughness); /* Remap to disney roughness. */
			}
			else {
				copy_v3_v3(data->base_color, &mat->r);
				copy_v3_v3(data->diffuse_color, &mat->r);
			}
		}
	}
}

char *workbench_material_build_defines(WORKBENCH_PrivateData *wpd, bool use_textures, bool is_hair)
{
	char *str = NULL;

	DynStr *ds = BLI_dynstr_new();

	if (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE) {
		BLI_dynstr_appendf(ds, "#define V3D_SHADING_OBJECT_OUTLINE\n");
	}
	if (wpd->shading.flag & V3D_SHADING_SHADOW) {
		BLI_dynstr_appendf(ds, "#define V3D_SHADING_SHADOW\n");
	}
	if (SSAO_ENABLED(wpd) || CURVATURE_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define WB_CAVITY\n");
	}
	if (SPECULAR_HIGHLIGHT_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define V3D_SHADING_SPECULAR_HIGHLIGHT\n");
	}
	if (STUDIOLIGHT_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define V3D_LIGHTING_STUDIO\n");
	}
	if (FLAT_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define V3D_LIGHTING_FLAT\n");
	}
	if (MATCAP_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define V3D_LIGHTING_MATCAP\n");
	}
	if (NORMAL_VIEWPORT_PASS_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define NORMAL_VIEWPORT_PASS_ENABLED\n");
	}
	if (use_textures) {
		BLI_dynstr_appendf(ds, "#define V3D_SHADING_TEXTURE_COLOR\n");
	}
	if (NORMAL_ENCODING_ENABLED()) {
		BLI_dynstr_appendf(ds, "#define WORKBENCH_ENCODE_NORMALS\n");
	}
	if (is_hair) {
		BLI_dynstr_appendf(ds, "#define HAIR_SHADER\n");
	}

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return str;
}

uint workbench_material_get_hash(WORKBENCH_MaterialData *material_template, bool is_ghost)
{
	uint input[4];
	uint result;
	float *color = material_template->diffuse_color;
	input[0] = (uint)(color[0] * 512);
	input[1] = (uint)(color[1] * 512);
	input[2] = (uint)(color[2] * 512);
	input[3] = material_template->object_id;
	result = BLI_ghashutil_uinthash_v4_murmur(input);

	color = material_template->specular_color;
	input[0] = (uint)(color[0] * 512);
	input[1] = (uint)(color[1] * 512);
	input[2] = (uint)(color[2] * 512);
	input[3] = (uint)(material_template->roughness * 512);
	result += BLI_ghashutil_uinthash_v4_murmur(input);

	result += BLI_ghashutil_uinthash((uint)is_ghost);

	/* add texture reference */
	if (material_template->ima) {
		result += BLI_ghashutil_inthash_p_murmur(material_template->ima);
	}

	return result;
}

int workbench_material_get_shader_index(WORKBENCH_PrivateData *wpd, bool use_textures, bool is_hair)
{
	/* NOTE: change MAX_SHADERS accordingly when modifying this function. */
	int index = 0;
	/* 1 bit V3D_SHADING_TEXTURE_COLOR */
	SET_FLAG_FROM_TEST(index, use_textures, 1 << 0);
	/* 2 bits FLAT/STUDIO/MATCAP + Specular highlight */
	int ligh_flag = SPECULAR_HIGHLIGHT_ENABLED(wpd) ? 3 : wpd->shading.light;
	SET_FLAG_FROM_TEST(index, wpd->shading.light, ligh_flag << 1);
	/* 3 bits for flags */
	SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_SHADOW, 1 << 3);
	SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_CAVITY, 1 << 4);
	SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE, 1 << 5);
	/* 1 bit for hair */
	SET_FLAG_FROM_TEST(index, is_hair, 1 << 6);
	return index;
}

int workbench_material_determine_color_type(WORKBENCH_PrivateData *wpd, Image *ima, Object *ob)
{
	int color_type = wpd->shading.color_type;
	if ((color_type == V3D_SHADING_TEXTURE_COLOR && ima == NULL) || (ob->dt < OB_TEXTURE)) {
		color_type = V3D_SHADING_MATERIAL_COLOR;
	}
	return color_type;
}

void workbench_material_shgroup_uniform(
        WORKBENCH_PrivateData *wpd, DRWShadingGroup *grp, WORKBENCH_MaterialData *material, Object *ob,
        const bool use_metallic)
{
	if (workbench_material_determine_color_type(wpd, material->ima, ob) == V3D_SHADING_TEXTURE_COLOR) {
		GPUTexture *tex = GPU_texture_from_blender(material->ima, NULL, GL_TEXTURE_2D, false, 0.0f);
		DRW_shgroup_uniform_texture(grp, "image", tex);
	}
	else {
		DRW_shgroup_uniform_vec3(grp, "materialDiffuseColor", (use_metallic) ? material->base_color : material->diffuse_color, 1);
	}

	if (SPECULAR_HIGHLIGHT_ENABLED(wpd)) {
		if (use_metallic) {
			DRW_shgroup_uniform_float(grp, "materialMetallic", &material->metallic, 1);
		}
		else {
			DRW_shgroup_uniform_vec3(grp, "materialSpecularColor", material->specular_color, 1);
		}
		DRW_shgroup_uniform_float(grp, "materialRoughness", &material->roughness, 1);
	}
}

void workbench_material_copy(WORKBENCH_MaterialData *dest_material, const WORKBENCH_MaterialData *source_material)
{
	dest_material->object_id = source_material->object_id;
	copy_v3_v3(dest_material->base_color, source_material->base_color);
	copy_v3_v3(dest_material->diffuse_color, source_material->diffuse_color);
	copy_v3_v3(dest_material->specular_color, source_material->specular_color);
	dest_material->metallic = source_material->metallic;
	dest_material->roughness = source_material->roughness;
	dest_material->ima = source_material->ima;
}
