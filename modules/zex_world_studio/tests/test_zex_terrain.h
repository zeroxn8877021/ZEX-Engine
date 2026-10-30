/**************************************************************************/
/*  test_zex_terrain.h                                                    */
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

#include "../zex_foliage.h"
#include "../zex_terrain.h"

#include "scene/resources/3d/primitive_meshes.h"
#include "tests/test_macros.h"

namespace TestZexTerrain {

TEST_CASE("[ZexTerrain] default state and resolution clamp") {
	ZexTerrain *t = memnew(ZexTerrain);
	CHECK(t->get_resolution() == 65);
	t->set_resolution(100000);
	CHECK(t->get_resolution() == ZexTerrain::MAX_RESOLUTION);
	t->set_resolution(0);
	CHECK(t->get_resolution() == ZexTerrain::MIN_RESOLUTION);
	memdelete(t);
}

TEST_CASE("[ZexTerrain] raise, lower and flatten brushes") {
	ZexTerrain *t = memnew(ZexTerrain);
	t->set_resolution(33);
	t->set_cell_size(1.0f);
	int changed = t->apply_brush(Vector3(16, 0, 16), ZexTerrain::BRUSH_RAISE, 4.0f, 2.0f, 0.0f);
	CHECK(changed > 0);
	CHECK(t->get_height_at(Vector2(16, 16)) == doctest::Approx(2.0f));
	CHECK(t->get_height_at(Vector2(0, 0)) == doctest::Approx(0.0f));
	t->apply_brush(Vector3(16, 0, 16), ZexTerrain::BRUSH_LOWER, 4.0f, 2.0f, 0.0f);
	CHECK(t->get_height_at(Vector2(16, 16)) == doctest::Approx(0.0f));
	t->flatten_all(5.0f);
	t->apply_brush(Vector3(16, 0, 16), ZexTerrain::BRUSH_RAISE, 2.0f, 3.0f, 0.0f);
	t->apply_brush(Vector3(16, 0, 16), ZexTerrain::BRUSH_FLATTEN, 6.0f, 1.0f, 0.0f);
	CHECK(t->get_height_at(Vector2(16, 16)) == doctest::Approx(8.0f));
	memdelete(t);
}

TEST_CASE("[ZexTerrain] smooth brush reduces a spike") {
	ZexTerrain *t = memnew(ZexTerrain);
	t->set_resolution(17);
	t->apply_brush(Vector3(8, 0, 8), ZexTerrain::BRUSH_RAISE, 1.0f, 10.0f, 0.0f);
	float before = t->get_height_at(Vector2(8, 8));
	t->apply_brush(Vector3(8, 0, 8), ZexTerrain::BRUSH_SMOOTH, 3.0f, 1.0f, 0.0f);
	CHECK(t->get_height_at(Vector2(8, 8)) < before);
	memdelete(t);
}

TEST_CASE("[ZexTerrain] subdivide keeps shape and noise is deterministic") {
	ZexTerrain *a = memnew(ZexTerrain);
	ZexTerrain *b = memnew(ZexTerrain);
	a->set_resolution(9);
	b->set_resolution(9);
	a->generate_noise(42, 0.1f, 3.0f, 3);
	b->generate_noise(42, 0.1f, 3.0f, 3);
	CHECK(a->get_heights() == b->get_heights());
	float extent = a->get_extent();
	float h = a->get_height_at(Vector2(3.0f, 5.0f));
	a->subdivide();
	CHECK(a->get_resolution() == 17);
	CHECK(a->get_extent() == doctest::Approx(extent));
	CHECK(a->get_height_at(Vector2(3.0f, 5.0f)) == doctest::Approx(h).epsilon(0.001));
	memdelete(a);
	memdelete(b);
}

TEST_CASE("[ZexTerrain] heightmap export and import round trip") {
	ZexTerrain *t = memnew(ZexTerrain);
	t->set_resolution(9);
	t->apply_brush(Vector3(4, 0, 4), ZexTerrain::BRUSH_RAISE, 2.0f, 5.0f, 0.0f);
	Ref<Image> img = t->export_heightmap(10.0f);
	REQUIRE(img.is_valid());
	CHECK(img->get_width() == 9);
	ZexTerrain *u = memnew(ZexTerrain);
	CHECK(u->load_heightmap(img, 10.0f) == OK);
	CHECK(u->get_resolution() == 9);
	CHECK(u->get_height_at(Vector2(4, 4)) == doctest::Approx(5.0f).epsilon(0.001));
	memdelete(t);
	memdelete(u);
}

TEST_CASE("[ZexFoliage] scatter places instances inside the terrain") {
	ZexTerrain *t = memnew(ZexTerrain);
	t->set_resolution(33);
	ZexFoliage *f = memnew(ZexFoliage);
	Ref<BoxMesh> box;
	box.instantiate();
	int n = f->scatter(t, box, 50, 7, 0.5f, 1.5f, true, 35.0f, Rect2());
	CHECK(n == 50);
	CHECK(f->get_multimesh()->get_instance_count() == 50);
	memdelete(f);
	memdelete(t);
}

} // namespace TestZexTerrain
