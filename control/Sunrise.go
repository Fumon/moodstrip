package main

import (
	"fmt"
	"log"
	"math/rand"
	"os"
	"time"

	"github.com/fumon/cttrgb"
	"github.com/nsf/termbox-go"
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

	// Termbox
	err = termbox.Init()
	if err != nil {
		log.Fatalln("Trouble with termbox init: ", err)
	}
	defer termbox.Close()

	evchan := make(chan termbox.Event, 10)

	go func() {
		evtick := time.Tick(100 * time.Millisecond)
		for {
			ev := termbox.PollEvent()
			select {
			case <-evtick:
				evchan <- ev
			default:

			}
		}
	}()

	buf := make([]byte, (nLEDs*3 + 1))
	buf[0] = 0x84

	// Color temperature
	ct := float64(500.00)

	// Time between updates
	tick := time.Tick(40 * time.Millisecond)
mainloop:
	for _ = range tick {
		select {
		case c := <-evchan:
			switch c.Type {
			case termbox.EventKey:
				switch c.Key {
				case termbox.KeyEsc:
					break mainloop
				case termbox.KeyArrowUp:
					ct += 10.0
				case termbox.KeyArrowDown:
					ct -= 10.0
				case termbox.KeyArrowRight:
				case termbox.KeyArrowLeft:
				}
			}
		default:
		}
		for i := 0; i < nLEDs; i++ {
			r, g, b := cttrgb.Convert(ct)
			buf[1+i*3], buf[2+i*3], buf[3+i*3] = map7(g)|0x80, map7(r)|0x80, map7(b)|0x80
		}
		termbox.Flush()
		termbox.SetCursor(20, 20)
		fmt.Println(ct)
		if ct < 2400 {
			if ct < 2000 {
				ct += 0.25
			} else {
				ct += 0.75
			}
		}
		ser.Write(buf)
	}
}
