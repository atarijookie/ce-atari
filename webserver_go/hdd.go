package main

import (
	"encoding/json"
	"log"
	"net/http"

	"github.com/go-chi/chi/v5"
)

// HDD-related handlers.

func (s *Server) handleGetHDDRaw(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetHDDRaw %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	s.mu.RLock()
	defer s.mu.RUnlock()
	devices, ok := s.hddRaw[mac]
	if !ok {
		http.Error(w, "unknown mac", http.StatusNotFound)
		return
	}
	writeJSON(w, http.StatusOK, devices)
}

func (s *Server) handlePutHDDRaw(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutHDDRaw %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	var payload []RawDevice
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}
	s.mu.Lock()
	s.hddRaw[mac] = payload
	s.mu.Unlock()
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
