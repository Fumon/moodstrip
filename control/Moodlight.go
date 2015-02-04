package main

import (
	"flag"
	"log"
	"math/rand"
	"net"
	"time"

	"code.google.com/p/sadbox/color"
	"github.com/nsf/termbox-go"
)

const nLEDs = 92
const directMagicByte = 0x84
const serverMagicByte = 0x0F
const bufferfill = 5
const pktlen = (nLEDs*3 + 1)

var steps = 1000
var timestep = 160 * time.Millisecond // milliseconds

var fadelmodes = 3

var directFlag = flag.Bool("direct", false, "When true, uses 0x84 magic bytes instead of 0x0F magic bytes to separate packets")

func map7(in uint8) uint8 {
	return uint8(float64(in) * (127.0 / 255.0))
}

func main() {
	flag.Parse()
	// Seed random number generator
	rand.Seed(time.Now().UnixNano())

	// Termbox
	err := termbox.Init()
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

	buf := make([]byte, (pktlen)*bufferfill)
	for j := 0; j < bufferfill; j++ {
		if *directFlag {
			buf[j*(pktlen)] = directMagicByte
		} else {
			buf[j*(pktlen)] = serverMagicByte
		}
	}

	var r, g, b uint8
	var h, s, l float64

	// Random starting Hue
	h = rand.Float64()

	// Default saturation
	s = 0.8
	// Switches fading saturation in and out
	fades := false
	// State variable for fading saturation
	pingpong := false
	// Coefficient of saturation fading
	scof := (0.8 / float64(steps))

	// Default Luminance
	l = 0.5
	// Switches fading luminance along the strip
	fadel := 1

	// Mutable state increment
	tau := 0

	// Time between updates
	tick := time.Tick(timestep)

	conn, err := net.Dial("tcp", "bedroom_leds:9996")
	if err != nil {
		log.Fatalln("Couldn't open localhost: ", err)
	}
	defer conn.Close()

mainloop:
	for {
		select {
		case c := <-evchan:
			switch c.Type {
			case termbox.EventKey:
				switch c.Key {
				case termbox.KeyEsc:
					break mainloop
				case termbox.KeyArrowUp:
					h += 0.01
					if h > 1.0 {
						h = 0.0
					}
				case termbox.KeyArrowDown:
					h -= 0.01
					if h < 0.0 {
						h = 1.0
					}
				case termbox.KeyArrowRight:
					l += 0.01
					if l > 1.0 {
						l = 0.0
					}
				case termbox.KeyArrowLeft:
					l -= 0.01
					if l < 0.0 {
						l = 1.0
					}
				case termbox.KeyCtrlS: // Switch fades on and off
					fades = !fades
				case termbox.KeyCtrlL:
					fadel++
					if fadel >= fadelmodes {
						fadel = 0
					}

					if fadel == 0 {
						l = 0.5
					}
				default:
					switch c.Ch {
					case 'S':
						s += 0.01
						if s > 1.0 {
							s = 0.0
						}
					case 's':
						s -= 0.01
						if s < 0.0 {
							s = 1.0
						}
					}
				}
			}
		default:
		}

		for j := 0; j < bufferfill; j++ {
			if fades {
				if pingpong {
					s = 0.3 + scof*float64(steps-(tau%steps))
				} else {
					s = 0.3 + scof*float64(tau%steps)
				}
			}

			for i := 0; i < nLEDs; i++ {
				switch fadel {
				case 0:
				case 1:
					l = (0.008 / float64(nLEDs) * float64((i+tau)%nLEDs*3))
				case 2:
					l = (0.008 / float64(nLEDs) * float64((i+tau)%nLEDs*1))
				}
				r, g, b = color.HSLToRGB(h, s, l)
				buf[((pktlen)*j)+(1+i*3)], buf[((pktlen)*j)+(2+i*3)], buf[((pktlen)*j)+(3+i*3)] = map7(g)|0x80, map7(r)|0x80, map7(b)|0x80
			}
			tau = (tau + 1)
			if tau%steps == 0 {
				pingpong = !pingpong
			}
		}
		conn.Write(buf[:pktlen*bufferfill])

		<-tick
	}
}
