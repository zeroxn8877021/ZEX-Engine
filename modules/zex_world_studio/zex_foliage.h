/**************************************************************************/
/*  zex_foliage.h                                                         */
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

#include "scene/3d/multimesh_instance_3d.h"

class ZexTerrain;

// ZEX WORLD STUDIO - foliage scattering (trees, grass, bushes, rocks) on a ZexTerrain using one MultiMesh.
class ZexFoliage : public MultiMeshInstance3D {
	GDCLASS(ZexFoliage, MultiMeshInstance3D);

protected:
	static void _bind_methods();

public:
	// Scatters up to `p_count` instances inside `p_area` (terrain-local X/Z rectangle; an empty area means the whole terrain).
	// Returns the number of instances that were actually placed (steep slopes are skipped).
	int scatter(ZexTerrain *p_terrain, const Ref<Mesh> &p_mesh, int p_count, int p_seed, float p_min_scale, float p_max_scale, bool p_random_yaw, float p_max_slope_degrees, const Rect2 &p_area);
	void clear_instances();
};
