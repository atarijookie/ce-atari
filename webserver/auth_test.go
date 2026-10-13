package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"
)

// useTempSettingsDir points SETTINGS_DIR to an empty temp dir, so no password is stored yet.
func useTempSettingsDir(t *testing.T) {
	t.Helper()
	t.Setenv("SETTINGS_DIR", t.TempDir())
}

func doLogin(s *Server, password string) *http.Response {
	data, _ := json.Marshal(LoginRequest{Password: password})
	req := httptest.NewRequest(http.MethodPost, "/auth/login", bytes.NewReader(data))
	w := httptest.NewRecorder()
	s.handleLogin(w, req)
	return w.Result()
}

func doSetPassword(s *Server, token, oldPassword, newPassword string) *http.Response {
	data, _ := json.Marshal(SetPasswordRequest{OldPassword: oldPassword, NewPassword: newPassword})
	req := httptest.NewRequest(http.MethodPost, "/auth/password", bytes.NewReader(data))
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	w := httptest.NewRecorder()
	s.handleSetPassword(w, req)
	return w.Result()
}

func TestHandleLoginDefaultPassword(t *testing.T) {
	useTempSettingsDir(t)
	s := &Server{authTokens: make(map[string]struct{})}

	res := doLogin(s, defaultPassword)
	if res.StatusCode != http.StatusOK {
		t.Fatalf("expected status %d, got %d", http.StatusOK, res.StatusCode)
	}

	var resp LoginResponse
	if err := json.NewDecoder(res.Body).Decode(&resp); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if resp.Token == "" {
		t.Fatalf("expected non-empty token")
	}
	if !resp.MustSetPassword {
		t.Fatalf("expected mustSetPassword while no password is stored")
	}

	// Token should be stored in authTokens.
	if _, ok := s.authTokens[resp.Token]; !ok {
		t.Fatalf("expected token to be stored in authTokens")
	}
}

func TestHandleLoginWrongPassword(t *testing.T) {
	useTempSettingsDir(t)
	s := &Server{authTokens: make(map[string]struct{})}

	res := doLogin(s, "bad")
	if res.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected status %d, got %d", http.StatusUnauthorized, res.StatusCode)
	}
}

func TestSetPasswordFlow(t *testing.T) {
	useTempSettingsDir(t)
	s := &Server{authTokens: map[string]struct{}{"tok123": {}, "other": {}}}

	// wrong old password is rejected
	if res := doSetPassword(s, "tok123", "bad", "newpass"); res.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected status %d for wrong old password, got %d", http.StatusUnauthorized, res.StatusCode)
	}

	// empty new password is rejected
	if res := doSetPassword(s, "tok123", defaultPassword, ""); res.StatusCode != http.StatusBadRequest {
		t.Fatalf("expected status %d for empty new password, got %d", http.StatusBadRequest, res.StatusCode)
	}

	// default -> newpass
	if res := doSetPassword(s, "tok123", defaultPassword, "newpass"); res.StatusCode != http.StatusNoContent {
		t.Fatalf("expected status %d, got %d", http.StatusNoContent, res.StatusCode)
	}

	// stored as bcrypt hash, not plain text
	content, err := os.ReadFile(passwordHashFilePath())
	if err != nil {
		t.Fatalf("expected password hash file: %v", err)
	}
	if strings.Contains(string(content), "newpass") || !strings.HasPrefix(string(content), "$2") {
		t.Fatalf("expected bcrypt hash in file, got %q", content)
	}

	// other sessions are logged out, the current one stays
	if _, ok := s.authTokens["other"]; ok {
		t.Fatalf("expected other sessions to be removed")
	}
	if _, ok := s.authTokens["tok123"]; !ok {
		t.Fatalf("expected current session to stay")
	}

	// default password no longer works, new one does and doesn't require setting a password
	if res := doLogin(s, defaultPassword); res.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected default password to be rejected, got %d", res.StatusCode)
	}
	res := doLogin(s, "newpass")
	if res.StatusCode != http.StatusOK {
		t.Fatalf("expected new password to work, got %d", res.StatusCode)
	}
	var resp LoginResponse
	if err := json.NewDecoder(res.Body).Decode(&resp); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if resp.MustSetPassword {
		t.Fatalf("expected mustSetPassword=false once password is stored")
	}

	// changing again needs the new password as the old one
	if res := doSetPassword(s, "tok123", defaultPassword, "x"); res.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected default password to be rejected as old password, got %d", res.StatusCode)
	}
	if res := doSetPassword(s, "tok123", "newpass", "x"); res.StatusCode != http.StatusNoContent {
		t.Fatalf("expected password change to work, got %d", res.StatusCode)
	}
}

