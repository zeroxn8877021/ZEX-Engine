/**************************************************************************/
/*  test_zex_auth.h                                                       */
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

#include "../zex_auth.h"

#include "core/io/json.h"
#include "tests/test_macros.h"

namespace TestZexAuth {

TEST_CASE("[ZexAuth] credential validation") {
	CHECK(ZexAuth::validate_credentials("user@example.com", "longenough1").is_empty());
	CHECK(!ZexAuth::validate_credentials("", "longenough1").is_empty());
	CHECK(!ZexAuth::validate_credentials("not-an-email", "longenough1").is_empty());
	CHECK(!ZexAuth::validate_credentials("a@b", "longenough1").is_empty());
	CHECK(!ZexAuth::validate_credentials("a b@example.com", "longenough1").is_empty());
	CHECK(!ZexAuth::validate_credentials("user@example.com", "short").is_empty());
}

TEST_CASE("[ZexAuth] error message extraction") {
	Variant v = JSON::parse_string("{\"error_description\":\"Invalid login credentials\"}");
	CHECK(ZexAuth::extract_error_message(v, 400) == "Invalid login credentials");
	Variant w = JSON::parse_string("{\"msg\":\"User already registered\"}");
	CHECK(ZexAuth::extract_error_message(w, 422) == "User already registered");
	CHECK(ZexAuth::extract_error_message(Variant(), 500).contains("500"));
	CHECK(ZexAuth::extract_error_message(Variant(), 0).contains("offline"));
}

TEST_CASE("[ZexAuth] unconfigured and offline behaviour") {
	ZexAuth *auth = memnew(ZexAuth);
	CHECK(!auth->is_configured());
	ERR_PRINT_OFF;
	CHECK(auth->sign_in("user@example.com", "longenough1") == ERR_UNCONFIGURED);
	ERR_PRINT_ON;
	CHECK(!auth->is_signed_in());
	auth->configure("https://example.supabase.co/", "anon");
	CHECK(auth->is_configured());
	CHECK(auth->sign_out() == OK);
	memdelete(auth);
}

} // namespace TestZexAuth
