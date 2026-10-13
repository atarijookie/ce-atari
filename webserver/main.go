package main

import (
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/go-chi/chi/v5"
)

type (
	LoginRequest struct {
		Password string `json:"password"`
	}

	LoginResponse struct {
		Token           string `json:"token"`
		MustSetPassword bool   `json:"mustSetPassword"`
	}

	SetPasswordRequest struct {
		OldPassword string `json:"oldPassword"`
		NewPassword string `json:"newPassword"`
	}

	Device struct {
		MAC       string `json:"mac"`
		Name      string `json:"name"`
		Connected bool   `json:"connected"`
	}
)

type Server struct {
	router        *chi.Mux
	mu            sync.RWMutex
	authTokens    map[string]struct{}
}

// responseWriter wraps http.ResponseWriter to capture status codes
type responseWriter struct {
	http.ResponseWriter
	statusCode int
}

func (rw *responseWriter) WriteHeader(code int) {
	rw.statusCode = code
	rw.ResponseWriter.WriteHeader(code)
}

func main() {
	loadEnvFromDotFile(".env")

	// Check if another instance is running and write PID file
	if otherInstanceIsRunning() {
		log.Fatalf("Another instance of this application is already running")
	}

	// Set up cleanup of PID file on exit
	pidFilePath := getPIDFilePath()
	defer func() {
		if err := os.Remove(pidFilePath); err != nil && !os.IsNotExist(err) {
			log.Printf("Failed to remove PID file %s: %v", pidFilePath, err)
		}
	}()

	// Also handle signals to clean up PID file
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-sigChan
		if err := os.Remove(pidFilePath); err != nil && !os.IsNotExist(err) {
			log.Printf("Failed to remove PID file %s: %v", pidFilePath, err)
		}
		os.Exit(0)
	}()

	// Configure logging to file with optional LOG_DIR override and rotation.
	logDir := os.Getenv("LOG_DIR")
	if logDir == "" {
		logDir = "/tmp/ce/log"
	}
	if err := os.MkdirAll(logDir, 0o755); err != nil {
		log.Printf("failed to create log directory %q: %v", logDir, err)
	} else {
		w, err := newRotatingFileWriter(logDir, "webserver.log", 1*1024*1024)
		if err != nil {
			log.Printf("failed to initialize log file writer: %v", err)
		} else {
			log.SetOutput(w)
		}
	}
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

	port := os.Getenv("WEBSERVER_PORT")
	if port == "" {
		port = "8080"
	}
	addr := ":" + port

	// Get local IP address for URL
	localIP := getLocalIP()
	url := "https://" + localIP + ":" + port
	if localIP == "127.0.0.1" || localIP == "localhost" {
		url = "https://localhost:" + port
	}

	// self-signed certificates for HTTPS, kept in SETTINGS_DIR/tls
	certs, err := newCertManager()
	if err != nil {
		log.Fatalf("TLS certificate setup failed: %v", err)
	}

	s := newServer()
	s.router.Get("/ca.crt", certs.handleCACert) // public, so the CA can be installed before logging in
	fmt.Println("Starting server on", addr)
	fmt.Println("Navigate to:", url)

	// Start goroutine to print status every 10 seconds
	go func() {
		ticker := time.NewTicker(10 * time.Second)
		defer ticker.Stop()
		for range ticker.C {
			fmt.Println("Server running on", addr, "-", url)
		}
	}()

	if err := listenAndServeHTTPS(addr, s.router, certs); err != nil {
		log.Fatalf("server error: %v", err)
	}
}

// getLocalIP returns the first non-loopback IP address found, or "127.0.0.1" if none found
func getLocalIP() string {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return "127.0.0.1"
	}
	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
			if ipnet.IP.To4() != nil {
				return ipnet.IP.String()
			}
		}
	}
	return "127.0.0.1"
}

