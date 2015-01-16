package main

import (
	"log"
	"math/rand"
	"os"
	"os/signal"
	"time"

	"code.google.com/p/sadbox/color"
	"github.com/tarm/goserial"
)

const nLEDs = 92

var steps = 1000

func map7(in uint8) uint8 {
	return uint8(float64(in) * (127.0 / 255.0))
}

func main() {
	// Open the serial port
	c := &serial.Config{Name: os.Args[1], Baud: 115200}
	ser, err := serial.OpenPort(c)
	if err != nil {
		log.Fatalln("Trouble opening serial: ", err)
	}

	killchan := make(chan os.Signal, 10)
	signal.Notify(killchan, os.Interrupt, os.Kill)

	buf := make([]byte, (nLEDs*3 + 1))
	buf[0] = 0x84

	rand.Seed(time.Now().UnixNano())
	tick := time.Tick(70 * time.Millisecond)
	var r, g, b uint8
	var h float64
	tau := 0
	h = rand.Float64()
	pingpong := false
mainloop:
	for _ = range tick {
		select {
		case <-killchan:
			log.Println("Dying gracefully")
			break mainloop
		default:
		}
		s := (0.8 / float64(steps))
		if pingpong {
			s *= float64(steps - (tau % steps))
		} else {
			s *= float64(tau % steps)
		}
		for i := 0; i < nLEDs; i++ {
			v := (0.008 / float64(nLEDs) * float64((i+tau)%nLEDs*3))
			r, g, b = color.HSVToRGB(h, 0.3+s, v)
			buf[1+i*3], buf[2+i*3], buf[3+i*3] = map7(g)|0x80, map7(r)|0x80, map7(b)|0x80
		}
		ser.Write(buf)
		tau = (tau + 1)
		if tau%steps == 0 {
			pingpong = !pingpong
		}
	}
}
