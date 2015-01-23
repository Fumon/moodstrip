package main

import (
	"log"
	"math/rand"
	"os"
	"time"

	"github.com/tarm/goserial"
)

func main() {
	// Open the serial port
	c := &serial.Config{Name: os.Args[1], Baud: 115200}
	s, err := serial.OpenPort(c)
	if err != nil {
		log.Fatalln("Trouble opening serial: ", err)
	}

	buf := make([]byte, (92*3 + 1))
	buf[0] = 0x84

	rand.Seed(time.Now().UnixNano())
	tick := time.Tick(100 * time.Millisecond)
	for _ = range tick {
		for i := 0; i < 92*3; i++ {
			buf[i+1] = byte(rand.Int()%50) | 0x80
		}
		s.Write(buf)
	}
}
