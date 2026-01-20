package main

import (
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strings"

	"github.com/go-chi/chi/v5"
)

// Device-related handlers.

// isValidMACFormat checks if a string is a valid MAC address format (12 hex characters)
func isValidMACFormat(s string) bool {
	matched, _ := regexp.MatchString(`^[0-9a-fA-F]{12}$`, s)
	return matched
}

// formatMACWithColons converts "aabbccddee01" to "aa:bb:cc:dd:ee:01"
func formatMACWithColons(mac string) string {
	if len(mac) != 12 {
		return mac
	}
	return mac[0:2] + ":" + mac[2:4] + ":" + mac[4:6] + ":" + mac[6:8] + ":" + mac[8:10] + ":" + mac[10:12]
}

func (s *Server) handleListDevices(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleListDevices %s %s", r.Method, r.URL.Path)

	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}

	// Read directory entries
	entries, err := os.ReadDir(settingsDir)
	if err != nil {
		log.Printf("handleListDevices - error reading directory %s: %v", settingsDir, err)
		http.Error(w, "cannot read settings directory", http.StatusInternalServerError)
		return
	}

	// Filter directories that look like MAC addresses (12 hex characters)
	devices := make([]Device, 0)
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}

		folderName := entry.Name()
		if isValidMACFormat(folderName) {
			// Format MAC address with colons
			macFormatted := formatMACWithColons(folderName)
			
			// Read device_name file if it exists
			deviceNameFile := filepath.Join(settingsDir, folderName, "device_name")
			deviceName := folderName // Default to MAC without colons
			if content, err := os.ReadFile(deviceNameFile); err == nil {
				deviceName = strings.TrimSpace(string(content))
				// If file exists but is empty, use MAC without colons
				if deviceName == "" {
					deviceName = folderName
				}
			}
			
			devices = append(devices, Device{
				MAC:       macFormatted,
				Name:      deviceName,
				Connected: false, // Default to not connected, can be updated later
			})
		}
	}

	// Handle connected filter if requested
	connectedOnly := strings.EqualFold(r.URL.Query().Get("connected"), "true")
	if connectedOnly {
		filtered := make([]Device, 0)
		for _, d := range devices {
			if d.Connected {
				filtered = append(filtered, d)
			}
		}
		devices = filtered
	}

	writeJSON(w, http.StatusOK, devices)
}

func (s *Server) handleGetDeviceName(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetDeviceName %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	// Strip colons from MAC for file path (MAC in URL might have colons)
	macWithoutColons := stripMACColons(mac)
	
	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	
	// Build path to device_name file: SETTINGS_DIR/{mac}/device_name
	deviceDir := filepath.Join(settingsDir, macWithoutColons)
	deviceNameFile := filepath.Join(deviceDir, "device_name")
	
	// Read device name from file
	content, err := os.ReadFile(deviceNameFile)
	if err != nil {
		// If file doesn't exist, return mac without colons
		w.Header().Set("Content-Type", "text/plain")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(macWithoutColons))
		return
	}
	
	// Return the device name as plain text
	name := strings.TrimSpace(string(content))
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte(name))
}

func (s *Server) handlePutDeviceName(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutDeviceName %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	// Strip colons from MAC for file path (MAC in URL might have colons)
	macWithoutColons := stripMACColons(mac)
	
	// Read the device name from request body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}
	deviceName := strings.TrimSpace(string(body))
	
	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	
	// Build path to device_name file: SETTINGS_DIR/{mac}/device_name
	deviceDir := filepath.Join(settingsDir, macWithoutColons)
	deviceNameFile := filepath.Join(deviceDir, "device_name")
	
	// Create device directory if it doesn't exist
	if err := os.MkdirAll(deviceDir, 0o755); err != nil {
		log.Printf("handlePutDeviceName - error creating device directory %s: %v", deviceDir, err)
		http.Error(w, "cannot create device directory", http.StatusInternalServerError)
		return
	}
	
	// Write device name to file
	if err := os.WriteFile(deviceNameFile, []byte(deviceName), 0o644); err != nil {
		log.Printf("handlePutDeviceName - error writing file %s: %v", deviceNameFile, err)
		http.Error(w, "cannot write device_name", http.StatusInternalServerError)
		return
	}
	
	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handleGetDeviceFeatures(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetDeviceFeatures %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	// Strip colons from MAC for file path (MAC in URL might have colons)
	macWithoutColons := stripMACColons(mac)

	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}

	// Build path to features file: SETTINGS_DIR/{mac}/features
	deviceDir := filepath.Join(settingsDir, macWithoutColons)
	featuresFile := filepath.Join(deviceDir, "features")

	// Read features file
	content, err := os.ReadFile(featuresFile)
	if err != nil || len(strings.TrimSpace(string(content))) == 0 {
		// File doesn't exist or is empty - return all features as true
		response := map[string]bool{
			"acsi": true,
			"scsi": true,
			"fdd":  true,
			"ikbd": true,
		}
		writeJSON(w, http.StatusOK, response)
		return
	}

	// Parse content to check for feature letters (case-insensitive)
	contentStr := strings.ToUpper(strings.TrimSpace(string(content)))
	
	response := map[string]bool{
		"acsi": strings.Contains(contentStr, "A"),
		"scsi": strings.Contains(contentStr, "S"),
		"fdd":  strings.Contains(contentStr, "F"),
		"ikbd": strings.Contains(contentStr, "I"),
	}

	writeJSON(w, http.StatusOK, response)
}
