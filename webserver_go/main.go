package main

import (
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/go-chi/chi/v5"
)

type (
	LoginRequest struct {
		Username string `json:"username"`
		Password string `json:"password"`
	}

	LoginResponse struct {
		Token string `json:"token"`
	}

	Device struct {
		MAC       string `json:"mac"`
		Name      string `json:"name"`
		Connected bool   `json:"connected"`
	}

	RawDevice struct {
		ID   string `json:"id"`
		Path string `json:"path"`
	}

	TranslatedMapping struct {
		Drive string `json:"drive"`
		Path  string `json:"path"`
	}

	Status struct {
		MAC        string    `json:"mac"`
		Online     bool      `json:"online"`
		HDDEnabled bool      `json:"hddEnabled"`
		FDEnabled  bool      `json:"fdEnabled"`
		UpdatedAt  time.Time `json:"updatedAt"`
	}

	DirEntry struct {
		Name  string `json:"name"`
		IsDir bool   `json:"isDir"`
	}

	HostDevice struct {
		ID          string `json:"id"`
		Description string `json:"description"`
	}
)

type Server struct {
	router        *chi.Mux
	mu            sync.RWMutex
	authTokens    map[string]string
	devices       []Device
	hddRaw        map[string][]RawDevice
	hddTranslated map[string][]TranslatedMapping
	fddImage      map[string]string
	status        map[string]Status
	hostDevices   []HostDevice
}

func main() {
	loadEnvFromDotFile(".env")

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

	s := newServer()
	log.Printf("starting server on %s", addr)
	if err := http.ListenAndServe(addr, s.router); err != nil {
		log.Fatalf("server error: %v", err)
	}
}

func newServer() *Server {
	r := chi.NewRouter()
	s := &Server{
		router:     r,
		authTokens: map[string]string{},
		devices: []Device{
			{MAC: "aa:bb:cc:dd:ee:01", Name: "Living room", Connected: true},
			{MAC: "aa:bb:cc:dd:ee:02", Name: "Office", Connected: false},
		},
		hddRaw: map[string][]RawDevice{
			"aa:bb:cc:dd:ee:01": {
				{ID: "dev1", Path: "/mnt/hdd1"},
				{ID: "dev2", Path: "/mnt/hdd2"},
			},
		},
		hddTranslated: map[string][]TranslatedMapping{
			"aa:bb:cc:dd:ee:01": {
				{Drive: "C", Path: "/mnt/hdd1/system"},
				{Drive: "D", Path: "/mnt/hdd2/data"},
			},
		},
		fddImage: map[string]string{
			"aa:bb:cc:dd:ee:01": "/images/os.img",
		},
		status: map[string]Status{
			"aa:bb:cc:dd:ee:01": {
				MAC:        "aa:bb:cc:dd:ee:01",
				Online:     true,
				HDDEnabled: true,
				FDEnabled:  true,
				UpdatedAt:  time.Now(),
			},
		},
		hostDevices: []HostDevice{
			{ID: "disk0", Description: "Internal SSD"},
			{ID: "usb1", Description: "USB Flash Drive"},
		},
	}

	r.Post("/auth/login", s.handleLogin)
	r.Group(func(protected chi.Router) {
		protected.Use(s.authMiddleware)
		protected.Post("/auth/logout", s.handleLogout)
		protected.Get("/devices", s.handleListDevices)
		protected.Get("/hdd/{mac}/raw", s.handleGetHDDRaw)
		protected.Put("/hdd/{mac}/raw", s.handlePutHDDRaw)
		protected.Get("/hdd/{mac}/translated", s.handleGetHDDTranslated)
		protected.Put("/hdd/{mac}/translated", s.handlePutHDDTranslated)
		protected.Get("/fdd/{mac}/image", s.handleGetFDDImage)
		protected.Put("/fdd/{mac}/image", s.handlePutFDDImage)
		protected.Get("/status/{mac}", s.handleGetStatus)
		protected.Get("/host/dir", s.handleHostDir)
		protected.Get("/host/devices", s.handleHostDevices)
	})

	// Serve static files (HTML, JS, images, etc.) from the local "static" directory.
	// This directory lives next to the Go sources / binary working directory.
	fileServer := http.FileServer(http.Dir("static"))
	r.Handle("/*", fileServer)

	return s
}

func (s *Server) authMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		token := parseBearerToken(r.Header.Get("Authorization"))
		if token == "" || !s.tokenValid(token) {
			http.Error(w, "unauthorized", http.StatusUnauthorized)
			return
		}
		next.ServeHTTP(w, r)
	})
}

func parseBearerToken(header string) string {
	if !strings.HasPrefix(strings.ToLower(header), "bearer ") {
		return ""
	}
	return strings.TrimSpace(header[7:])
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

// rotatingFileWriter is an io.Writer that writes to a single log file and
// rotates it when the size exceeds maxSize bytes.
type rotatingFileWriter struct {
	mu       sync.Mutex
	dir      string
	baseName string
	file     *os.File
	size     int64
	maxSize  int64
}

func newRotatingFileWriter(dir, baseName string, maxSize int64) (*rotatingFileWriter, error) {
	w := &rotatingFileWriter{
		dir:      dir,
		baseName: baseName,
		maxSize:  maxSize,
	}
	if err := w.openCurrent(); err != nil {
		return nil, err
	}
	return w, nil
}

func (w *rotatingFileWriter) Write(p []byte) (int, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.file == nil {
		if err := w.openCurrent(); err != nil {
			return 0, err
		}
	}

	// Rotate if this write would exceed maxSize.
	if w.size+int64(len(p)) > w.maxSize {
		if err := w.rotate(); err != nil {
			return 0, err
		}
	}

	n, err := w.file.Write(p)
	w.size += int64(n)
	return n, err
}

func (w *rotatingFileWriter) openCurrent() error {
	path := filepath.Join(w.dir, w.baseName)
	f, err := os.OpenFile(path, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0o644)
	if err != nil {
		return err
	}
	info, err := f.Stat()
	if err != nil {
		_ = f.Close()
		return err
	}
	w.file = f
	w.size = info.Size()
	return nil
}

func (w *rotatingFileWriter) rotate() error {
	if w.file != nil {
		_ = w.file.Close()
	}

	currentPath := filepath.Join(w.dir, w.baseName)
	rotatedPath := currentPath + ".1"

	// Keep only two files: base and base.1
	// Remove previous .1 (if any), then move current -> .1
	_ = os.Remove(rotatedPath)
	_ = os.Rename(currentPath, rotatedPath)

	// Open a fresh current file.
	f, err := os.OpenFile(currentPath, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0o644)
	if err != nil {
		return err
	}
	w.file = f
	w.size = 0
	return nil
}

// loadEnvFromDotFile loads simple KEY=VALUE lines from the given .env file
// in the current working directory. It silently ignores missing files
// and malformed lines.
func loadEnvFromDotFile(path string) {
	data, err := os.ReadFile(path)
	if err != nil {
		// No .env file; nothing to do.
		return
	}

	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if idx := strings.Index(line, "="); idx != -1 {
			key := strings.TrimSpace(line[:idx])
			val := strings.TrimSpace(line[idx+1:])

			// Optionally strip surrounding quotes.
			if len(val) >= 2 && ((val[0] == '"' && val[len(val)-1] == '"') || (val[0] == '\'' && val[len(val)-1] == '\'')) {
				val = val[1 : len(val)-1]
			}

			if key != "" {
				_ = os.Setenv(key, val)
			}
		}
	}
}
