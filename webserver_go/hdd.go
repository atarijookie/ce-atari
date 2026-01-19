package main

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/go-chi/chi/v5"
)

// HDD-related handlers.

func (s *Server) handleGetHDDRaw(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetHDDRaw %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	
	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	
	// Build path to device directory: SETTINGS_DIR/{mac}/
	deviceDir := filepath.Join(settingsDir, mac)
	
	// Initialize arrays for paths and dev_types
	paths := make([]string, 8)
	devTypes := make([]int, 8)
	
	// Read files for devices 0-7
	for i := 0; i < 8; i++ {
		// Read PATH_RAW_{i}
		pathRawFile := filepath.Join(deviceDir, "PATH_RAW_"+strconv.Itoa(i))
		if content, err := os.ReadFile(pathRawFile); err == nil {
			paths[i] = strings.TrimSpace(string(content))
		} else {
			paths[i] = "" // Default to empty string if file doesn't exist
		}
		
		// Read ACSI_DEVTYPE_{i}
		devTypeFile := filepath.Join(deviceDir, "ACSI_DEVTYPE_"+strconv.Itoa(i))
		if content, err := os.ReadFile(devTypeFile); err == nil {
			if val, err := strconv.Atoi(strings.TrimSpace(string(content))); err == nil {
				devTypes[i] = val
			} else {
				devTypes[i] = 0 // Default to 0 if parsing fails
			}
		} else {
			devTypes[i] = 0 // Default to 0 if file doesn't exist
		}
	}
	
	// Return JSON response
	response := map[string]interface{}{
		"paths":    paths,
		"dev_types": devTypes,
	}
	
	// Log JSON data before sending
	if jsonData, err := json.Marshal(response); err == nil {
		log.Printf("handleGetHDDRaw - sending JSON: %s", string(jsonData))
	} else {
		log.Printf("handleGetHDDRaw - error marshaling JSON: %v", err)
	}
	
	writeJSON(w, http.StatusOK, response)
}

func (s *Server) handlePutHDDRaw(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutHDDRaw %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	
	// Read request body to log it
	var bodyBytes []byte
	if r.Body != nil {
		var err error
		bodyBytes, err = io.ReadAll(r.Body)
		if err != nil {
			log.Printf("handlePutHDDRaw - error reading request body: %v", err)
			http.Error(w, "invalid request body", http.StatusBadRequest)
			return
		}
		// Log received JSON data
		log.Printf("handlePutHDDRaw - received JSON: %s", string(bodyBytes))
		// Create a new reader from the bytes for decoding
		r.Body = io.NopCloser(strings.NewReader(string(bodyBytes)))
	}
	
	// Parse JSON payload with paths and dev_types arrays
	var payload struct {
		Paths    []string `json:"paths"`
		DevTypes []int    `json:"dev_types"`
	}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}
	
	// Validate arrays have correct length (8 elements)
	if len(payload.Paths) != 8 || len(payload.DevTypes) != 8 {
		http.Error(w, "paths and dev_types must each have 8 elements", http.StatusBadRequest)
		return
	}
	
	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	
	// Build path to device directory: SETTINGS_DIR/{mac}/
	deviceDir := filepath.Join(settingsDir, mac)
	
	// Create device directory if it doesn't exist
	if err := os.MkdirAll(deviceDir, 0o755); err != nil {
		log.Printf("handlePutHDDRaw - error creating directory %s: %v", deviceDir, err)
		http.Error(w, "cannot create device directory", http.StatusInternalServerError)
		return
	}
	
	// Write files for devices 0-7
	for i := 0; i < 8; i++ {
		// Write PATH_RAW_{i}
		pathRawFile := filepath.Join(deviceDir, "PATH_RAW_"+strconv.Itoa(i))
		if err := os.WriteFile(pathRawFile, []byte(payload.Paths[i]), 0o644); err != nil {
			log.Printf("handlePutHDDRaw - error writing file %s: %v", pathRawFile, err)
			http.Error(w, "cannot write path file", http.StatusInternalServerError)
			return
		}
		
		// Write ACSI_DEVTYPE_{i}
		devTypeFile := filepath.Join(deviceDir, "ACSI_DEVTYPE_"+strconv.Itoa(i))
		devTypeStr := strconv.Itoa(payload.DevTypes[i])
		if err := os.WriteFile(devTypeFile, []byte(devTypeStr), 0o644); err != nil {
			log.Printf("handlePutHDDRaw - error writing file %s: %v", devTypeFile, err)
			http.Error(w, "cannot write dev_type file", http.StatusInternalServerError)
			return
		}
	}
	
	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handleGetHDDTranslated(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetHDDTranslated %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	s.mu.RLock()
	defer s.mu.RUnlock()
	mappings, ok := s.hddTranslated[mac]
	if !ok {
		http.Error(w, "unknown mac", http.StatusNotFound)
		return
	}
	writeJSON(w, http.StatusOK, mappings)
}

func (s *Server) handlePutHDDTranslated(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutHDDTranslated %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	var payload []TranslatedMapping
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}
	s.mu.Lock()
	s.hddTranslated[mac] = payload
	s.mu.Unlock()
	w.WriteHeader(http.StatusNoContent)
}
