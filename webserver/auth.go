package main

import (
	"encoding/json"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"golang.org/x/crypto/bcrypt"
)

// Auth-related handlers.

// defaultPassword is accepted while no password hash is stored in SETTINGS_DIR.
const defaultPassword = "ce"

// passwordHashFileName is the file in SETTINGS_DIR holding the bcrypt hash of the management password.
const passwordHashFileName = "web_password_hash"

// passwordHashFilePath returns the path to the stored password hash file.
func passwordHashFilePath() string {
	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	return filepath.Join(settingsDir, passwordHashFileName)
}

// loadPasswordHash returns the stored password hash, or nil if no password has been set yet.
func loadPasswordHash() []byte {
	content, err := os.ReadFile(passwordHashFilePath())
	if err != nil {
		return nil
	}
	hash := []byte(strings.TrimSpace(string(content)))
	if len(hash) == 0 {
		return nil
	}
	return hash
}

// passwordIsSet reports whether a password hash is stored, i.e. the default password is no longer used.
func passwordIsSet() bool {
	return loadPasswordHash() != nil
}

// checkPassword verifies password against the stored hash, or against the default password if none is stored.
func checkPassword(password string) bool {
	hash := loadPasswordHash()
	if hash == nil {
		return password == defaultPassword
	}
	return bcrypt.CompareHashAndPassword(hash, []byte(password)) == nil
}

// storePassword hashes password and writes it to SETTINGS_DIR (via temp file + rename, so it's never half-written).
func storePassword(password string) error {
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return err
	}

	path := passwordHashFilePath()
	tmpPath := path + ".tmp"
	if err := os.WriteFile(tmpPath, append(hash, '\n'), 0o600); err != nil {
		return err
	}
	return os.Rename(tmpPath, path)
}

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
		req.Password = r.FormValue("password")
	} else {
		// Parse JSON
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			http.Error(w, "invalid request body", http.StatusBadRequest)
			log.Printf("handleLogin - invalid request body - json")
			return
		}
	}

	if !checkPassword(req.Password) {
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
	s.authTokens[token] = struct{}{}
	s.mu.Unlock()

	// Always set cookie for browser-based logins (both form and JSON)
	// This allows JavaScript redirects to work properly
	http.SetCookie(w, &http.Cookie{
		Name:     "auth_token",
		Value:    token,
		Path:     "/",
		HttpOnly: true,
		Secure:   true,
		SameSite: http.SameSiteLaxMode,
	})

	// still using the default password? the user must set a new one first
	mustSetPassword := !passwordIsSet()

	// For form submissions, redirect to index.html (or to set password page)
	if strings.HasPrefix(contentType, "application/x-www-form-urlencoded") {
		tokenPreview := token
		if len(token) > 10 {
			tokenPreview = token[:10]
		}
		target := "/index.html"
		if mustSetPassword {
			target = setPasswordPage
		}
		log.Printf("handleLogin - OK, token stored: %s..., redirecting to %s", tokenPreview, target)
		http.Redirect(w, r, target, http.StatusFound)
		return
	}

	// For JSON requests, return JSON response (cookie is already set)
	log.Printf("handleLogin - OK and writeJSON")
	writeJSON(w, http.StatusOK, LoginResponse{Token: token, MustSetPassword: mustSetPassword})
}

func (s *Server) handleLogout(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleLogout %s %s", r.Method, r.URL.Path)

	token := requestToken(r)
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
		Secure:   true,
		SameSite: http.SameSiteLaxMode,
		MaxAge:   -1,
	})

	w.WriteHeader(http.StatusNoContent)
}

// handleSetPassword changes the management password; requires the current (or default) password.
func (s *Server) handleSetPassword(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleSetPassword %s %s", r.Method, r.URL.Path)

	var req SetPasswordRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		log.Printf("handleSetPassword - invalid request body")
		return
	}

	if !checkPassword(req.OldPassword) {
		http.Error(w, "invalid old password", http.StatusUnauthorized)
		log.Printf("handleSetPassword - invalid old password")
		return
	}

	if req.NewPassword == "" {
		http.Error(w, "new password must not be empty", http.StatusBadRequest)
		log.Printf("handleSetPassword - empty new password")
		return
	}

	if err := storePassword(req.NewPassword); err != nil {
		http.Error(w, "could not store password", http.StatusInternalServerError)
		log.Printf("handleSetPassword - could not store password: %v", err)
		return
	}

	// log out all other sessions, keep the one which changed the password
	currentToken := requestToken(r)
	s.mu.Lock()
	for token := range s.authTokens {
		if token != currentToken {
			delete(s.authTokens, token)
		}
	}
	s.mu.Unlock()

	log.Printf("handleSetPassword - password changed")
	w.WriteHeader(http.StatusNoContent)
}

// requestToken returns the auth token from the Authorization header or from the auth_token cookie.
func requestToken(r *http.Request) string {
	token := parseBearerToken(r.Header.Get("Authorization"))
	if token == "" {
		if cookie, err := r.Cookie("auth_token"); err == nil {
			token = cookie.Value
		}
	}
	return token
}
