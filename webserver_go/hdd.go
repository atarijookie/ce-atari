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
	
	// Send reload command to core
	cmd := map[string]string{"module": "disks", "action": "reload_raw"}
	if cmdJSON, err := json.Marshal(cmd); err == nil {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePutHDDRaw - failed to send cmd socket: %v", err)
		}
	}
	
	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handleGetHDDTranslated(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetHDDTranslated %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	
	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	
	// Build path to device directory: SETTINGS_DIR/{mac}/
	deviceDir := filepath.Join(settingsDir, mac)
	
	// Read DRIVELETTER_CONFDRIVE to find which drive is the config drive
	confDriveFile := filepath.Join(deviceDir, "DRIVELETTER_CONFDRIVE")
	var configDriveLetter string
	if content, err := os.ReadFile(confDriveFile); err == nil {
		configDriveLetter = strings.TrimSpace(string(content))
	}
	
	// Initialize arrays for paths and drive_types (16 elements, indices 0-15)
	// Drive letters C-P correspond to indices 2-15
	paths := make([]string, 16)
	driveTypes := make([]int, 16)
	
	// Read files for drive letters C-P (indices 2-15)
	for i := 2; i < 16; i++ {
		driveLetter := string(rune('A' + i)) // C=2, D=3, ..., P=15
		pathGemFile := filepath.Join(deviceDir, "PATH_GEM_"+driveLetter)
		
		// Read PATH_GEM_{letter}
		if content, err := os.ReadFile(pathGemFile); err == nil {
			pathContent := strings.TrimSpace(string(content))
			paths[i] = pathContent
			
			// Determine drive type:
			// 0 = empty/off
			// 1 = normal drive with path
			// 2 = config drive
			if driveLetter == configDriveLetter {
				driveTypes[i] = 2 // Config drive
			} else if pathContent != "" {
				driveTypes[i] = 1 // Normal drive
			} else {
				driveTypes[i] = 0 // Off/empty
			}
		} else {
			// File doesn't exist, check if it's the config drive
			if driveLetter == configDriveLetter {
				driveTypes[i] = 2 // Config drive even if path file doesn't exist
			} else {
				driveTypes[i] = 0 // Off/empty
			}
		}
	}
	
	// Return JSON response
	response := map[string]interface{}{
		"paths":      paths,
		"drive_types": driveTypes,
	}
	
	// Log JSON data before sending
	if jsonData, err := json.Marshal(response); err == nil {
		log.Printf("handleGetHDDTranslated - sending JSON: %s", string(jsonData))
	} else {
		log.Printf("handleGetHDDTranslated - error marshaling JSON: %v", err)
	}
	
	writeJSON(w, http.StatusOK, response)
}

func (s *Server) handlePutHDDTranslated(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutHDDTranslated %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	
	// Read request body to log it
	var bodyBytes []byte
	if r.Body != nil {
		var err error
		bodyBytes, err = io.ReadAll(r.Body)
		if err != nil {
			log.Printf("handlePutHDDTranslated - error reading request body: %v", err)
			http.Error(w, "invalid request body", http.StatusBadRequest)
			return
		}
		// Log received JSON data
		log.Printf("handlePutHDDTranslated - received JSON: %s", string(bodyBytes))
		// Create a new reader from the bytes for decoding
		r.Body = io.NopCloser(strings.NewReader(string(bodyBytes)))
	}
	
	// Parse JSON payload with paths and drive_types arrays
	var payload struct {
		Paths     []string `json:"paths"`
		DriveTypes []int    `json:"drive_types"`
	}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}
	
	// Validate arrays have correct length (16 elements)
	if len(payload.Paths) != 16 || len(payload.DriveTypes) != 16 {
		http.Error(w, "paths and drive_types must each have 16 elements", http.StatusBadRequest)
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
		log.Printf("handlePutHDDTranslated - error creating directory %s: %v", deviceDir, err)
		http.Error(w, "cannot create device directory", http.StatusInternalServerError)
		return
	}
	
	var configDriveLetter string
	
	// Process drive letters C-P (indices 2-15)
	for i := 2; i < 16; i++ {
		driveLetter := string(rune('A' + i)) // C=2, D=3, ..., P=15
		pathGemFile := filepath.Join(deviceDir, "PATH_GEM_"+driveLetter)
		
		driveType := payload.DriveTypes[i]
		path := payload.Paths[i]
		
		switch driveType {
		case 0:
			// Drive type 0: off - make path empty (delete file or write empty)
			if err := os.WriteFile(pathGemFile, []byte(""), 0o644); err != nil {
				log.Printf("handlePutHDDTranslated - error writing file %s: %v", pathGemFile, err)
				http.Error(w, "cannot write path file", http.StatusInternalServerError)
				return
			}
		case 1:
			// Drive type 1: normal drive - save path to PATH_GEM_{letter}
			if err := os.WriteFile(pathGemFile, []byte(path), 0o644); err != nil {
				log.Printf("handlePutHDDTranslated - error writing file %s: %v", pathGemFile, err)
				http.Error(w, "cannot write path file", http.StatusInternalServerError)
				return
			}
		case 2:
			// Drive type 2: config drive - save drive letter to DRIVELETTER_CONFDRIVE
			configDriveLetter = driveLetter
			// Also save the path if provided
			if path != "" {
				if err := os.WriteFile(pathGemFile, []byte(path), 0o644); err != nil {
					log.Printf("handlePutHDDTranslated - error writing file %s: %v", pathGemFile, err)
					http.Error(w, "cannot write path file", http.StatusInternalServerError)
					return
				}
			}
		}
	}
	
	// Write DRIVELETTER_CONFDRIVE file if a config drive was set
	confDriveFile := filepath.Join(deviceDir, "DRIVELETTER_CONFDRIVE")
	if configDriveLetter != "" {
		if err := os.WriteFile(confDriveFile, []byte(configDriveLetter), 0o644); err != nil {
			log.Printf("handlePutHDDTranslated - error writing file %s: %v", confDriveFile, err)
			http.Error(w, "cannot write config drive file", http.StatusInternalServerError)
			return
		}
	} else {
		// If no config drive, delete or write empty file
		if err := os.WriteFile(confDriveFile, []byte(""), 0o644); err != nil {
			log.Printf("handlePutHDDTranslated - error writing file %s: %v", confDriveFile, err)
			http.Error(w, "cannot write config drive file", http.StatusInternalServerError)
			return
		}
	}
	
	// Send reload command to core
	cmd := map[string]string{"module": "disks", "action": "reload_trans"}
	if cmdJSON, err := json.Marshal(cmd); err == nil {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePutHDDTranslated - failed to send cmd socket: %v", err)
		}
	}
	
	w.WriteHeader(http.StatusNoContent)
}
