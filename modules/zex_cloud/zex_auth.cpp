/**************************************************************************/
/*  zex_auth.cpp                                                          */
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

#include "zex_auth.h"

#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"

ZexAuth::ZexAuth() {
	http = memnew(HTTPRequest);
	http->set_timeout(20.0);
	add_child(http);
	http->connect("request_completed", callable_mp(this, &ZexAuth::_on_request_completed));
}

String ZexAuth::validate_credentials(const String &p_email, const String &p_password) {
	String email = p_email.strip_edges();
	int at = email.find_char('@');
	if (email.is_empty() || at <= 0 || at != email.rfind_char('@') || email.find_char('.', at) < 0 || email.ends_with(".") || email.contains(" ")) {
		return "Please enter a valid email address.";
	}
	if (p_password.length() < 8) {
		return "Password must be at least 8 characters.";
	}
	return String();
}

String ZexAuth::extract_error_message(const Variant &p_response, int p_http_code) {
	if (p_response.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_response;
		const char *keys[] = { "error_description", "msg", "message", "error" };
		for (const char *k : keys) {
			if (d.has(k) && d[k].get_type() == Variant::STRING && !String(d[k]).is_empty()) {
				return d[k];
			}
		}
	}
	if (p_http_code == 0) {
		return "No connection to the ZEX cloud. You can keep working offline.";
	}
	return vformat("Request failed (HTTP %d).", p_http_code);
}

void ZexAuth::configure(const String &p_url, const String &p_anon_key) {
	String url = p_url.strip_edges();
	while (url.ends_with("/")) {
		url = url.substr(0, url.length() - 1);
	}
	supabase_url = url;
	anon_key = p_anon_key.strip_edges();
}

Error ZexAuth::load_config(const String &p_path) {
	Ref<ConfigFile> cf;
	cf.instantiate();
	Error err = cf->load(p_path);
	if (err != OK) {
		return err;
	}
	configure(cf->get_value("supabase", "url", ""), cf->get_value("supabase", "anon_key", ""));
	return is_configured() ? OK : ERR_UNCONFIGURED;
}

String ZexAuth::_endpoint(const String &p_path) const {
	return supabase_url + p_path;
}

Vector<String> ZexAuth::_headers(bool p_with_token) const {
	Vector<String> h;
	h.push_back("Content-Type: application/json");
	h.push_back("apikey: " + anon_key);
	if (p_with_token && !access_token.is_empty()) {
		h.push_back("Authorization: Bearer " + access_token);
	}
	return h;
}

Error ZexAuth::_send(Operation p_op, const String &p_path, const Dictionary &p_body, bool p_with_token) {
	ERR_FAIL_COND_V_MSG(!is_configured(), ERR_UNCONFIGURED, "ZexAuth is not configured. Call configure() or load_config() first.");
	ERR_FAIL_COND_V_MSG(pending != OP_NONE, ERR_BUSY, "Another authentication request is in progress.");
	ERR_FAIL_COND_V_MSG(!supabase_url.begins_with("https://") && !supabase_url.begins_with("http://localhost") && !supabase_url.begins_with("http://127.0.0.1"), ERR_INVALID_PARAMETER, "Supabase URL must use https://.");
	pending = p_op;
	Error err = http->request(_endpoint(p_path), _headers(p_with_token), HTTPClient::METHOD_POST, JSON::stringify(p_body));
	if (err != OK) {
		pending = OP_NONE;
		emit_signal("auth_failed", "Could not start the request. Check your internet connection.", 0);
	}
	return err;
}

Error ZexAuth::sign_up(const String &p_email, const String &p_password) {
	String problem = validate_credentials(p_email, p_password);
	if (!problem.is_empty()) {
		emit_signal("auth_failed", problem, 0);
		return ERR_INVALID_PARAMETER;
	}
	pending_email = p_email.strip_edges();
	Dictionary body;
	body["email"] = pending_email;
	body["password"] = p_password;
	return _send(OP_SIGN_UP, "/auth/v1/signup", body, false);
}

Error ZexAuth::sign_in(const String &p_email, const String &p_password) {
	String problem = validate_credentials(p_email, p_password);
	if (!problem.is_empty()) {
		emit_signal("auth_failed", problem, 0);
		return ERR_INVALID_PARAMETER;
	}
	pending_email = p_email.strip_edges();
	Dictionary body;
	body["email"] = pending_email;
	body["password"] = p_password;
	return _send(OP_SIGN_IN, "/auth/v1/token?grant_type=password", body, false);
}

Error ZexAuth::refresh_session() {
	ERR_FAIL_COND_V_MSG(refresh_token.is_empty(), ERR_UNAVAILABLE, "There is no session to refresh.");
	Dictionary body;
	body["refresh_token"] = refresh_token;
	return _send(OP_REFRESH, "/auth/v1/token?grant_type=refresh_token", body, false);
}

Error ZexAuth::sign_out() {
	if (access_token.is_empty()) {
		return OK;
	}
	if (!is_configured() || pending != OP_NONE) {
		// Offline or busy: still sign out locally so the user is never stuck.
		_clear_session();
		clear_saved_session();
		emit_signal("signed_out");
		return OK;
	}
	return _send(OP_SIGN_OUT, "/auth/v1/logout", Dictionary(), true);
}

void ZexAuth::_apply_session(const Dictionary &p_data) {
	access_token = p_data.get("access_token", "");
	refresh_token = p_data.get("refresh_token", "");
	double expires_in = p_data.get("expires_in", 3600);
	expires_at = OS::get_singleton()->get_unix_time() + expires_in;
	Dictionary user = p_data.get("user", Dictionary());
	user_id = user.get("id", "");
	user_email = user.get("email", pending_email);
}

