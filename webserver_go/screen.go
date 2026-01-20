package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/go-chi/chi/v5"
	"golang.org/x/sys/unix"
)

// Screen-related handlers.

func (s *Server) handleGetScreen(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetScreen %s %s", r.Method, r.URL.Path)

	// data_size = 1 + 32 + 32000 (resolution + palette + screen data)
	dataSize := 1 + 32 + 32000
	fd := -1

	// Get shared memory name from environment
	memName := os.Getenv("SCREENCAST_MEMORY_NAME")
	if memName == "" {
		log.Printf("handleGetScreen - SCREENCAST_MEMORY_NAME not set")
		http.Error(w, "shared memory access failed", http.StatusBadRequest)
		return
	}

	// Open shared memory file using shm_open equivalent
	// On Linux, shm_open creates files in /dev/shm/
	shmPath := "/dev/shm/" + memName
	f, err := os.OpenFile(shmPath, os.O_RDONLY, 0)
	if err != nil {
		log.Printf("handleGetScreen - failed to open shared memory %s: %v", shmPath, err)
		http.Error(w, "shared memory access failed", http.StatusBadRequest)
		return
	}
	defer f.Close()
	fd = int(f.Fd())

	// Map the shared memory for reading
	data, err := unix.Mmap(fd, 0, dataSize, unix.PROT_READ, unix.MAP_SHARED)
	if err != nil {
		log.Printf("handleGetScreen - failed to mmap shared memory: %v", err)
		http.Error(w, "shared memory access failed", http.StatusBadRequest)
		return
	}
	defer unix.Munmap(data)

	// Set response headers
	w.Header().Set("Content-Type", "application/binary")
	w.Header().Set("Cache-Control", "no-cache, no-store")
	w.Header().Set("Pragma", "no-cache")
	w.Header().Set("Content-Length", fmt.Sprintf("%d", dataSize))

	// Write the data
	w.WriteHeader(http.StatusOK)
	_, err = w.Write(data)
	if err != nil {
		log.Printf("handleGetScreen - failed to write response: %v", err)
	}
}

func (s *Server) handlePostScreenScreenshot(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePostScreenScreenshot %s %s", r.Method, r.URL.Path)

	// Send screenshot command to core (log error but don't fail the request)
	cmd := map[string]string{"module": "screencast", "action": "do_screenshot"}
	cmdJSON, err := json.Marshal(cmd)
	if err != nil {
		log.Printf("handlePostScreenScreenshot - error marshaling command: %v", err)
	} else {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePostScreenScreenshot - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handlePostScreenVBL(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePostScreenVBL %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	macWithoutColons := stripMACColons(mac)

	// Parse JSON payload to get boolean value
	var payload struct {
		Value bool `json:"value"`
	}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := getSettingsDir()

	// Build path to VBL settings file: SETTINGS_DIR/{mac}/vbl
	deviceDir := filepath.Join(settingsDir, macWithoutColons)
	vblFile := filepath.Join(deviceDir, "vbl")

	// Create device directory if it doesn't exist
	if err := os.MkdirAll(deviceDir, 0o755); err != nil {
		log.Printf("handlePostScreenVBL - error creating device directory %s: %v", deviceDir, err)
		http.Error(w, "cannot create device directory", http.StatusInternalServerError)
		return
	}

	// Convert boolean to 0 or 1
	vblValue := "0"
	if payload.Value {
		vblValue = "1"
	}

	// Write VBL value to file (0 or 1)
	if err := os.WriteFile(vblFile, []byte(vblValue), 0o644); err != nil {
		log.Printf("handlePostScreenVBL - error writing file %s: %v", vblFile, err)
		http.Error(w, "cannot write vbl", http.StatusInternalServerError)
		return
	}

	// Send VBL command to core (log error but don't fail the request)
	vblInt := 0
	if payload.Value {
		vblInt = 1
	}
	cmd := map[string]interface{}{"module": "screencast", "action": "vbl", "value": vblInt}
	cmdJSON, err := json.Marshal(cmd)
	if err != nil {
		log.Printf("handlePostScreenVBL - error marshaling command: %v", err)
	} else {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePostScreenVBL - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handleGetScreenVBL(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetScreenVBL %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	macWithoutColons := stripMACColons(mac)

	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := getSettingsDir()

	// Build path to VBL settings file: SETTINGS_DIR/{mac}/vbl
	deviceDir := filepath.Join(settingsDir, macWithoutColons)
	vblFile := filepath.Join(deviceDir, "vbl")

	// Read VBL settings from file if it exists
	content, err := os.ReadFile(vblFile)
	if err != nil {
		// File doesn't exist, return false
		response := map[string]bool{
			"vbl": false,
		}
		writeJSON(w, http.StatusOK, response)
		return
	}

	// Parse 0 or 1 and convert to boolean
	vblStr := strings.TrimSpace(string(content))
	vblBool := false
	if vblStr == "1" {
		vblBool = true
	} else if vblStr == "0" {
		vblBool = false
	} else {
		// If file contains something else, try to parse as integer
		if val, err := strconv.Atoi(vblStr); err == nil && val != 0 {
			vblBool = true
		}
	}

	// Return VBL as boolean
	response := map[string]bool{
		"vbl": vblBool,
	}
	writeJSON(w, http.StatusOK, response)
}
