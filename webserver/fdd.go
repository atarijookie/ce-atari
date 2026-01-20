package main

import (
	"encoding/json"
	"log"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"sort"
	"strings"

	"github.com/go-chi/chi/v5"
)

// FDD-related handlers.

func getSettingsDir() string {
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	return settingsDir
}

func (s *Server) readFddImage(mac string) (string, error) {
	imageFile := filepath.Join(getSettingsDir(), mac, "FLOPPY_IMAGE")
	content, err := os.ReadFile(imageFile)
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(content)), nil
}

func (s *Server) writeFddImage(mac, image string) error {
	deviceDir := filepath.Join(getSettingsDir(), mac)
	if err := os.MkdirAll(deviceDir, 0o755); err != nil {
		return err
	}
	imageFile := filepath.Join(deviceDir, "FLOPPY_IMAGE")
	return os.WriteFile(imageFile, []byte(image), 0o644)
}

func (s *Server) handleGetFDDImage(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetFDDImage %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")

	image, err := s.readFddImage(mac)
	if err != nil {
		image = ""
	}

	writeJSON(w, http.StatusOK, map[string]string{"mac": mac, "image": image})
}

func (s *Server) handlePutFDDImage(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutFDDImage %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	var body struct {
		Image string `json:"image"`
	}
	if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}
	if body.Image == "" {
		http.Error(w, "image is required", http.StatusBadRequest)
		return
	}

	if err := s.writeFddImage(mac, body.Image); err != nil {
		http.Error(w, "cannot write FLOPPY_IMAGE", http.StatusInternalServerError)
		return
	}

	// Send insert command to core
	cmd := map[string]string{"module": "floppy", "action": "insert", "mac": mac}
	if cmdJSON, err := json.Marshal(cmd); err == nil {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePutFDDImage - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handleGetFDDImageNext(w http.ResponseWriter, r *http.Request) {
	s.handleGetFDDImageStep(w, r, +1)
}

func (s *Server) handleGetFDDImagePrev(w http.ResponseWriter, r *http.Request) {
	s.handleGetFDDImageStep(w, r, -1)
}

func (s *Server) handleGetFDDImageStep(w http.ResponseWriter, r *http.Request, step int) {
	log.Printf("handleGetFDDImageStep %s %s step=%d", r.Method, r.URL.Path, step)

	mac := chi.URLParam(r, "mac")

	current, err := s.readFddImage(mac)
	if err != nil || strings.TrimSpace(current) == "" {
		http.Error(w, "current image not set", http.StatusBadRequest)
		return
	}

	// Stored paths are expected to be POSIX-like ("/home/..."), so use path.* for split/join.
	dir := path.Dir(current)
	base := path.Base(current)
	if dir == "." || base == "." || base == "/" || base == "" {
		http.Error(w, "invalid current image path", http.StatusBadRequest)
		return
	}

	entries, err := os.ReadDir(dir)
	if err != nil {
		http.Error(w, "cannot read image directory", http.StatusInternalServerError)
		return
	}

	names := make([]string, 0, len(entries))
	for _, e := range entries {
		if e.IsDir() {
			continue
		}
		names = append(names, e.Name())
	}
	if len(names) == 0 {
		http.Error(w, "no files in image directory", http.StatusBadRequest)
		return
	}
	sort.Strings(names)

	// Find current file index; if not found, treat as 0.
	idx := 0
	for i := 0; i < len(names); i++ {
		if names[i] == base {
			idx = i
			break
		}
	}

	nextIdx := (idx + step) % len(names)
	if nextIdx < 0 {
		nextIdx += len(names)
	}

	newImage := path.Join(dir, names[nextIdx])
	if err := s.writeFddImage(mac, newImage); err != nil {
		http.Error(w, "cannot write FLOPPY_IMAGE", http.StatusInternalServerError)
		return
	}

	// Send insert command to core
	cmd := map[string]string{"module": "floppy", "action": "insert", "mac": mac}
	if cmdJSON, err := json.Marshal(cmd); err == nil {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handleGetFDDImageStep - failed to send cmd socket: %v", err)
		}
	}

	writeJSON(w, http.StatusOK, map[string]string{"mac": mac, "image": newImage})
}

// handleFloppyAction sends a floppy action command to the core
func handleFloppyAction(action, mac string) error {
	cmd := map[string]string{"module": "floppy", "action": action, "mac": mac}
	cmdJSON, err := json.Marshal(cmd)
	if err != nil {
		return err
	}
	if err := sendToCmdSocket(string(cmdJSON)); err != nil {
		log.Printf("handleFloppyAction - failed to send cmd socket: %v", err)
		return err
	}
	return nil
}

func (s *Server) handleDeleteFDDImage(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleDeleteFDDImage %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")

	// Send eject command to core (log error but don't fail the request)
	if err := handleFloppyAction("eject", mac); err != nil {
		log.Printf("handleDeleteFDDImage - failed to send eject command: %v", err)
	}

	// Clear the floppy image setting file
	if err := s.writeFddImage(mac, ""); err != nil {
		log.Printf("handleDeleteFDDImage - failed to write empty FLOPPY_IMAGE: %v", err)
		http.Error(w, "cannot write FLOPPY_IMAGE", http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusNoContent)
}
