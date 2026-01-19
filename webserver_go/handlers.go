package main

import (
	"log"
	"net/http"
	"os"
	"strings"

	"github.com/go-chi/chi/v5"
)

// Misc / other handlers (devices, status, host).

func (s *Server) handleListDevices(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleListDevices %s %s", r.Method, r.URL.Path)

	connectedOnly := strings.EqualFold(r.URL.Query().Get("connected"), "true")
	s.mu.RLock()
	defer s.mu.RUnlock()

	if !connectedOnly {
		writeJSON(w, http.StatusOK, s.devices)
		return
	}

	filtered := make([]Device, 0, len(s.devices))
	for _, d := range s.devices {
		if d.Connected {
			filtered = append(filtered, d)
		}
	}
	writeJSON(w, http.StatusOK, filtered)
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