func newServer() *Server {
	r := chi.NewRouter()
	s := &Server{
		router: r,
		authTokens: map[string]struct{}{},
	}

	// browsers must revalidate pages instead of using cached copies (304 if unchanged), otherwise
	// e.g. a login page cached from plain http:// would keep posting to http:// after switching to https://
	r.Use(func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Cache-Control", "no-cache")
			next.ServeHTTP(w, r)
		})
	})

	r.Post("/auth/login", s.handleLogin)

	// Public, so pages can check whether the server responds (also on the login page).
	r.Get("/ping", s.handlePing)
	r.Get("/js/server_check.js", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "static/js/server_check.js")
	})

	// Serve login.html publicly so users can access the login page without authentication.
	r.Get("/login.html", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "static/login.html")
	})

	// Redirect root and /login to login page.
	r.Get("/", func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, "/login.html", http.StatusFound)
	})
	r.Get("/login", func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, "/login.html", http.StatusFound)
	})

	r.Group(func(protected chi.Router) {
		// Log all requests first
		protected.Use(func(next http.Handler) http.Handler {
			return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				log.Printf("request - %s %s", r.Method, r.URL.Path)
				next.ServeHTTP(w, r)
			})
		})
		protected.Use(s.authMiddleware)
		protected.Post("/auth/logout", s.handleLogout)
		protected.Post("/auth/password", s.handleSetPassword)
		protected.Get("/devices", s.handleListDevices)
		protected.Get("/hdd/{mac}/raw", s.handleGetHDDRaw)
		protected.Put("/hdd/{mac}/raw", s.handlePutHDDRaw)
		protected.Get("/hdd/{mac}/translated", s.handleGetHDDTranslated)
		protected.Put("/hdd/{mac}/translated", s.handlePutHDDTranslated)
		protected.Get("/fdd/{mac}/image", s.handleGetFDDImage)
		protected.Put("/fdd/{mac}/image", s.handlePutFDDImage)
		protected.Delete("/fdd/{mac}/image", s.handleDeleteFDDImage)
		protected.Get("/fdd/{mac}/image/next", s.handleGetFDDImageNext)
		protected.Get("/fdd/{mac}/image/prev", s.handleGetFDDImagePrev)
		protected.Get("/ikbd/{mac}", s.handleGetIKBD)
		protected.Put("/ikbd/{mac}", s.handlePutIKBD)
		protected.Post("/hid/mouse", s.handlePostHIDMouse)
		protected.Post("/hid/keyboard", s.handlePostHIDKeyboard)
		protected.Get("/status/{mac}", s.handleGetStatus)
		protected.Get("/device/{mac}/name", s.handleGetDeviceName)
		protected.Put("/device/{mac}/name", s.handlePutDeviceName)
		protected.Get("/device/{mac}/features", s.handleGetDeviceFeatures)
		protected.Get("/screen/{mac}", s.handleGetScreen)
		protected.Post("/screen/{mac}/screenshot", s.handlePostScreenScreenshot)
		protected.Post("/screen/{mac}/vbl", s.handlePostScreenVBL)
		protected.Get("/screen/{mac}/vbl", s.handleGetScreenVBL)
		protected.Get("/host/dir", s.handleHostDir)

		// Serve static files (HTML, JS, images, etc.) from the local "static" directory.
		// This directory lives next to the Go sources / binary working directory.
		// Static files require authentication.
		// Use Handle with /* to catch all remaining routes
		protected.Handle("/*", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			// Get the file path from the URL
			filePath := "static" + r.URL.Path
			log.Printf("fileServer - request path: %s, file path: %s", r.URL.Path, filePath)

			// Check if file exists
			info, err := os.Stat(filePath)
			if os.IsNotExist(err) {
				// Silently ignore favicon.ico requests if file doesn't exist
				if r.URL.Path == "/favicon.ico" {
					http.NotFound(w, r)
					return
				}
				log.Printf("fileServer - file not found: %s (requested: %s)", filePath, r.URL.Path)
				http.NotFound(w, r)
				return
			}

			// If it's a directory, don't serve it
			if info.IsDir() {
				http.NotFound(w, r)
				return
			}

			log.Printf("fileServer - serving %s", r.URL.Path)
			// Open and serve the file directly to avoid any redirects
			file, err := os.Open(filePath)
			if err != nil {
				log.Printf("fileServer - error opening file: %v", err)
				http.Error(w, "Internal server error", http.StatusInternalServerError)
				return
			}
			defer file.Close()

			// Set content type based on file extension
			if strings.HasSuffix(filePath, ".html") {
				w.Header().Set("Content-Type", "text/html; charset=utf-8")
			} else if strings.HasSuffix(filePath, ".css") {
				w.Header().Set("Content-Type", "text/css")
			} else if strings.HasSuffix(filePath, ".js") {
				w.Header().Set("Content-Type", "application/javascript")
			} else if strings.HasSuffix(filePath, ".png") {
				w.Header().Set("Content-Type", "image/png")
			} else if strings.HasSuffix(filePath, ".jpg") || strings.HasSuffix(filePath, ".jpeg") {
				w.Header().Set("Content-Type", "image/jpeg")
			} else if strings.HasSuffix(filePath, ".gif") {
				w.Header().Set("Content-Type", "image/gif")
			}

			http.ServeContent(w, r, info.Name(), info.ModTime(), file)
			log.Printf("fileServer - served %s successfully", r.URL.Path)
		}))
	})

	return s
}

