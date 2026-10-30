/**************************************************************************/
/*  zex_foliage.cpp                                                       */
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

#include "zex_foliage.h"

#include "zex_terrain.h"

#include "core/math/random_pcg.h"
#include "scene/resources/multimesh.h"

int ZexFoliage::scatter(ZexTerrain *p_terrain, const Ref<Mesh> &p_mesh, int p_count, int p_seed, float p_min_scale, float p_max_scale, bool p_random_yaw, float p_max_slope_degrees, const Rect2 &p_area) {
	ERR_FAIL_NULL_V_MSG(p_terrain, 0, "Terrain is null.");
	ERR_FAIL_COND_V_MSG(p_mesh.is_null(), 0, "Mesh is null.");
	ERR_FAIL_COND_V_MSG(p_count < 0, 0, "Count must not be negative.");
	ERR_FAIL_COND_V_MSG(p_min_scale <= 0.0f || p_max_scale < p_min_scale, 0, "Invalid scale range.");

	const float extent = p_terrain->get_extent();
	Rect2 area = p_area;
	if (area.size.x <= 0.0f || area.size.y <= 0.0f) {
		area = Rect2(0, 0, extent, extent);
	}
	area = area.intersection(Rect2(0, 0, extent, extent));
	ERR_FAIL_COND_V_MSG(area.size.x <= 0.0f || area.size.y <= 0.0f, 0, "Scatter area is outside the terrain.");

	const float min_up_dot = Math::cos(Math::deg_to_rad(CLAMP(p_max_slope_degrees, 0.0f, 90.0f)));

	RandomPCG rng;
	rng.seed(p_seed);

	Vector<Transform3D> placed;
	// Allow extra attempts so rejected (too steep) samples do not leave the area under-populated.
	const int max_attempts = p_count * 4;
	for (int i = 0; i < max_attempts && placed.size() < p_count; i++) {
		float x = area.position.x + rng.randf() * area.size.x;
		float z = area.position.y + rng.randf() * area.size.y;
		Vector2 xz(x, z);
		if (p_terrain->get_normal_at(xz).y < min_up_dot) {
			continue;
		}
		float s = Math::lerp(p_min_scale, p_max_scale, rng.randf());
		float yaw = p_random_yaw ? (float)(rng.randf() * Math::TAU) : 0.0f;
		Basis basis = Basis(Vector3(0, 1, 0), yaw).scaled(Vector3(s, s, s));
		placed.push_back(Transform3D(basis, Vector3(x, p_terrain->get_height_at(xz), z)));
	}

	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(p_mesh);
	mm->set_instance_count(placed.size());
	for (int i = 0; i < placed.size(); i++) {
		mm->set_instance_transform(i, placed[i]);
	}
	set_multimesh(mm);

	// Foliage positions are terrain-local, so follow the terrain's transform.
	set_transform(p_terrain->get_transform());
	return placed.size();
}

void ZexFoliage::clear_instances() {
	set_multimesh(Ref<MultiMesh>());
}

void ZexFoliage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("scatter", "terrain", "mesh", "count", "seed", "min_scale", "max_scale", "random_yaw", "max_slope_degrees", "area"), &ZexFoliage::scatter, DEFVAL(1.0), DEFVAL(1.0), DEFVAL(true), DEFVAL(35.0), DEFVAL(Rect2()));
	ClassDB::bind_method(D_METHOD("clear_instances"), &ZexFoliage::clear_instances);
}
