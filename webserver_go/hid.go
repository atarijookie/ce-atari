package main

import (
	"encoding/json"
	"log"
	"net/http"
)

// HID-related handlers.

func (s *Server) handlePostHIDMouse(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePostHIDMouse %s %s", r.Method, r.URL.Path)

	// Parse JSON payload
	var payload map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	// Create base command
	cmd := map[string]interface{}{
		"module": "ikbd",
		"action": "mouse",
	}

	// Extend base command with payload content
	for key, value := range payload {
		cmd[key] = value
	}

	// Send to command socket (log error but don't fail request)
	cmdJSON, err := json.Marshal(cmd)
	if err != nil {
		log.Printf("handlePostHIDMouse - error marshaling command: %v", err)
	} else {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePostHIDMouse - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handlePostHIDKeyboard(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePostHIDKeyboard %s %s", r.Method, r.URL.Path)

	// Parse JSON payload
	var payload map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	// Create base command
	cmd := map[string]interface{}{
		"module": "ikbd",
		"action": "keyboard",
	}

	// Extend base command with payload content
	for key, value := range payload {
		cmd[key] = value
	}

	// Send to command socket (log error but don't fail request)
	cmdJSON, err := json.Marshal(cmd)
	if err != nil {
		log.Printf("handlePostHIDKeyboard - error marshaling command: %v", err)
	} else {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePostHIDKeyboard - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}
