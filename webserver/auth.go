package main

import (
	"encoding/json"
	"log"
	"net/http"
	"strings"

	"github.com/msteinert/pam"
)

// Auth-related handlers.

// pamAuthFunc is a package-level variable so tests can stub out PAM calls.
var pamAuthFunc = verifyWithPAM

func (s *Server) handleLogin(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleLogin %s %s", r.Method, r.URL.Path)

	var req LoginRequest

	// Check Content-Type to determine if it's form data or JSON
	contentType := r.Header.Get("Content-Type")
	if strings.HasPrefix(contentType, "application/x-www-form-urlencoded") {
		// Parse form data
		if err := r.ParseForm(); err != nil {
			http.Error(w, "invalid request body", http.StatusBadRequest)
			log.Printf("handleLogin - invalid request body - form")
			return
		}
		req.Username = r.FormValue("username")
		req.Password = r.FormValue("password")
	} else {
		// Parse JSON
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			http.Error(w, "invalid request body", http.StatusBadRequest)
			log.Printf("handleLogin - invalid request body - json")
			return
		}
	}

	if err := pamAuthFunc(req.Username, req.Password); err != nil {
		// Treat any PAM error as invalid credentials to avoid leaking details.
		http.Error(w, "invalid credentials", http.StatusUnauthorized)
		log.Printf("handleLogin - invalid credentials")
		return
	}

	token, err := generateToken()
	if err != nil {
		http.Error(w, "could not create session", http.StatusInternalServerError)
		log.Printf("handleLogin - could not create session")
		return
	}

	s.mu.Lock()
	s.authTokens[token] = req.Username
	s.mu.Unlock()

	// Always set cookie for browser-based logins (both form and JSON)
	// This allows JavaScript redirects to work properly
	http.SetCookie(w, &http.Cookie{
		Name:     "auth_token",
		Value:    token,
		Path:     "/",
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
	})

	// For form submissions, redirect to index.html
	if strings.HasPrefix(contentType, "application/x-www-form-urlencoded") {
		tokenPreview := token
		if len(token) > 10 {
			tokenPreview = token[:10]
		}
		log.Printf("handleLogin - OK, token stored: %s..., redirecting to index.html", tokenPreview)
		http.Redirect(w, r, "/index.html", http.StatusFound)
		return
	}

	// For JSON requests, return JSON response (cookie is already set)
	log.Printf("handleLogin - OK and writeJSON")
	writeJSON(w, http.StatusOK, LoginResponse{Token: token})
}

func (s *Server) handleLogout(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleLogout %s %s", r.Method, r.URL.Path)

	token := parseBearerToken(r.Header.Get("Authorization"))
	if token == "" {
		// Try to get token from cookie (for browser-based logout)
		if cookie, err := r.Cookie("auth_token"); err == nil {
			token = cookie.Value
		}
	}
	
	if token == "" {
		http.Error(w, "missing token", http.StatusBadRequest)
		return
	}

	// Delete token from server
	s.mu.Lock()
	delete(s.authTokens, token)
	s.mu.Unlock()

	// Clear the cookie
	http.SetCookie(w, &http.Cookie{
		Name:     "auth_token",
		Value:    "",
		Path:     "/",
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
		MaxAge:   -1,
	})

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
