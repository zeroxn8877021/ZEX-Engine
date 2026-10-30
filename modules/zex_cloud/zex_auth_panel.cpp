/**************************************************************************/
/*  zex_auth_panel.cpp                                                    */
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

#include "zex_auth_panel.h"

#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/scene_string_names.h"

static const float ZEX_TOUCH_HEIGHT = 56.0f;

ZexAuthPanel::ZexAuthPanel() {
	Label *title = memnew(Label);
	title->set_text("ZEX INTERACTIVE Account");
	add_child(title);

	email_edit = memnew(LineEdit);
	email_edit->set_placeholder("Email");
	email_edit->set_custom_minimum_size(Size2(0, ZEX_TOUCH_HEIGHT));
	add_child(email_edit);

	password_edit = memnew(LineEdit);
	password_edit->set_placeholder("Password (8+ characters)");
	password_edit->set_secret(true);
	password_edit->set_custom_minimum_size(Size2(0, ZEX_TOUCH_HEIGHT));
	add_child(password_edit);

	login_button = memnew(Button);
	login_button->set_text("Log in");
	login_button->set_custom_minimum_size(Size2(0, ZEX_TOUCH_HEIGHT));
	login_button->connect(SceneStringName(pressed), callable_mp(this, &ZexAuthPanel::_on_login_pressed));
	add_child(login_button);

	signup_button = memnew(Button);
	signup_button->set_text("Create account");
	signup_button->set_custom_minimum_size(Size2(0, ZEX_TOUCH_HEIGHT));
	signup_button->connect(SceneStringName(pressed), callable_mp(this, &ZexAuthPanel::_on_signup_pressed));
	add_child(signup_button);

	logout_button = memnew(Button);
	logout_button->set_text("Log out");
	logout_button->set_custom_minimum_size(Size2(0, ZEX_TOUCH_HEIGHT));
	logout_button->connect(SceneStringName(pressed), callable_mp(this, &ZexAuthPanel::_on_logout_pressed));
	add_child(logout_button);

	status_label = memnew(Label);
	add_child(status_label);

	auth = memnew(ZexAuth);
	add_child(auth);
	auth->connect("auth_succeeded", callable_mp(this, &ZexAuthPanel::_on_auth_succeeded));
	auth->connect("auth_failed", callable_mp(this, &ZexAuthPanel::_on_auth_failed));
	auth->connect("confirmation_required", callable_mp(this, &ZexAuthPanel::_on_confirmation_required));
	auth->connect("signed_out", callable_mp(this, &ZexAuthPanel::_on_signed_out));

	_refresh_ui();
}

void ZexAuthPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		// Look for the (public) cloud config; the engine keeps working offline without it.
		if (auth->load_config("user://zex_cloud.cfg") != OK) {
			auth->load_config("res://zex_cloud.cfg");
		}
		// Resume a saved session if there is one; works offline because nothing is sent.
		if (auth->restore_session() == OK) {
			_set_status("Signed in as " + auth->get_user_email());
		} else if (!auth->is_configured()) {
			_set_status("Offline mode. Cloud account is not configured.");
		}
		_refresh_ui();
	}
}

void ZexAuthPanel::_set_status(const String &p_text) {
	status_label->set_text(p_text);
}

void ZexAuthPanel::_refresh_ui() {
	bool signed_in = auth->is_signed_in();
	bool busy = auth->is_busy();
	email_edit->set_editable(!signed_in && !busy);
	password_edit->set_editable(!signed_in && !busy);
	login_button->set_disabled(signed_in || busy);
	signup_button->set_disabled(signed_in || busy);
	logout_button->set_disabled(!signed_in || busy);
}

void ZexAuthPanel::_on_login_pressed() {
	if (auth->sign_in(email_edit->get_text(), password_edit->get_text()) == OK) {
		_set_status("Logging in...");
	}
	_refresh_ui();
}

void ZexAuthPanel::_on_signup_pressed() {
	if (auth->sign_up(email_edit->get_text(), password_edit->get_text()) == OK) {
		_set_status("Creating account...");
	}
	_refresh_ui();
}

void ZexAuthPanel::_on_logout_pressed() {
	auth->sign_out();
	_refresh_ui();
}

void ZexAuthPanel::_on_auth_succeeded(const String &p_email) {
	password_edit->clear();
	_set_status("Signed in as " + p_email);
	_refresh_ui();
}

void ZexAuthPanel::_on_auth_failed(const String &p_message, int p_code) {
	_set_status(p_message);
	_refresh_ui();
}

void ZexAuthPanel::_on_confirmation_required(const String &p_email) {
	password_edit->clear();
	_set_status("Account created. Check " + p_email + " to confirm your email, then log in.");
	_refresh_ui();
}

void ZexAuthPanel::_on_signed_out() {
	_set_status("Signed out.");
	_refresh_ui();
}

void ZexAuthPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_auth"), &ZexAuthPanel::get_auth);
}