void ZexAuth::_clear_session() {
	access_token = String();
	refresh_token = String();
	user_id = String();
	user_email = String();
	expires_at = 0.0;
}

bool ZexAuth::is_session_expired() const {
	return is_signed_in() && OS::get_singleton()->get_unix_time() >= expires_at;
}

void ZexAuth::_on_request_completed(int p_result, int p_code, const Vector<String> &p_headers, const Vector<uint8_t> &p_body) {
	Operation op = pending;
	pending = OP_NONE;

	Variant response;
	if (p_body.size() > 0) {
		response = JSON::parse_string(String::utf8((const char *)p_body.ptr(), p_body.size()));
	}

	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		emit_signal("auth_failed", extract_error_message(Variant(), 0), 0);
		return;
	}

	if (op == OP_SIGN_OUT) {
		// Whatever the server says, the local session is removed.
		_clear_session();
		clear_saved_session();
		emit_signal("signed_out");
		return;
	}

	if (p_code < 200 || p_code >= 300) {
		emit_signal("auth_failed", extract_error_message(response, p_code), p_code);
		return;
	}

	Dictionary data = response.get_type() == Variant::DICTIONARY ? Dictionary(response) : Dictionary();
	if (data.has("access_token")) {
		_apply_session(data);
		save_session();
		emit_signal("auth_succeeded", user_email);
	} else if (op == OP_SIGN_UP) {
		// Email confirmation is enabled: the account exists but there is no session yet.
		emit_signal("confirmation_required", pending_email);
	} else {
		emit_signal("auth_failed", "Unexpected response from the server.", p_code);
	}
}

String ZexAuth::_session_password() const {
	return "zex-session-" + OS::get_singleton()->get_unique_id();
}

Error ZexAuth::save_session() const {
	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->set_value("session", "access_token", access_token);
	cf->set_value("session", "refresh_token", refresh_token);
	cf->set_value("session", "user_id", user_id);
	cf->set_value("session", "email", user_email);
	cf->set_value("session", "expires_at", expires_at);
	return cf->save_encrypted_pass(session_path, _session_password());
}

Error ZexAuth::restore_session() {
	Ref<ConfigFile> cf;
	cf.instantiate();
	Error err = cf->load_encrypted_pass(session_path, _session_password());
	if (err != OK) {
		return err;
	}
	access_token = cf->get_value("session", "access_token", "");
	refresh_token = cf->get_value("session", "refresh_token", "");
	user_id = cf->get_value("session", "user_id", "");
	user_email = cf->get_value("session", "email", "");
	expires_at = cf->get_value("session", "expires_at", 0.0);
	return is_signed_in() ? OK : ERR_FILE_CORRUPT;
}

void ZexAuth::clear_saved_session() {
	if (FileAccess::exists(session_path)) {
		DirAccess::remove_absolute(session_path);
	}
}

void ZexAuth::_bind_methods() {
	ClassDB::bind_static_method("ZexAuth", D_METHOD("validate_credentials", "email", "password"), &ZexAuth::validate_credentials);

	ClassDB::bind_method(D_METHOD("configure", "url", "anon_key"), &ZexAuth::configure);
	ClassDB::bind_method(D_METHOD("load_config", "path"), &ZexAuth::load_config);
	ClassDB::bind_method(D_METHOD("is_configured"), &ZexAuth::is_configured);
	ClassDB::bind_method(D_METHOD("sign_up", "email", "password"), &ZexAuth::sign_up);
	ClassDB::bind_method(D_METHOD("sign_in", "email", "password"), &ZexAuth::sign_in);
	ClassDB::bind_method(D_METHOD("refresh_session"), &ZexAuth::refresh_session);
	ClassDB::bind_method(D_METHOD("sign_out"), &ZexAuth::sign_out);
	ClassDB::bind_method(D_METHOD("is_signed_in"), &ZexAuth::is_signed_in);
	ClassDB::bind_method(D_METHOD("is_session_expired"), &ZexAuth::is_session_expired);
	ClassDB::bind_method(D_METHOD("is_busy"), &ZexAuth::is_busy);
	ClassDB::bind_method(D_METHOD("get_access_token"), &ZexAuth::get_access_token);
	ClassDB::bind_method(D_METHOD("get_user_id"), &ZexAuth::get_user_id);
	ClassDB::bind_method(D_METHOD("get_user_email"), &ZexAuth::get_user_email);
	ClassDB::bind_method(D_METHOD("save_session"), &ZexAuth::save_session);
	ClassDB::bind_method(D_METHOD("restore_session"), &ZexAuth::restore_session);
	ClassDB::bind_method(D_METHOD("clear_saved_session"), &ZexAuth::clear_saved_session);

	ADD_SIGNAL(MethodInfo("auth_succeeded", PropertyInfo(Variant::STRING, "email")));
	ADD_SIGNAL(MethodInfo("auth_failed", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::INT, "http_code")));
	ADD_SIGNAL(MethodInfo("confirmation_required", PropertyInfo(Variant::STRING, "email")));
	ADD_SIGNAL(MethodInfo("signed_out"));

	BIND_ENUM_CONSTANT(OP_NONE);
	BIND_ENUM_CONSTANT(OP_SIGN_UP);
	BIND_ENUM_CONSTANT(OP_SIGN_IN);
	BIND_ENUM_CONSTANT(OP_REFRESH);
	BIND_ENUM_CONSTANT(OP_SIGN_OUT);
}
