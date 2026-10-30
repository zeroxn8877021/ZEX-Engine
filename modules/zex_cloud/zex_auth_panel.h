/**************************************************************************/
/*  zex_auth_panel.h                                                      */
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

#include "zex_auth.h"

#include "scene/gui/box_container.h"

class Button;
class Label;
class LineEdit;

// ZEX ACCOUNT - touch-friendly login / sign-up panel that drives a ZexAuth node.
class ZexAuthPanel : public VBoxContainer {
	GDCLASS(ZexAuthPanel, VBoxContainer);

	ZexAuth *auth = nullptr;
	LineEdit *email_edit = nullptr;
	LineEdit *password_edit = nullptr;
	Button *login_button = nullptr;
	Button *signup_button = nullptr;
	Button *logout_button = nullptr;
	Label *status_label = nullptr;

	void _set_status(const String &p_text);
	void _refresh_ui();
	void _on_login_pressed();
	void _on_signup_pressed();
	void _on_logout_pressed();
	void _on_auth_succeeded(const String &p_email);
	void _on_auth_failed(const String &p_message, int p_code);
	void _on_confirmation_required(const String &p_email);
	void _on_signed_out();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	ZexAuth *get_auth() const { return auth; }

	ZexAuthPanel();
};
