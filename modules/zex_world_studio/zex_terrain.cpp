/**************************************************************************/
/*  zex_terrain.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "zex_terrain.h"

#include "core/math/math_funcs.h"
#include "scene/resources/mesh.h"

// Deterministic value noise (no external dependency).
static uint32_t _zex_hash(int32_t p_x, int32_t p_z, uint32_t p_seed) {
	uint32_t h = (uint32_t)p_x * 374761393u + (uint32_t)p_z * 668265263u + p_seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static float _zex_lattice(int32_t p_x, int32_t p_z, uint32_t p_seed) {
	return (float)(_zex_hash(p_x, p_z, p_seed) & 0xFFFFFF) / (float)0xFFFFFF * 2.0f - 1.0f;
}

static float _zex_value_noise(float p_x, float p_z, uint32_t p_seed) {
	int32_t x0 = (int32_t)Math::floor(p_x);
	int32_t z0 = (int32_t)Math::floor(p_z);
	float fx = p_x - x0;
	float fz = p_z - z0;
	float sx = fx * fx * (3.0f - 2.0f * fx);
	float sz = fz * fz * (3.0f - 2.0f * fz);
	float a = _zex_lattice(x0, z0, p_seed);
	float b = _zex_lattice(x0 + 1, z0, p_seed);
	float c = _zex_lattice(x0, z0 + 1, p_seed);
	float d = _zex_lattice(x0 + 1, z0 + 1, p_seed);
	return Math::lerp(Math::lerp(a, b, sx), Math::lerp(c, d, sx), sz);
}

ZexTerrain::ZexTerrain() {
	heights.resize(resolution * resolution);
	heights.fill(0.0f);
	rebuild_mesh();
}

float ZexTerrain::_sample_cell(int p_x, int p_z) const {
	p_x = CLAMP(p_x, 0, resolution - 1);
	p_z = CLAMP(p_z, 0, resolution - 1);
	return heights[_idx(p_x, p_z)];
}

void ZexTerrain::set_resolution(int p_resolution) {
	p_resolution = CLAMP(p_resolution, MIN_RESOLUTION, MAX_RESOLUTION);
	if (p_resolution == resolution) {
		return;
	}
	resolution = p_resolution;
	heights.resize(resolution * resolution);
	heights.fill(0.0f);
	rebuild_mesh();
}

void ZexTerrain::set_cell_size(float p_cell_size) {
	ERR_FAIL_COND_MSG(p_cell_size <= 0.0f, "Cell size must be greater than zero.");
	cell_size = p_cell_size;
	rebuild_mesh();
}

void ZexTerrain::set_heights(const Vector<float> &p_heights) {
	ERR_FAIL_COND_MSG(p_heights.size() != resolution * resolution, "Heights array size must equal resolution * resolution.");
	heights = p_heights;
	rebuild_mesh();
}

void ZexTerrain::flatten_all(float p_height) {
	heights.fill(p_height);
	rebuild_mesh();
}

void ZexTerrain::generate_noise(int p_seed, float p_frequency, float p_amplitude, int p_octaves) {
	ERR_FAIL_COND_MSG(p_frequency <= 0.0f, "Frequency must be greater than zero.");
	p_octaves = CLAMP(p_octaves, 1, 8);
	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			float freq = p_frequency;
			float amp = p_amplitude;
			float h = 0.0f;
			for (int o = 0; o < p_octaves; o++) {
				h += _zex_value_noise(x * cell_size * freq, z * cell_size * freq, (uint32_t)p_seed + o * 101u) * amp;
				freq *= 2.0f;
				amp *= 0.5f;
			}
			heights.write[_idx(x, z)] = h;
		}
	}
	rebuild_mesh();
}

Error ZexTerrain::load_heightmap(const Ref<Image> &p_image, float p_max_height) {
	ERR_FAIL_COND_V_MSG(p_image.is_null() || p_image->is_empty(), ERR_INVALID_PARAMETER, "Heightmap image is empty.");
	Ref<Image> img = p_image->duplicate();
	if (img->is_compressed()) {
		Error err = img->decompress();
		ERR_FAIL_COND_V_MSG(err != OK, err, "Could not decompress heightmap image.");
	}
	int res = CLAMP(MIN(img->get_width(), img->get_height()), MIN_RESOLUTION, MAX_RESOLUTION);
	resolution = res;
	heights.resize(res * res);
	for (int z = 0; z < res; z++) {
		for (int x = 0; x < res; x++) {
			heights.write[_idx(x, z)] = img->get_pixel(x, z).r * p_max_height;
		}
	}
	rebuild_mesh();
	return OK;
}

Ref<Image> ZexTerrain::export_heightmap(float p_max_height) const {
	ERR_FAIL_COND_V_MSG(p_max_height <= 0.0f, Ref<Image>(), "Max height must be greater than zero.");
	Ref<Image> img = Image::create_empty(resolution, resolution, false, Image::FORMAT_RF);
	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			float v = CLAMP(heights[_idx(x, z)] / p_max_height, 0.0f, 1.0f);
			img->set_pixel(x, z, Color(v, 0, 0, 1));
		}
	}
	return img;
}

void ZexTerrain::subdivide() {
	int new_res = resolution * 2 - 1;
	ERR_FAIL_COND_MSG(new_res > MAX_RESOLUTION, "Terrain is already at maximum resolution.");
	Vector<float> nh;
	nh.resize(new_res * new_res);
	float new_cell = cell_size * (float)(resolution - 1) / (float)(new_res - 1);
	int old_res = resolution;
	float old_cell = cell_size;
	for (int z = 0; z < new_res; z++) {
		for (int x = 0; x < new_res; x++) {
			Vector2 p(x * new_cell, z * new_cell);
			nh.write[z * new_res + x] = get_height_at(p);
		}
	}
	(void)old_res;
	(void)old_cell;
	resolution = new_res;
	cell_size = new_cell;
	heights = nh;
	rebuild_mesh();
}

float ZexTerrain::get_height_at(const Vector2 &p_xz) const {
	float gx = CLAMP(p_xz.x / cell_size, 0.0f, (float)(resolution - 1));
	float gz = CLAMP(p_xz.y / cell_size, 0.0f, (float)(resolution - 1));
	int x0 = MIN((int)Math::floor(gx), resolution - 2);
	int z0 = MIN((int)Math::floor(gz), resolution - 2);
	float fx = gx - x0;
	float fz = gz - z0;
	float h00 = heights[_idx(x0, z0)];
	float h10 = heights[_idx(x0 + 1, z0)];
	float h01 = heights[_idx(x0, z0 + 1)];
	float h11 = heights[_idx(x0 + 1, z0 + 1)];
	return Math::lerp(Math::lerp(h00, h10, fx), Math::lerp(h01, h11, fx), fz);
}

Vector3 ZexTerrain::get_normal_at(const Vector2 &p_xz) const {
	float e = cell_size;
	float hl = get_height_at(p_xz - Vector2(e, 0));
	float hr = get_height_at(p_xz + Vector2(e, 0));
	float hd = get_height_at(p_xz - Vector2(0, e));
	float hu = get_height_at(p_xz + Vector2(0, e));
	return Vector3(hl - hr, 2.0f * e, hd - hu).normalized();
}

int ZexTerrain::apply_brush(const Vector3 &p_local_pos, BrushMode p_mode, float p_radius, float p_strength, float p_softness) {
	ERR_FAIL_COND_V_MSG(p_radius <= 0.0f, 0, "Brush radius must be greater than zero.");
	p_softness = CLAMP(p_softness, 0.0f, 1.0f);
	const float hard = 1.0f - p_softness;
	const float target = get_height_at(Vector2(p_local_pos.x, p_local_pos.z));

	int x_min = MAX(0, (int)Math::floor((p_local_pos.x - p_radius) / cell_size));
	int x_max = MIN(resolution - 1, (int)Math::ceil((p_local_pos.x + p_radius) / cell_size));
	int z_min = MAX(0, (int)Math::floor((p_local_pos.z - p_radius) / cell_size));
	int z_max = MIN(resolution - 1, (int)Math::ceil((p_local_pos.z + p_radius) / cell_size));

	// Smoothing reads from a snapshot so the result does not depend on iteration order.
	Vector<float> source = heights;
	int changed = 0;

	for (int z = z_min; z <= z_max; z++) {
		for (int x = x_min; x <= x_max; x++) {
			float dx = x * cell_size - p_local_pos.x;
			float dz = z * cell_size - p_local_pos.z;
			float d = Math::sqrt(dx * dx + dz * dz) / p_radius;
			if (d >= 1.0f) {
				continue;
			}
			float w = 1.0f;
			if (d > hard) {
				float t = (d - hard) / MAX(1.0f - hard, 0.0001f);
				w = 1.0f - t * t * (3.0f - 2.0f * t);
			}
			float &h = heights.write[_idx(x, z)];
			switch (p_mode) {
				case BRUSH_RAISE: {
					h += p_strength * w;
				} break;
				case BRUSH_LOWER: {
					h -= p_strength * w;
				} break;
				case BRUSH_SMOOTH: {
					int xl = MAX(x - 1, 0);
					int xr = MIN(x + 1, resolution - 1);
					int zd = MAX(z - 1, 0);
					int zu = MIN(z + 1, resolution - 1);
					float avg = (source[_idx(x, z)] + source[_idx(xl, z)] + source[_idx(xr, z)] + source[_idx(x, zd)] + source[_idx(x, zu)]) / 5.0f;
					h = Math::lerp(source[_idx(x, z)], avg, CLAMP(p_strength * w, 0.0f, 1.0f));
				} break;
				case BRUSH_FLATTEN: {
					h = Math::lerp(h, target, CLAMP(p_strength * w, 0.0f, 1.0f));
				} break;
			}
			changed++;
		}
	}

	if (changed > 0) {
		rebuild_mesh();
	}
	return changed;
}

void ZexTerrain::rebuild_mesh() {
	const int count = resolution * resolution;
	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	Vector<int> indices;
	vertices.resize(count);
	normals.resize(count);
	uvs.resize(count);
	indices.resize((resolution - 1) * (resolution - 1) * 6);

	Vector3 *vw = vertices.ptrw();
	Vector3 *nw = normals.ptrw();
	Vector2 *uw = uvs.ptrw();
	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			int i = _idx(x, z);
			vw[i] = Vector3(x * cell_size, heights[i], z * cell_size);
			uw[i] = Vector2((float)x / (float)(resolution - 1), (float)z / (float)(resolution - 1));
			float hl = _sample_cell(x - 1, z);
			float hr = _sample_cell(x + 1, z);
			float hd = _sample_cell(x, z - 1);
			float hu = _sample_cell(x, z + 1);
			nw[i] = Vector3(hl - hr, 2.0f * cell_size, hd - hu).normalized();
		}
	}

	int *iw = indices.ptrw();
	int k = 0;
	for (int z = 0; z < resolution - 1; z++) {
		for (int x = 0; x < resolution - 1; x++) {
			int a = _idx(x, z);
			int b = _idx(x + 1, z);
			int c = _idx(x, z + 1);
			int d = _idx(x + 1, z + 1);
			// Clockwise winding (Godot front faces), viewed from above.
			iw[k++] = a;
			iw[k++] = b;
			iw[k++] = c;
			iw[k++] = b;
			iw[k++] = d;
			iw[k++] = c;
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	set_mesh(mesh);
}

void ZexTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_resolution", "resolution"), &ZexTerrain::set_resolution);
	ClassDB::bind_method(D_METHOD("get_resolution"), &ZexTerrain::get_resolution);
	ClassDB::bind_method(D_METHOD("set_cell_size", "cell_size"), &ZexTerrain::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &ZexTerrain::get_cell_size);
	ClassDB::bind_method(D_METHOD("set_heights", "heights"), &ZexTerrain::set_heights);
	ClassDB::bind_method(D_METHOD("get_heights"), &ZexTerrain::get_heights);
	ClassDB::bind_method(D_METHOD("get_extent"), &ZexTerrain::get_extent);

	ClassDB::bind_method(D_METHOD("flatten_all", "height"), &ZexTerrain::flatten_all);
	ClassDB::bind_method(D_METHOD("generate_noise", "seed", "frequency", "amplitude", "octaves"), &ZexTerrain::generate_noise, DEFVAL(4));
	ClassDB::bind_method(D_METHOD("load_heightmap", "image", "max_height"), &ZexTerrain::load_heightmap);
	ClassDB::bind_method(D_METHOD("export_heightmap", "max_height"), &ZexTerrain::export_heightmap);
	ClassDB::bind_method(D_METHOD("subdivide"), &ZexTerrain::subdivide);

	ClassDB::bind_method(D_METHOD("get_height_at", "xz"), &ZexTerrain::get_height_at);
	ClassDB::bind_method(D_METHOD("get_normal_at", "xz"), &ZexTerrain::get_normal_at);
	ClassDB::bind_method(D_METHOD("apply_brush", "local_position", "mode", "radius", "strength", "softness"), &ZexTerrain::apply_brush, DEFVAL(0.5));
	ClassDB::bind_method(D_METHOD("rebuild_mesh"), &ZexTerrain::rebuild_mesh);

	// Order matters: resolution must be restored before heights when loading a scene.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "resolution", PROPERTY_HINT_RANGE, "2,1025,1"), "set_resolution", "get_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.01,100,0.01,or_greater"), "set_cell_size", "get_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "heights", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_heights", "get_heights");

	BIND_ENUM_CONSTANT(BRUSH_RAISE);
	BIND_ENUM_CONSTANT(BRUSH_LOWER);
	BIND_ENUM_CONSTANT(BRUSH_SMOOTH);
	BIND_ENUM_CONSTANT(BRUSH_FLATTEN);
}