func (s *Server) authMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		token := parseBearerToken(r.Header.Get("Authorization"))
		if token == "" {
			// Try to get token from cookie
			if cookie, err := r.Cookie("auth_token"); err == nil {
				token = cookie.Value
				log.Printf("authMiddleware - found token in cookie for path %s", r.URL.Path)
			} else {
				log.Printf("authMiddleware - no cookie found (err: %v) for path %s", err, r.URL.Path)
			}
		} else {
			log.Printf("authMiddleware - found token in Authorization header for path %s", r.URL.Path)
		}
		if token == "" || !s.tokenValid(token) {
			log.Printf("authMiddleware - token invalid or missing (token empty: %v, valid: %v) for path %s", token == "", token != "" && s.tokenValid(token), r.URL.Path)
			// Redirect to login page instead of returning 401.
			http.Redirect(w, r, "/login.html", http.StatusFound)
			return
		}
		// while the default password is used, only allow setting a new password (and logout)
		if !passwordAllowedPath(r.URL.Path) && !passwordIsSet() {
			log.Printf("authMiddleware - no password set yet, redirecting %s to %s", r.URL.Path, setPasswordPage)
			http.Redirect(w, r, setPasswordPage, http.StatusFound)
			return
		}
		log.Printf("authMiddleware - token valid, allowing access to %s", r.URL.Path)
		next.ServeHTTP(w, r)
	})
}

// setPasswordPage is where the user is sent while no password is stored (default password in use).
const setPasswordPage = "/set_password.html"

// passwordAllowedPath reports whether path is reachable while the default password is still in use.
func passwordAllowedPath(path string) bool {
	switch path {
	case setPasswordPage, "/auth/password", "/auth/logout":
		return true
	}
	// assets needed to render the set password page
	return strings.HasPrefix(path, "/css/") || strings.HasPrefix(path, "/js/") || strings.HasPrefix(path, "/img/")
}

func parseBearerToken(header string) string {
	if !strings.HasPrefix(strings.ToLower(header), "bearer ") {
		return ""
	}
	return strings.TrimSpace(header[7:])
}

// handlePing tells the pages that the server responds: 204 with a valid session, 401 without one
// (sessions are kept in memory, so they are gone after a server restart).
func (s *Server) handlePing(w http.ResponseWriter, r *http.Request) {
	token := requestToken(r)
	if token == "" || !s.tokenValid(token) {
		w.WriteHeader(http.StatusUnauthorized)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) tokenValid(token string) bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	_, ok := s.authTokens[token]
	return ok
}

func writeJSON(w http.ResponseWriter, status int, payload interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	if err := json.NewEncoder(w).Encode(payload); err != nil {
		log.Printf("failed to write response: %v", err)
	}
}

func generateToken() (string, error) {
	var buf [32]byte
	if _, err := rand.Read(buf[:]); err != nil {
		return "", err
	}
	return base64.StdEncoding.EncodeToString(buf[:]), nil
}
