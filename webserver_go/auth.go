package main

import (
	"encoding/json"
	"log"
	"net/http"

	"github.com/msteinert/pam"
)

// Auth-related handlers.

// pamAuthFunc is a package-level variable so tests can stub out PAM calls.
var pamAuthFunc = verifyWithPAM

func (s *Server) handleLogin(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleLogin %s %s", r.Method, r.URL.Path)

	var req LoginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	if err := pamAuthFunc(req.Username, req.Password); err != nil {
		// Treat any PAM error as invalid credentials to avoid leaking details.
		http.Error(w, "invalid credentials", http.StatusUnauthorized)
		return
	}

	token, err := generateToken()
	if err != nil {
		http.Error(w, "could not create session", http.StatusInternalServerError)
		return
	}

	s.mu.Lock()
	s.authTokens[token] = req.Username
	s.mu.Unlock()

	writeJSON(w, http.StatusOK, LoginResponse{Token: token})
}

func (s *Server) handleLogout(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleLogout %s %s", r.Method, r.URL.Path)

	token := parseBearerToken(r.Header.Get("Authorization"))
	if token == "" {
		http.Error(w, "missing token", http.StatusBadRequest)
		return
	}

	s.mu.Lock()
	delete(s.authTokens, token)
	s.mu.Unlock()

	w.WriteHeader(http.StatusNoContent)
}

// verifyWithPAM authenticates against the host's PAM stack using the "login" service.
func verifyWithPAM(username, password string) error {
	t, err := pam.StartFunc("login", username, func(s pam.Style, msg string) (string, error) {
		switch s {
		case pam.PromptEchoOff:
			return password, nil
		case pam.PromptEchoOn:
			return "", nil
		case pam.ErrorMsg, pam.TextInfo:
			return "", nil
		default:
			// Ignore other prompt types.
			return "", nil
		}
	})
	if err != nil {
		return err
	}
	return t.Authenticate(0)
}
