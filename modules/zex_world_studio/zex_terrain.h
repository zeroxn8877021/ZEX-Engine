/**************************************************************************/
/*  zex_terrain.h                                                         */
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

#pragma once

#include "core/io/image.h"
#include "scene/3d/mesh_instance_3d.h"

// ZEX WORLD STUDIO - heightmap terrain.
// The terrain is a square grid of `resolution` x `resolution` vertices spaced `cell_size` apart.
// Local X/Z span [0, (resolution - 1) * cell_size]; heights are stored in world units.
class ZexTerrain : public MeshInstance3D {
	GDCLASS(ZexTerrain, MeshInstance3D);

public:
	enum BrushMode {
		BRUSH_RAISE,
		BRUSH_LOWER,
		BRUSH_SMOOTH,
		BRUSH_FLATTEN,
	};

	static constexpr int MIN_RESOLUTION = 2;
	static constexpr int MAX_RESOLUTION = 1025;

private:
	int resolution = 65;
	float cell_size = 1.0f;
	Vector<float> heights;

	_FORCE_INLINE_ int _idx(int p_x, int p_z) const { return p_z * resolution + p_x; }
	float _sample_cell(int p_x, int p_z) const;

protected:
	static void _bind_methods();

public:
	void set_resolution(int p_resolution);
	int get_resolution() const { return resolution; }

	void set_cell_size(float p_cell_size);
	float get_cell_size() const { return cell_size; }

	void set_heights(const Vector<float> &p_heights);
	Vector<float> get_heights() const { return heights; }

	float get_extent() const { return (resolution - 1) * cell_size; }

	// Generation.
	void flatten_all(float p_height);
	void generate_noise(int p_seed, float p_frequency, float p_amplitude, int p_octaves);
	Error load_heightmap(const Ref<Image> &p_image, float p_max_height);
	Ref<Image> export_heightmap(float p_max_height) const;
	void subdivide();

	// Queries (local space).
	float get_height_at(const Vector2 &p_xz) const;
	Vector3 get_normal_at(const Vector2 &p_xz) const;

	// Sculpting. `p_local_pos` is in the terrain's local space, `p_softness` in [0, 1].
	// Returns the number of vertices that were modified.
	int apply_brush(const Vector3 &p_local_pos, BrushMode p_mode, float p_radius, float p_strength, float p_softness);

	void rebuild_mesh();

	ZexTerrain();
};

VARIANT_ENUM_CAST(ZexTerrain::BrushMode);
