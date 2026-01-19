package main

import (
	"log"
	"net/http"
	"os"
	"regexp"
	"strings"

	"github.com/go-chi/chi/v5"
)

// Misc / other handlers (devices, status, host).

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

// stripMACColons removes colons from MAC address "aa:bb:cc:dd:ee:01" -> "aabbccddee01"
func stripMACColons(mac string) string {
	return strings.ReplaceAll(mac, ":", "")
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
			devices = append(devices, Device{
				MAC:       macFormatted,
				Name:      folderName, // Use folder name as default name
				Connected: false,      // Default to not connected, can be updated later
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

func (s *Server) handleGetStatus(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetStatus %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	s.mu.RLock()
	defer s.mu.RUnlock()
	status, ok := s.status[mac]
	if !ok {
		http.Error(w, "unknown mac", http.StatusNotFound)
		return
	}
	writeJSON(w, http.StatusOK, status)
}

func (s *Server) handleHostDir(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleHostDir %s %s", r.Method, r.URL.Path)

	path := r.URL.Query().Get("path")
	if path == "" {
		path = "."
	}
	entries, err := os.ReadDir(path)
	if err != nil {
		http.Error(w, "cannot read directory", http.StatusBadRequest)
		return
	}
	response := make([]DirEntry, 0, len(entries))
	for _, e := range entries {
		response = append(response, DirEntry{
			Name:  e.Name(),
			IsDir: e.IsDir(),
		})
	}
	writeJSON(w, http.StatusOK, response)
}

func (s *Server) handleHostDevices(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleHostDevices %s %s", r.Method, r.URL.Path)

	s.mu.RLock()
	defer s.mu.RUnlock()
	writeJSON(w, http.StatusOK, s.hostDevices)
}
