package main

import (
	"log"
	"math/rand"
	"os"
	"time"

	"github.com/fumon/cttrgb"
	"github.com/tarm/goserial"
)

const nLEDs = 92

var steps = 1000
var timestep = 40 // milliseconds

func map7(in float64) uint8 {
	return uint8(in * (127.0 / 255.0))
}

func main() {
	// Seed random number generator
	rand.Seed(time.Now().UnixNano())

	// Open the serial port
	c := &serial.Config{Name: os.Args[1], Baud: 115200}
	ser, err := serial.OpenPort(c)
	if err != nil {
		log.Fatalln("Trouble opening serial: ", err)
	}

	buf := make([]byte, (nLEDs*3 + 1))
	buf[0] = 0x84

	// Color temperature
	ct := float64(500.00)

	// Time between updates
	tick := time.Tick(40 * time.Millisecond)
mainloop:
	for _ = range tick {
		r, g, b := cttrgb.Convert(ct)
		for i := 0; i < nLEDs; i++ {
			buf[1+i*3], buf[2+i*3], buf[3+i*3] = map7(g)|0x80, map7(r)|0x80, map7(b)|0x80
		}
		ser.Write(buf)
		if ct < 2400 {
			if ct < 2000 {
				ct += 0.25
			} else {
				ct += 0.75
			}
		} else {
			break mainloop
		}
	}
}
