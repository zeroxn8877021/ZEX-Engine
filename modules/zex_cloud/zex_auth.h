/**************************************************************************/
/*  zex_auth.h                                                            */
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

#include "scene/main/http_request.h"
#include "scene/main/node.h"

// ZEX CLOUD - Supabase email/password authentication (GoTrue REST API).
//
// Security notes:
// - No credentials are compiled in. The project URL and the *anon* (public) key are supplied at runtime,
//   via configure() or load_config(). The service-role key must NEVER be given to this class.
// - The saved session is encrypted with a device-derived password. This protects against casual copying
//   of the file; it is not a replacement for the platform keystore.
// - The engine works fully offline: every method fails gracefully with a signal when there is no network.
class ZexAuth : public Node {
	GDCLASS(ZexAuth, Node);

public:
	enum Operation {
		OP_NONE,
		OP_SIGN_UP,
		OP_SIGN_IN,
		OP_REFRESH,
		OP_SIGN_OUT,
	};

private:
	String supabase_url;
	String anon_key;
	String session_path = "user://zex_session.dat";

	HTTPRequest *http = nullptr;
	Operation pending = OP_NONE;
	String pending_email;

	// Session data.
	String access_token;
	String refresh_token;
	String user_id;
	String user_email;
	double expires_at = 0.0;

	String _endpoint(const String &p_path) const;
	Vector<String> _headers(bool p_with_token) const;
	Error _send(Operation p_op, const String &p_path, const Dictionary &p_body, bool p_with_token);
	void _on_request_completed(int p_result, int p_code, const Vector<String> &p_headers, const Vector<uint8_t> &p_body);
	void _apply_session(const Dictionary &p_data);
	void _clear_session();
	String _session_password() const;

protected:
	static void _bind_methods();

public:
	// Pure helpers (also used by tests).
	static String validate_credentials(const String &p_email, const String &p_password);
	static String extract_error_message(const Variant &p_response, int p_http_code);

	void configure(const String &p_url, const String &p_anon_key);
	Error load_config(const String &p_path);
	bool is_configured() const { return !supabase_url.is_empty() && !anon_key.is_empty(); }

	Error sign_up(const String &p_email, const String &p_password);
	Error sign_in(const String &p_email, const String &p_password);
	Error refresh_session();
	Error sign_out();

	bool is_signed_in() const { return !access_token.is_empty(); }
	bool is_session_expired() const;
	bool is_busy() const { return pending != OP_NONE; }
	String get_access_token() const { return access_token; }
	String get_user_id() const { return user_id; }
	String get_user_email() const { return user_email; }

	Error save_session() const;
	Error restore_session();
	void clear_saved_session();

	ZexAuth();
};

VARIANT_ENUM_CAST(ZexAuth::Operation);
