package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestHandleLoginSuccess(t *testing.T) {
	// Stub PAM to always succeed and capture credentials.
	oldPamAuth := pamAuthFunc
	defer func() { pamAuthFunc = oldPamAuth }()

	var gotUser, gotPass string
	pamAuthFunc = func(user, pass string) error {
		gotUser, gotPass = user, pass
		return nil
	}

	s := &Server{
		authTokens: make(map[string]string),
	}

	body := LoginRequest{Username: "alice", Password: "secret"}
	data, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/login", bytes.NewReader(data))
	w := httptest.NewRecorder()

	s.handleLogin(w, req)

	res := w.Result()
	if res.StatusCode != http.StatusOK {
		t.Fatalf("expected status %d, got %d", http.StatusOK, res.StatusCode)
	}

	if gotUser != "alice" || gotPass != "secret" {
		t.Fatalf("pamAuthFunc called with wrong credentials: %q/%q", gotUser, gotPass)
	}

	var resp LoginResponse
	if err := json.NewDecoder(res.Body).Decode(&resp); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if resp.Token == "" {
		t.Fatalf("expected non-empty token")
	}

	// Token should be stored in authTokens.
	if _, ok := s.authTokens[resp.Token]; !ok {
		t.Fatalf("expected token to be stored in authTokens")
	}
}

func TestHandleLoginAuthFailure(t *testing.T) {
	// Stub PAM to fail.
	oldPamAuth := pamAuthFunc
	defer func() { pamAuthFunc = oldPamAuth }()
	pamAuthFunc = func(user, pass string) error {
		return assertError("auth failed")
	}

	s := &Server{
		authTokens: make(map[string]string),
	}

	body := LoginRequest{Username: "alice", Password: "bad"}
	data, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/login", bytes.NewReader(data))
	w := httptest.NewRecorder()

	s.handleLogin(w, req)

	res := w.Result()
	if res.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected status %d, got %d", http.StatusUnauthorized, res.StatusCode)
	}
}

func TestHandleLogoutSuccess(t *testing.T) {
	s := &Server{
		authTokens: map[string]string{
			"tok123": "alice",
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
		authTokens: make(map[string]string),
	}

	req := httptest.NewRequest(http.MethodPost, "/auth/logout", nil)
	w := httptest.NewRecorder()

	s.handleLogout(w, req)

	res := w.Result()
	if res.StatusCode != http.StatusBadRequest {
		t.Fatalf("expected status %d, got %d", http.StatusBadRequest, res.StatusCode)
	}
}

// assertError is a small helper type to satisfy the error interface in tests.
type assertError string

func (e assertError) Error() string { return string(e) }