func TestMiddlewareRedirectsToSetPasswordWhileDefault(t *testing.T) {
	useTempSettingsDir(t)
	s := newServer()
	s.authTokens["tok123"] = struct{}{}

	get := func(path string) *http.Response {
		req := httptest.NewRequest(http.MethodGet, path, nil)
		req.Header.Set("Authorization", "Bearer tok123")
		w := httptest.NewRecorder()
		s.router.ServeHTTP(w, req)
		return w.Result()
	}

	// no password stored -> everything goes to the set password page
	res := get("/devices")
	if res.StatusCode != http.StatusFound || res.Header.Get("Location") != setPasswordPage {
		t.Fatalf("expected redirect to %s, got %d %q", setPasswordPage, res.StatusCode, res.Header.Get("Location"))
	}

	// once a password is stored, access is allowed
	if err := storePassword("newpass"); err != nil {
		t.Fatalf("storePassword: %v", err)
	}
	res = get("/devices")
	if res.StatusCode == http.StatusFound {
		t.Fatalf("expected no redirect once password is set, got redirect to %q", res.Header.Get("Location"))
	}
}

func TestHandleLogoutSuccess(t *testing.T) {
	s := &Server{
		authTokens: map[string]struct{}{
			"tok123": {},
		},
	}

	req := httptest.NewRequest(http.MethodPost, "/auth/logout", nil)
	req.Header.Set("Authorization", "Bearer tok123")
	w := httptest.NewRecorder()

	s.handleLogout(w, req)

	res := w.Result()
	if res.StatusCode != http.StatusNoContent {
		t.Fatalf("expected status %d, got %d", http.StatusNoContent, res.StatusCode)
	}

	if _, ok := s.authTokens["tok123"]; ok {
		t.Fatalf("expected token to be removed from authTokens")
	}
}

func TestHandleLogoutMissingToken(t *testing.T) {
	s := &Server{
		authTokens: make(map[string]struct{}),
	}

	req := httptest.NewRequest(http.MethodPost, "/auth/logout", nil)
	w := httptest.NewRecorder()

	s.handleLogout(w, req)

	res := w.Result()
	if res.StatusCode != http.StatusBadRequest {
		t.Fatalf("expected status %d, got %d", http.StatusBadRequest, res.StatusCode)
	}
}

func TestPing(t *testing.T) {
	useTempSettingsDir(t)
	s := newServer()
	s.authTokens["tok123"] = struct{}{}

	ping := func(token string) int {
		req := httptest.NewRequest(http.MethodGet, "/ping", nil)
		if token != "" {
			req.Header.Set("Authorization", "Bearer "+token)
		}
		w := httptest.NewRecorder()
		s.router.ServeHTTP(w, req)
		return w.Result().StatusCode
	}

	if got := ping("tok123"); got != http.StatusNoContent {
		t.Fatalf("expected %d with valid session, got %d", http.StatusNoContent, got)
	}
	if got := ping("unknown"); got != http.StatusUnauthorized {
		t.Fatalf("expected %d with unknown session, got %d", http.StatusUnauthorized, got)
	}
	if got := ping(""); got != http.StatusUnauthorized {
		t.Fatalf("expected %d without session, got %d", http.StatusUnauthorized, got)
	}
}
