package main

import (
	"fmt"
	"log"
	"net"
	"os"
)

// sendToCmdSocket sends the given message to the UNIX datagram socket defined by CMD_SOCK_PATH.
// The CMD_SOCK_PATH value is expected to be loaded from .env (with ${VAR} expansion handled by loadEnvFromDotFile).
func sendToCmdSocket(msg string) error {
	sockPath := os.Getenv("CMD_SOCK_PATH")
	if sockPath == "" {
		return fmt.Errorf("CMD_SOCK_PATH is not set")
	}

	conn, err := net.Dial("unixgram", sockPath)
	if err != nil {
		return fmt.Errorf("dial unixgram %q: %w", sockPath, err)
	}
	defer conn.Close()

	if _, err := conn.Write([]byte(msg)); err != nil {
		return fmt.Errorf("write to unixgram %q: %w", sockPath, err)
	}

	log.Printf("cmdsocket: sent to %s: %q", sockPath, msg)
	return nil
}
