package main

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"strconv"
	"time"
)

const socketPath = "/tmp/prime_socket"
const bufferSize = 1024 // Reduced buffer size

type PrimeResponse struct {
    Primes []int  `json:"primes"`
    Time   string `json:"time"`
}

func handleError(w http.ResponseWriter, message string, code int, err error) {
    http.Error(w, message, code)
    if err != nil {
        log.Println("Error:", err)
    } else {
        log.Println("Error:", message)
    }
}

func handleRequest(w http.ResponseWriter, r *http.Request) {
    start := time.Now()

    limitStr := r.URL.Query().Get("limit")
    if limitStr == "" {
        handleError(w, "Missing 'limit' query parameter", http.StatusBadRequest, nil)
        return
    }

    limit, err := strconv.Atoi(limitStr)
    if err != nil {
        handleError(w, "Invalid 'limit' query parameter", http.StatusBadRequest, err)
        return
    }

    conn, err := net.Dial("unix", socketPath)
    if err != nil {
        handleError(w, "Failed to connect to Unix socket", http.StatusInternalServerError, err)
        return
    }
    defer conn.Close()

    request := fmt.Sprintf("%d", limit)
    log.Printf("Sending request to Unix socket: %s\n", request) // Log the request being sent
    _, err = conn.Write([]byte(request))
    if err != nil {
        handleError(w, "Failed to send request to Unix socket", http.StatusInternalServerError, err)
        return
    }

    // Read the file path from the Unix socket
    filePath := make([]byte, bufferSize)
    n, err := conn.Read(filePath)
    if err != nil {
        handleError(w, "Failed to read file path from Unix socket", http.StatusInternalServerError, err)
        return
    }
    filePathStr := string(filePath[:n])
    log.Printf("Received file path from Unix socket: %s\n", filePathStr)

    // Open and read the JSON response from the file
    file, err := os.Open(filePathStr)
    if err != nil {
        handleError(w, "Failed to open response file", http.StatusInternalServerError, err)
        return
    }
    defer file.Close()

    var response PrimeResponse
    decoder := json.NewDecoder(file)
    err = decoder.Decode(&response)
    if err != nil {
        if err == io.EOF {
            log.Println("Reached end of file while reading response")
        } else {
            handleError(w, "Failed to decode JSON response from file", http.StatusInternalServerError, err)
            return
        }
    }

    elapsed := time.Since(start)
    response.Time = elapsed.String()
    w.Header().Set("Content-Type", "application/json")
    err = json.NewEncoder(w).Encode(response)
    if err != nil {
        handleError(w, "Failed to encode response as JSON", http.StatusInternalServerError, err)
        return
    }

    log.Printf("Handled request: limit=%d, time=%s\n", limit, elapsed)
}

func primeListHandler(w http.ResponseWriter, r *http.Request) {
    log.Println("Received request for /prime_list")
    handleRequest(w, r)
}

func main() {
    http.HandleFunc("/prime_list", primeListHandler)

    log.Println("Starting server on :8080")
    if err := http.ListenAndServe(":8080", nil); err != nil {
        log.Println("Failed to start server:", err)
    }
}